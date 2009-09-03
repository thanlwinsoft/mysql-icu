/* Copyright (C) 2009 MySQL AB & Keith Stribley
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; version 2
   of the License.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include <my_global.h>
#include <m_string.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <my_alloc.h>

#include <unicode/utf.h>
#include <unicode/ucol.h>
#include <unicode/uiter.h>
#include <unicode/ucnv.h>
#include <unicode/usearch.h>

#undef VERSION
#undef PACKAGE
#include "config.h"

#ifdef HAVE_UNICODE_UCOL_H

#define ICU_STACK_BUFFER 1024

extern MY_CHARSET_HANDLER my_charset_utf8_handler;
extern MY_CHARSET_HANDLER my_charset_usc2_handler;

static my_bool my_coll_init_icu(CHARSET_INFO *cs, void *(*alloc)(size_t))
{
    UErrorCode status = U_ZERO_ERROR;
    UParseError rulesError;
    const char * actual = NULL;
    UChar * rules = NULL;
    size_t rulesLength = 0;
    UConverter * utf8Converter = NULL;

    if (strlen(cs->tailoring) < strlen(cs->name))
    {
        cs->sort_order = (uchar*)ucol_open(cs->tailoring, &status);

        if (U_SUCCESS(status))
        {
            ucol_setAttribute((UCollator*)cs->sort_order, UCOL_STRENGTH, UCOL_IDENTICAL, &status);
            actual = ucol_getLocaleByType((UCollator*)cs->sort_order, ULOC_ACTUAL_LOCALE, &status);
            DBUG_PRINT("icu",("ICU collator: %s",actual));

            /* create a converter for utf8 */
            if (strcmp(cs->csname, "utf8") == 0)
            {
                /* this isn't really the sort order, but it provides a pointer to the converter*/
                cs->contractions = (uint16*)ucnv_open((const char*)(cs->csname), &status);
                if (!cs->contractions || !U_SUCCESS(status))
                    return 1;
            }
            return 0;
        }
        return 1;
    }
    else
    {
        utf8Converter = ucnv_open((const char*)(cs->csname), &status);
        if (!U_SUCCESS(status)) return 1;

        rulesLength = strlen(cs->tailoring);
        rules = my_malloc(rulesLength * 2, MYF(MY_WME));

        rulesLength = ucnv_toUChars(utf8Converter, rules, rulesLength * 2, cs->tailoring, rulesLength, &status);
        if (!U_SUCCESS(status))
        {
            my_free(rules, MYF(MY_WME));
            return 1;
        }
        /* TODO tailored collation */
        cs->sort_order = (uchar*)ucol_openRules(rules, rulesLength, UCOL_DEFAULT, UCOL_DEFAULT_STRENGTH, &rulesError, &status);
        if (strcmp(cs->csname, "utf8") == 0)
        {
            cs->contractions = (uint16*)utf8Converter;
        }
        my_free(rules, MYF(MY_WME));
    }
    return 0;
}


static size_t my_strnxfrm_icu(CHARSET_INFO *cs,
                                  uchar *dst, size_t dstlen,
                                  const uchar *src, size_t srclen)
{
    size_t remDstLen = dstlen;
    uint32_t state[2];
    int32_t sortKeyLen = 0;
    UErrorCode status = U_ZERO_ERROR;
    struct UCharIterator iter;
    UCollator * icu_collator = (UCollator*)cs->sort_order;

    if (srclen == 0 || dstlen == 0)
        return dstlen;

    if (strcmp(cs->csname,"ucs2")==0) /* uscs2 */
    {
        uiter_setString(&iter, (UChar*)src, srclen/sizeof(UChar));
    }
    else if (strcmp(cs->csname,"utf8")==0)
    {
        uiter_setUTF8(&iter, (const char*)src, srclen);
    }
    else
    {
        DBUG_ASSERT(0);
        return 0; /* other types not supported yet */
    }
    state[0] = state[1] = 0;

    while (remDstLen)
    {
        sortKeyLen = ucol_nextSortKeyPart(icu_collator, &iter, state, dst, remDstLen, &status);
        if (!U_SUCCESS(status)) break;
        if (sortKeyLen <= 0)
            break;
        dst += sortKeyLen;
        DBUG_ASSERT(remDstLen >= sortKeyLen);
        remDstLen -= sortKeyLen;
    }
    bzero(dst, remDstLen);
    /* the return has to be dstlen */
    return dstlen;
}

static int my_strnncoll_icu(CHARSET_INFO *cs,
                                const uchar *s, size_t slen,
                                const uchar *t, size_t tlen,
                                my_bool t_is_prefix)
{
    UErrorCode status = U_ZERO_ERROR;
    UCharIterator sIter;
    UCharIterator tIter;
    int32_t sPrefixLen = 0;
    int32_t tPrefixLen = 0;
    UCollationResult result = UCOL_EQUAL;
    UChar32 dummy;
    UCollator * icu_collator = (UCollator*)cs->sort_order;
    /* handle case of an empty string */
    if (slen == 0)
    {
        if (tlen == 0) return 0;
        return -1;
    }
    else if (tlen == 0) return 1;

    if (strcmp(cs->csname, "utf8") == 0)
    {
        if (t_is_prefix)
        {
            /* count code points in tlen and advance slen accordingly */
            while (sPrefixLen < slen && tPrefixLen < tlen)
            {
                U8_NEXT(t, tPrefixLen, tlen, dummy);
                if (dummy == U_SENTINEL) break;
                U8_NEXT(s, sPrefixLen, slen, dummy);
            }
            slen = (sPrefixLen < slen)? sPrefixLen : slen;
        }
        uiter_setUTF8(&sIter, (const char*)s, slen);
        uiter_setUTF8(&tIter, (const char*)t, tlen);
        result = ucol_strcollIter(icu_collator, &sIter, &tIter, &status);
        DBUG_ASSERT(U_SUCCESS(status));
    }
    else if (strcmp(cs->csname, "ucs2") == 0)
    {
        if (t_is_prefix)
        {
            /* count code points in tlen and advance slen accordingly */
            while (sPrefixLen < slen && tPrefixLen < tlen)
            {
                U16_NEXT(t, tPrefixLen, tlen, dummy);
                if (dummy == U_SENTINEL) break;
                U16_NEXT(s, sPrefixLen, slen, dummy);
            }
            slen = (sPrefixLen < slen)? sPrefixLen : slen;
        }
        result = ucol_strcoll(icu_collator, (const UChar*) s, slen/sizeof(UChar),
                              (const UChar*)t, tlen/sizeof(UChar));
        DBUG_ASSERT(U_SUCCESS(status));
    }
    else /* not supported */
    {
        DBUG_ASSERT(0);
    }
    return result;
}

static int my_strnncollsp_icu(CHARSET_INFO *cs,
                                  const uchar *s, size_t slen,
                                  const uchar *t, size_t tlen,
                                  my_bool diff_if_only_endspace_difference)
{
    size_t i;
    uchar tmp[ICU_STACK_BUFFER];
    uchar * p;
    int result = UCOL_EQUAL;

#ifndef VARCHAR_WITH_DIFF_ENDSPACE_ARE_DIFFERENT_FOR_UNIQUE
    diff_if_only_endspace_difference= 0;
#endif

    /* handle case of an empty string */
    if (slen == 0)
    {
        if (tlen == 0) return 0;
        return -1;
    }
    else if (tlen == 0) return 1;

    if (slen == tlen || diff_if_only_endspace_difference)
        return my_strnncoll_icu(cs, s, slen, t, tlen, 0);

    /* TODO consider caching the buffer when it is greater than ICU_STACK_BUFFER
     to avoid allocating each time */
    if (slen < tlen)
    {
        if (tlen <= ICU_STACK_BUFFER) p = tmp;
        else p = (uchar*)my_malloc(tlen, MYF(MY_WME | MY_ZEROFILL));
        if (!p) return 0;

        memcpy(p, s, slen);
        /* pad with spaces */
        if (strcmp(cs->csname, "ucs2") == 0)
        {
            for (i = slen; i < tlen; i+=2)
            {
                *(p + i) = 0;
                *(p + i + 1) = ' ';
            }
        }
        else /* UTF-8 */
        {
            for (i = slen; i < tlen; i++)
            {
                *(p + i) = ' ';
            }
        }
        result = my_strnncoll_icu(cs, p, tlen, t, tlen, 0);
    }
    else /* tlen < slen */
    {
        if (tlen <= ICU_STACK_BUFFER) p = tmp;
        else p = (uchar*)my_malloc(slen, MYF(MY_WME | MY_ZEROFILL));
        if (!p) return 0;

        memcpy(p, t, tlen);
        /* pad with spaces */
        if (strcmp(cs->csname, "ucs2") == 0)
        {
            for (i = tlen; i < slen; i+=2)
            {
                *(p + i) = 0;
                *(p + i + 1) = ' ';
            }
        }
        else /* if (strcmp(cs->csname, "utf8") == 0) */
        {
            for (i = tlen; i < slen; i++)
            {
                *(p + i) = ' ';
            }
        }
        result = my_strnncoll_icu(cs, s, slen, p, slen, 0);

    }

    if (p != tmp)
        my_free((void*)p, MYF(0));
    return result;
}

static
int my_wildcmp_icu_helper(UCollator * icu_collator, UChar * sStr, int sLen, UChar * wStr, int wLen, int escape, int w_one, int w_many)
{
    char manyBefore = 0;
    int i, j;
    int result = -1;
    int wPos = 0;
    int wPosEnd = 0;
    int sPos = 0;
    UStringSearch * search = NULL;
    UErrorCode status = U_ZERO_ERROR;

    while (wPos < wLen)
    {
        if (wStr[wPos] == w_one)
        {
            ++sPos;
            if (sPos > sLen)
                return -1;
            manyBefore = 0;
            ++wPos;
            continue;
        }
        if (wStr[wPos] == w_many)
        {
            manyBefore = 1;
            ++wPos;
            continue;
        }
        if (wStr[wPos] == escape && wPos < wLen - 1)
        {
            /* shift everything up by 1 */
            for (i = wPos; i < wLen - 1; i++)
                wStr[i] = wStr[i+1];
            --wLen;
        }
        if (sPos == sLen) /* no more text to match */
            return -1;
        /* find the next wildcard */
        for (wPosEnd = wPos + 1; wPosEnd < wLen; wPosEnd++)
        {
            if (wStr[wPosEnd] == w_one)
            {
                break;
            }
            if (wStr[wPosEnd] == w_many)
            {
                break;
            }
            if (wStr[wPosEnd] == escape)
            {
                /* shift everything up by 1 */
                for (i = wPosEnd; i < wLen - 1; i++)
                    wStr[i] = wStr[i+1];
                --wLen;
            }
        }
        DBUG_ASSERT(sLen > sPos);
        DBUG_ASSERT(wPosEnd > wPos);
        search = usearch_openFromCollator(wStr + wPos, wPosEnd - wPos,
            sStr + sPos, sLen - sPos, icu_collator, NULL, &status);
        if (!search || !U_SUCCESS(status)) return 1;
        i = usearch_first(search, &status);
        if (i == USEARCH_DONE || !U_SUCCESS(status))
        {
            usearch_close(search);
            return (wPosEnd == wLen)? -1 : 1;
        }
        if (!manyBefore && (i != 0))
        {
            usearch_close(search);
            return (wPosEnd == wLen)? -1 : 1;
        }
        do
        {
            j = usearch_getMatchedLength(search);
            /* if we are at the end of the wildcard string */
            if ((wPosEnd == wLen))
            {
                usearch_close(search);
                return (sPos + i + j == sLen)? 0 : 1;
            }
            /* found a match, restart search after match */
            result = my_wildcmp_icu_helper(icu_collator,
                sStr + sPos + i + j, sLen - sPos - i - j,
                wStr + wPosEnd, wLen - wPosEnd, escape, w_one, w_many);
            if (result <= 0)
            {
                usearch_close(search);
                return result;
            }
            if (!manyBefore)
            {
                usearch_close(search);
                return 1;
            }
            i = usearch_next(search, &status);
        } while (i != USEARCH_DONE || !U_SUCCESS(status));
        break;
    }
    if (wPos == wLen) /* wild card at end of string */
    {
        if (manyBefore)
            result = 0;
        else
            result = (sPos == sLen)? 0 : 1;
    }
    if (search) usearch_close(search);
    return result;
}

/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 not matched, reached end of wildcard str
*/

static
int my_wildcmp_icu(CHARSET_INFO *cs,
		   const char *str,const char *str_end,
		   const char *wildstr,const char *wildend,
		   int escape, int w_one, int w_many)
{
    int result = -1;/* no match*/
    UErrorCode status = U_ZERO_ERROR;
    UChar * pStr = NULL;
    UChar * pWild = NULL;
    UChar * pNormStr = NULL;
    UChar * pNormWild = NULL;
    UChar wildBuffer[ICU_STACK_BUFFER];
    UChar strBuffer[ICU_STACK_BUFFER];
    int32_t wLen = 0;
    int32_t sLen = 0;
    int sOrigLen = str_end - str;
    int wOrigLen = wildend - wildstr;

    UCollator * icu_collator = (UCollator*)cs->sort_order;
    UConverter * icu_converter = (UConverter*)cs->contractions;
    if (strcmp(cs->csname, "utf8") == 0)
    {
        /* assume doubling the utf8 size will be sufficient for utf16
         create double the desired buffer to give space for normalization */
        if (2 * wOrigLen < ICU_STACK_BUFFER)
        {
            pWild = wildBuffer;
        }
        else
        {
            pWild = (UChar*)my_malloc(2 * wOrigLen * sizeof(UChar), MYF(MY_WME));
            if (!pWild) return 1;
        }
        if (2 * sOrigLen < ICU_STACK_BUFFER)
        {
            pStr = strBuffer;
        }
        else
        {
            pStr = (UChar*)my_malloc(2 * sOrigLen * sizeof(UChar), MYF(MY_WME));
            if (!pStr) return 1;
        }
        wLen = ucnv_toUChars(icu_converter, pWild, 2 * wOrigLen,
                             wildstr, wOrigLen, &status);
        if (!U_SUCCESS(status)) return 1;
        DBUG_ASSERT(wLen < wOrigLen);
        sLen = ucnv_toUChars(icu_converter, pStr, 2 * sOrigLen, str,
                             sOrigLen, &status);
        if (!U_SUCCESS(status)) return 1;
        DBUG_ASSERT(sLen < sOrigLen);

        /* normalize if needed */
        if (unorm_quickCheck(pStr, sLen, UNORM_NFC, &status) != UNORM_YES || !U_SUCCESS(status))
        {
            pNormStr = pStr + sOrigLen;
            sLen = unorm_normalize(pStr, sLen, UNORM_NFC, 0, pNormStr,
                sOrigLen,  &status);
            DBUG_ASSERT(U_SUCCESS(status));
        }
        else pNormStr = pStr;/* no need to normalize */

        if (unorm_quickCheck(pWild, wLen, UNORM_NFC, &status) != UNORM_YES || !U_SUCCESS(status))
        {
            pNormWild = pWild + wOrigLen;
            wLen = unorm_normalize(pWild, wLen, UNORM_NFC, 0, pNormWild,
                wOrigLen,  &status);
            DBUG_ASSERT(U_SUCCESS(status));
        }
        else pNormWild = pWild; /* no need to normalize */

    }
    else if (strcmp(cs->csname, "ucs2") == 0)
    {
        /* no need to convert, just cast */
        pStr = (UChar*)str;
        pWild = (UChar*)wildstr;
        sLen = sOrigLen/2;
        wLen = wOrigLen/2;

        if (unorm_quickCheck(pStr, sLen, UNORM_NFC, &status) != UNORM_YES || !U_SUCCESS(status))
        {
            if (sOrigLen > ICU_STACK_BUFFER)
            {
                pNormStr = (UChar*)my_malloc(sOrigLen * sizeof(UChar), MYF(MY_WME));
                if (pNormStr == NULL) return -1; /* out of memory*/
            }
            else
            {
                pNormStr = strBuffer;
            }
            sLen = unorm_normalize(pStr, sLen, UNORM_NFC, 0, pNormStr,
                sOrigLen,  &status);
            pStr = pNormStr; /* for free check*/
            DBUG_ASSERT(U_SUCCESS(status));
        }
        else pNormStr = pStr;/* no need to normalize */

        if (unorm_quickCheck(pWild, wLen, UNORM_NFC, &status) != UNORM_YES || !U_SUCCESS(status))
        {
            if (wOrigLen > ICU_STACK_BUFFER)
            {
                pNormWild = (UChar*)my_malloc(wOrigLen * sizeof(UChar), MYF(MY_WME));
                if (pNormWild == NULL) return -1; /* out of memory */
            }
            else
            {
                pNormWild = wildBuffer;
            }
            wLen = unorm_normalize(pWild, wLen, UNORM_NFC, 0, pNormWild,
                wOrigLen,  &status);
            pWild = pNormWild; /* for free check */
            DBUG_ASSERT(U_SUCCESS(status));
        }
        else pNormWild = pWild;/* no need to normalize */
    }
    else
    {
        DBUG_ASSERT(0);
        return 1; /* other encodings not supported */
    }

    result = my_wildcmp_icu_helper(icu_collator, pNormStr, sLen, pNormWild, wLen, escape, w_one, w_many);

    /* deallocate if needed */
    if (pWild != wildBuffer && pWild != (const UChar*)wildstr)
        my_free((void*)pWild, MYF(0));
    if (pStr != strBuffer && pStr != (const UChar*)str)
        my_free((void*)pStr, MYF(0));
    return result;
}


/*
  Calculates hash value for the given string,
  according to the collation, and ignoring trailing spaces.
  
  SYNOPSIS:
    my_hash_sort_uca()
    cs		Character set information
    s		String
    slen	String's length
    n1		First hash parameter
    n2		Second hash parameter
  
  NOTES:
    Scans consequently weights and updates
    hash parameters n1 and n2. In a case insensitive collation,
    upper and lower case of the same letter will return the same
    weight sequence, and thus will produce the same hash values
    in n1 and n2.
  
  RETURN
    N/A
*/

static void my_hash_sort_icu(CHARSET_INFO *cs,
                                 const uchar *s, size_t slen,
                                 ulong *n1, ulong *n2)
{
    int i = 0;
    uchar dest[2];
    uint32_t state[2];
    UErrorCode status = U_ZERO_ERROR;
    struct UCharIterator iter;
    UCollator * icu_collator = (UCollator*)cs->sort_order;

    slen= cs->cset->lengthsp(cs, (char*) s, slen);
    state[1] = state[0] = 0;

    if (slen == 0)
        return;

    if (strcmp(cs->csname,"ucs2")==0) /* uscs2 */
    {
        uiter_setString(&iter, (UChar*)s, slen/sizeof(UChar));
    }
    else if (strcmp(cs->csname,"utf8")==0)
    {
        uiter_setUTF8(&iter, (const char*)s, slen);
    }
    else
    {
        DBUG_ASSERT(0);
        return; /* other types not supported yet */
    }

    do
    {
        i = ucol_nextSortKeyPart(icu_collator, &iter, state, dest, (size_t)2, &status);
        if (!U_SUCCESS(status)) break;
        if (i < 2)
        {
            dest[1] = 0;
            if (i == 0) break;
        }
        n1[0]^= (((n1[0] & 63)+n2[0])*(dest[1]))+ (n1[0] << 8);
        n2[0]+=3;
        n1[0]^= (((n1[0] & 63)+n2[0])*(dest[0]))+ (n1[0] << 8);
        n2[0]+=3;
    } while (i);

}

MY_COLLATION_HANDLER my_collation_icu_handler =
{
    my_coll_init_icu,	/* init */
    my_strnncoll_icu,
    my_strnncollsp_icu,
    my_strnxfrm_icu,
    my_strnxfrmlen_simple,
    my_like_range_mb,
    my_wildcmp_icu,
    NULL,
    my_instr_mb,
    my_hash_sort_icu,
    my_propagate_complex
};

#ifdef HAVE_CHARSET_ucs2

CHARSET_INFO my_charset_ucs2_icu_ci=
{
    148,0,0,		/* number       */
    MY_CS_COMPILED|MY_CS_STRNXFRM|MY_CS_UNICODE,
    "ucs2",		/* cs name    */
    "ucs2_icu_root_ci",	/* name         */
    "",			/* comment      */
    "root",		/* tailoring    */
    NULL,		/* ctype        */
    NULL,		/* to_lower     */
    NULL,		/* to_upper     */
    NULL,		/* sort_order   */
    NULL,		/* contractions */
    NULL,		/* sort_order_big*/
    NULL,		/* tab_to_uni   */
    NULL,		/* tab_from_uni */
    my_unicase_default, /* caseinfo     */
    NULL,		/* state_map    */
    NULL,		/* ident_map    */
    16,			/* strxfrm_multiply */
    1,                  /* caseup_multiply  */
    1,                  /* casedn_multiply  */
    2,			/* mbminlen     */
    2,			/* mbmaxlen     */
    9,			/* min_sort_char */
    0xFFFF,		/* max_sort_char */
    ' ',                /* pad char      */
    0,                  /* escape_with_backslash_is_dangerous */
    &my_charset_ucs2_handler,
    &my_collation_icu_handler
};
#endif

#ifdef HAVE_CHARSET_utf8

/*
  We consider bytes with code more than 127 as a letter.
  This garantees that word boundaries work fine with regular
  expressions. Note, there is no need to mark byte 255  as a
  letter, it is illegal byte in UTF8.
*/
static uchar ctype_utf8[] = {
    0,
   32, 32, 32, 32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 32, 32,
   32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
  132,132,132,132,132,132,132,132,132,132, 16, 16, 16, 16, 16, 16,
   16,129,129,129,129,129,129,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 16, 16, 16, 16, 16,
   16,130,130,130,130,130,130,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 16, 16, 16, 16, 32,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  0
};

CHARSET_INFO my_charset_utf8_icu_ci=
{
    212,0,0,		/* number       */
    MY_CS_COMPILED|MY_CS_STRNXFRM|MY_CS_UNICODE,
    "utf8",		/* cs name    */
    "utf8_icu_root_ci",	/* name         */
    "",			/* comment      */
    "root",		/* tailoring    */
    ctype_utf8,		/* ctype        */
    NULL,		/* to_lower     */
    NULL,		/* to_upper     */
    NULL,		/* sort_order   */
    NULL,		/* contractions */
    NULL,		/* sort_order_big*/
    NULL,		/* tab_to_uni   */
    NULL,		/* tab_from_uni */
    my_unicase_default, /* caseinfo     */
    NULL,		/* state_map    */
    NULL,		/* ident_map    */
    16,			/* strxfrm_multiply */
    1,                  /* caseup_multiply  */
    1,                  /* casedn_multiply  */
    1,			/* mbminlen     */
    3,			/* mbmaxlen     */
    9,			/* min_sort_char */
    0xFFFF,		/* max_sort_char */
    ' ',                /* pad char      */
    0,                  /* escape_with_backslash_is_dangerous */
    &my_charset_utf8_handler,
    &my_collation_icu_handler
};

#endif /* HAVE_CHARSET_utf8 */


#endif /* HAVE_UNICODE_UCOL_H */
