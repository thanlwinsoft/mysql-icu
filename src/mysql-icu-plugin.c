/***************************************************************************
 *   Copyright (C) 2009 by Keith Stribley                                  *
 *   devel@thanlwinsoft.org                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <my_list.h>
#include <mysql_version.h>
#include <plugin.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <unicode/uclean.h>
#include <unicode/uchar.h>
#include <unicode/ucnv.h>
#include <unicode/ubrk.h>
#include <unicode/uversion.h>

#include "word-brk-iter.h"

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

#ifdef HAVE_CHARSET_ucs2
extern CHARSET_INFO my_charset_ucs2_icu_ci;
#endif
#ifdef HAVE_CHARSET_utf8
extern CHARSET_INFO my_charset_utf8_icu_ci;
#endif

static void* icu_malloc(const void* context, size_t size)
{ return (void*)my_malloc(size,MYF(MY_WME)); }

static void* icu_realloc(const void* context, void* ptr, size_t size)
{ return (void*)my_realloc(ptr,size,MYF(MY_WME)); }

static void  icu_free(const void* context, void *ptr)
{ my_free(ptr,MYF(0)); }


static char     *icu_plugin_locales_value;
static char     *icu_plugin_collation_file_value;
static char     *icu_plugin_wordbreak_file_value;

static long number_of_calls= 0; /* for SHOW STATUS, see below */


static void addLocale(int locIndex, const char * locale, CHARSET_INFO * baseCharset)
{
    char * csName = 0;
    char * locName = 0;
    all_charsets[locIndex] = my_malloc(sizeof(CHARSET_INFO), MYF(MY_WME));
    csName = my_malloc(strlen(locale) + 13, MYF(MY_WME));
    locName = my_malloc(strlen(locale) + 1, MYF(MY_WME));
    if (all_charsets[locIndex] && csName && locName)
    {
        memcpy(all_charsets[locIndex],
            baseCharset, sizeof(CHARSET_INFO));
        all_charsets[locIndex]->number = locIndex;
        sprintf(locName, "%s", locale);
        sprintf(csName, "utf8_icu_%s_ci", locale);
        all_charsets[locIndex]->name = csName;
        all_charsets[locIndex]->tailoring = locName;
    }
    else
    {
        if (all_charsets[locIndex]) my_free(all_charsets[locIndex], MYF(0));
        if (csName) my_free(csName, MYF(0));
        if (locName) my_free(locName, MYF(0));
        all_charsets[locIndex] = 0;
    }
}

static uchar * loadDataFromFile(const char * fileName, my_off_t * fileLen)
{
    uchar * data = NULL;
    FILE * icuRules = my_fopen(fileName, O_RDONLY | O_BINARY, MYF(MY_WME));
    if (icuRules)
    {
        *fileLen = my_fseek(icuRules, 0L, MY_SEEK_END, MYF(MY_WME));
        *fileLen = my_ftell(icuRules, MYF(MY_WME));
        my_fseek(icuRules, 0L, MY_SEEK_SET, MYF(MY_WME));
        data = my_malloc(*fileLen, MYF(MY_WME));
        if (data && my_fread(icuRules, data, *fileLen, MYF(MY_WME)) == *fileLen)
        {
            /* data read successfully*/
        }
        else
        {
            my_free(data, MYF(MY_WME));
            data = NULL;
            fprintf(stderr, "ICU: Failed to read ICU rules from %s length %ld\n",
            icu_plugin_collation_file_value, (long)*fileLen);
        }
        my_fclose(icuRules, MYF(MY_WME));
    }
    else
    {
        fprintf(stderr, "ICU: Failed to read ICU rules from %s\n",
            icu_plugin_collation_file_value);
    }
    return data;
}

static int mysql_icu_plugin_init(void *arg __attribute__((unused)))
{
    int i = 0;
    int j = 0;
    int lastUtf8 = 0;
    int lastUcs2 = 0;
    int icuLocaleCount = 0;
    char locale[128];
    my_off_t fileLen = 0;
    UErrorCode ustatus = 0;
    UVersionInfo versionInfo;
    uchar * data = NULL;
    u_getVersion(versionInfo); /* get ICU version */
    u_setMemoryFunctions(NULL, icu_malloc, icu_realloc, icu_free, &ustatus);

    for (i = 0; i < sizeof(all_charsets) / sizeof(all_charsets[0]); i++)
    {
        if (all_charsets[i])
        {
            /*fprintf(stderr, "ICU: %d %s %s\n", i, all_charsets[i]->csname,
                    all_charsets[i]->name);*/
            if (strcmp(all_charsets[i]->csname, "utf8") == 0) lastUtf8 = i;
            if (strcmp(all_charsets[i]->csname, "ucs2") == 0) lastUcs2 = i;
        }
    }
    /*fprintf(stderr, "ICU: last utf8 %d last ucs2 %d\n", lastUtf8, lastUcs2);*/
    ++icuLocaleCount;
#ifdef HAVE_CHARSET_ucs2
    my_charset_ucs2_icu_ci.number = lastUcs2+1;
    all_charsets[lastUcs2+1] = &my_charset_ucs2_icu_ci;
    all_charsets[lastUcs2+1]->state |= MY_CS_AVAILABLE;
#endif
#ifdef HAVE_CHARSET_utf8
    my_charset_utf8_icu_ci.number = lastUtf8+1;
    all_charsets[lastUtf8+1] = &my_charset_utf8_icu_ci;
    all_charsets[lastUtf8+1]->state |= MY_CS_AVAILABLE;
#endif
    if (icu_plugin_locales_value)
    {
        for (i = 0; i < strlen(icu_plugin_locales_value); i++)
        {
            if (icu_plugin_locales_value[i] == ' ') continue;
            for (j = i; j < strlen(icu_plugin_locales_value); j++)
            {
                if (icu_plugin_locales_value[j] == ' ')
                {
                    break;
                }
            }
            if (j - i < sizeof(locale))
            {
                strncpy(locale, icu_plugin_locales_value + i, j - i);
                locale[j-i] = '\0';
                ++icuLocaleCount;

#ifdef HAVE_CHARSET_ucs2
                addLocale(lastUcs2+icuLocaleCount, locale, &my_charset_ucs2_icu_ci);
#endif
#ifdef HAVE_CHARSET_utf8
                addLocale(lastUtf8+icuLocaleCount, locale, &my_charset_utf8_icu_ci);
#endif
            }
            i = j;
        }
    }

    /* custom*/
    if (icu_plugin_collation_file_value)
    {
        data = loadDataFromFile(icu_plugin_collation_file_value, &fileLen);
        if (data)
        {
                ++icuLocaleCount;

#ifdef HAVE_CHARSET_ucs2
                addLocale(lastUcs2+icuLocaleCount, "custom", &my_charset_ucs2_icu_ci);
                all_charsets[lastUcs2+icuLocaleCount]->tailoring = data;
                data = my_malloc(fileLen, MYF(MY_WME));
                if (data)
                    memcpy(data, all_charsets[lastUcs2+icuLocaleCount]->tailoring, fileLen);
#endif
#ifdef HAVE_CHARSET_utf8
                if (data)
                {
                    addLocale(lastUtf8+icuLocaleCount, "custom", &my_charset_utf8_icu_ci);
                    all_charsets[lastUtf8+icuLocaleCount]->tailoring = (char*)data;
                }
#endif
        }
    }

    return 0;
}

static int mysql_icu_plugin_deinit(void *arg __attribute__((unused)))
{
    /* TODO remove icu charsets */
    return(0);
}

static int mysql_icu_parser_init(MYSQL_FTPARSER_PARAM *param __attribute__((unused)))
{

    return 0;
}

static int mysql_icu_parser_deinit(MYSQL_FTPARSER_PARAM *param)
{
    if (param->ftparser_state)
    {
        ubrk_close((UBreakIterator *)param->ftparser_state);
        param->ftparser_state = NULL;
    }
    return 0;
}

extern char ft_boolean_syntax[15];
/* Boolean search operators */
#define FTB_YES   (ft_boolean_syntax[0])
#define FTB_EGAL  (ft_boolean_syntax[1])
#define FTB_NO    (ft_boolean_syntax[2])
#define FTB_INC   (ft_boolean_syntax[3])
#define FTB_DEC   (ft_boolean_syntax[4])
#define FTB_LBR   (ft_boolean_syntax[5])
#define FTB_RBR   (ft_boolean_syntax[6])
#define FTB_NEG   (ft_boolean_syntax[7])
#define FTB_TRUNC (ft_boolean_syntax[8])
#define FTB_LQUOT (ft_boolean_syntax[10])
#define FTB_RQUOT (ft_boolean_syntax[11])

static void add_word(MYSQL_FTPARSER_PARAM *param, UChar * uWord, int uWordLen,
    char * word, int len, UWordBreak breakStatus, UChar prev, UChar next,
    char * inQuote)
{
    /*
    UErrorCode status = U_ZERO_ERROR;
    UConverter * icu_converter = (UConverter*)param->cs->contractions;
    char word[1024];
    int utfLen = 0;
    */
    int i;

    MYSQL_FTPARSER_BOOLEAN_INFO boolInfo=
    { FT_TOKEN_WORD, 0, 0, 0, 0, ' ', 0 };

    if (prev == (UChar)FTB_YES) boolInfo.yesno = 1;
    if (prev == (UChar)FTB_EGAL) boolInfo.yesno = 0;
    if (prev == (UChar)FTB_NO) boolInfo.yesno = -1;
    if (prev == (UChar)FTB_INC) boolInfo.weight_adjust++;
    if (prev == (UChar)FTB_DEC) boolInfo.weight_adjust--;
    if (prev == (UChar)FTB_NEG) boolInfo.wasign= !boolInfo.wasign;
    if (next == (UChar)FTB_TRUNC) boolInfo.trunc = (char)1;

    if (breakStatus >= UBRK_WORD_NONE && breakStatus < UBRK_WORD_NONE_LIMIT)
    {
        fprintf(stderr, "ICU: non word '%x'\n", uWord[0]);
        if (uWordLen == 1)
        {
            if (uWord[0] == FTB_LBR || (!(*inQuote) && uWord[0] == FTB_LQUOT))
            {
                boolInfo.type = FT_TOKEN_LEFT_PAREN;
                if (uWord[0] == FTB_LQUOT)
                {
                    boolInfo.quot = (char*)1;
                    *inQuote = 1;
                }
            }
            else if (uWord[0] == FTB_RBR || uWord[0] == FTB_RQUOT)
            {
                boolInfo.type = FT_TOKEN_RIGHT_PAREN;
                if (uWord[0] == FTB_RQUOT)
                    *inQuote = 0;
            }
            else
            {
                return;
            }
        }
        else
        {
            fprintf(stderr, "ICU: non word '%x' len %d\n", uWord[0], uWordLen);
            return; /* ignore other characters */
        }
    }
    if (*inQuote)
    {
        /*boolInfo.quot = (char*)1;*/
        boolInfo.yesno = 1;
    }

    if (param->cs->contractions) /* UTF-8 */
    {
        /* convert back to UTF-8 * /
        utfLen = ucnv_fromUChars(icu_converter, word, 1024, uWord, uWordLen, &status);
        if (!U_SUCCESS(status)) return;/ * ignore this word * /
        param->flags |= MYSQL_FTFLAGS_NEED_COPY;*/
        fprintf(stderr, "ICU: add_word %d yesno%d ", boolInfo.type, (int)boolInfo.yesno);
        for (i = 0; i < len; i++) fprintf(stderr, "%c", word[i]);
        fprintf(stderr, "\n");

        param->mysql_add_word(param, word, len, &boolInfo);
    }
    else
    {
        param->mysql_add_word(param, (char*)uWord, uWordLen*sizeof(UChar), &boolInfo);
    }
}

static int mysql_icu_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator * iBreak = param->ftparser_state;
    UChar * uText = NULL;
    int textLen = 0;
    my_off_t fileLen = 0;
    uchar * brkRules = NULL;
    UChar * uBrkRules = NULL;
    UParseError parseErr;
    UConverter * icu_converter = (UConverter*)param->cs->contractions;
    int start = 0;
    int boundary = 0;
    char quote = 0;
    UChar32 uCode32;
    char * pDoc = param->doc;
    int u16pos = 0, u8s = 0, u8e = 0;

    if (strstr(param->cs->name, "icu") == NULL)
    {
        fprintf(stderr, "ICU: parser can only be used for ICU charsets");
        return param->mysql_parse(param, param->doc, param->length);
    }

    ++number_of_calls;
    if (icu_converter)
    {
        DBUG_ASSERT(strcmp(param->cs->csname, "utf8") == 0);
        uText = my_malloc(param->length * 2 * sizeof(UChar), MYF(MY_WME));
        textLen = ucnv_toUChars(icu_converter, uText, param->length * 2,
            param->doc, param->length, &status);
        if (!U_SUCCESS(status)) return 1;
    }
    else
    {
        DBUG_ASSERT(strcmp(param->cs->csname, "ucs2") == 0);
        uText = (UChar*)param->doc;
        textLen = param->length/2;
    }
    if (iBreak)
    {
        ubrk_setText(iBreak, uText, textLen, &status);
        if (!U_SUCCESS(status))
            return 1;
    }
    else
    {
        if (strlen(param->cs->tailoring) < strlen(param->cs->name))
        {
            iBreak = ubrk_open(UBRK_WORD, param->cs->tailoring, uText, textLen, &status);
        }
        else if (icu_plugin_wordbreak_file_value)
        {
            /* we use custom break rules for custom collation */
            brkRules = loadDataFromFile(icu_plugin_wordbreak_file_value, &fileLen);
            if (brkRules)
            {
                uBrkRules = my_malloc(fileLen * 2, MYF(MY_WME));
                if (uBrkRules)
                {
                    if (icu_converter == NULL) /* UCS2 charset*/
                    {
                        icu_converter = ucnv_open("utf8", &status);
                    }
                    if (icu_converter)
                    {
                        fileLen = ucnv_toUChars(icu_converter, uBrkRules, fileLen * 2, (char*)brkRules, fileLen, &status);
                    }
                    if (U_SUCCESS(status) && fileLen)
                    {
                        /*iBreak = ubrk_openRules(uBrkRules, fileLen, uText, textLen, &parseErr, &status);*/
                        iBreak = createBreakIterator(uBrkRules, fileLen,
                            UBRK_WORD, &parseErr, &status);
                        if (iBreak && U_SUCCESS(status))
                            ubrk_setText(iBreak, uText, textLen, &status);
                        if (U_FAILURE(status))
                        {
                            iBreak = NULL;
                            fprintf(stderr, "ICU: Failed to parse break rules at %s line %d, %d",
                                icu_plugin_wordbreak_file_value, parseErr.line, parseErr.offset);
                        }
                        else
                        {
                            fprintf(stderr, "ICU: Loaded custom word break rules\n");
                        }
                    }
                    if (!param->cs->contractions)
                    {
                        ucnv_close(icu_converter);
                    }
                    my_free(uBrkRules, MYF(MY_WME));
                }
            }
            else
            {
                fprintf(stderr, "ICU: Failed to load break rules from %s\n", icu_plugin_wordbreak_file_value);
            }
        }
        if (!iBreak)
        {
            iBreak = ubrk_open(UBRK_WORD, "root", uText, textLen, &status);
        }
        if (!U_SUCCESS(status))
        {
            return 1;
        }
        param->ftparser_state = iBreak;
    }

    boundary = ubrk_next(iBreak);
    if (param->cs->contractions) /* UTF-8 */
    {
        while (boundary != UBRK_DONE)
        {
            /* compute utf8 positions */
            for (u16pos = start; u16pos < boundary;
                 u16pos += U16_IS_SURROGATE(uText[u16pos])? 2 : 1)
            {
                U8_NEXT(pDoc, u8e, param->length, uCode32);
            }
            add_word(param, uText + start, boundary - start,
                param->doc + u8s, u8e - u8s,
                ubrk_getRuleStatus(iBreak),
                (start > 0)? uText[start - 1] : ' ',
                (boundary < textLen)? uText[boundary] : ' ', &quote);
            start = boundary;
            u8s = u8e;
            boundary = ubrk_next(iBreak);
        }
        if (start < textLen)
        {
            for (u16pos = start; u16pos < boundary;
                 u16pos += U16_IS_SURROGATE(uText[u16pos])? 2 : 1)
            {
                U8_NEXT(pDoc, u8e, param->length, uCode32);
            }
            add_word(param, uText + start, textLen - start,
                param->doc + u8s, u8e - u8s,
                ubrk_getRuleStatus(iBreak), (start > 0)? uText[start - 1] : ' ',
                ' ', &quote);
        }
    }
    else /* UCS2*/
    {
        while (boundary != UBRK_DONE)
        {
            /* compute  positions */
            add_word(param, uText + start, boundary - start,
                (char*)(uText + start), 2 * (textLen - start),
                ubrk_getRuleStatus(iBreak),
                (start > 0)? uText[start - 1] : ' ',
                (boundary < textLen)? uText[boundary] : ' ', &quote);
            start = boundary;
            boundary = ubrk_next(iBreak);
        }
        if (start < textLen)
        {
            add_word(param, uText + start, textLen - start,
                (char*)(uText + start), 2 * (textLen - start),
                ubrk_getRuleStatus(iBreak), (start > 0)? uText[start - 1] : ' ',
                ' ', &quote);
        }
    }

    if (param->cs->contractions) /* UTF-8 */
    {
        my_free(uText, MYF(MY_WME));
    }
    return 0;
}

static int icu_plugin_locales_check(MYSQL_THD thd,
                                    struct st_mysql_sys_var *var,
                                    void *save, struct st_mysql_value *value)
{
    static const int STATE_DELIMITER = 0;
/*
    static const int STATE_LANG1 = 1;
    static const int STATE_LANG2 = 2;
    static const int STATE_SEP1 = 3;
    static const int STATE_COUNTRY1 = 4;
    static const int STATE_COUNTRY2 = 5;
    static const int STATE_SEP2 = 6;
*/
    const char *str;
    int len = 256;
    char locales[256];
    int i;
    int state = STATE_DELIMITER;

    if (!var || !value)
        return -1;
    str = value->val_str(value, locales, &len);
    if (!str) return -2;
    fprintf(stderr, "ICU: Check locales: %s %d\n", str, len);
    for (i = 0; i < len; i++)
    {
        switch (state)
        {
            case 0:/*STATE_DELIMITER*/
                if (str[i] == ' ') continue;
                if (str[i] >= 'a' && str[i] <= 'z')
                {
                    ++state;
                    break;
                }
                return 1;
            case 1:/*STATE_LANG1*/
                if (str[i] >= 'a' && str[i] <= 'z')
                {
                    ++state;
                    break;
                }
                return 1;
            case 2:/*STATE_LANG2*/
                if (str[i] == ' ')
                {
                    state = STATE_DELIMITER;
                    break;
                }
                if (str[i] == '_')
                {
                    ++state;
                    break;
                }
                return 1;
            case 3:/*STATE_SEP1*/
            case 4:/*STATE_COUNTRY1*/
                if (str[i] >= 'A' && str[i] <= 'Z')
                {
                    ++state;
                    break;
                }
                return 1;
            case 5:/*STATE_COUNTRY2*/
                if (str[i] == '_')
                {
                    ++state;
                    break;
                }
                if (str[i] == ' ')
                {
                    state = STATE_DELIMITER;
                    break;
                }
                return 1;
            case 6:/*STATE_SEP2 most characters after this are ok until space*/
                if (str[i] == ' ')
                {
                    state = STATE_DELIMITER;
                    break;
                }
                break;
            default:
                fprintf(stderr, "ICU: Unexpected state %d\n", state);
                return -1;
        }
    }
    fprintf(stderr, "ICU: Locale OK\n");
    *(const char**)save=str;
    return 0;
}

static MYSQL_SYSVAR_STR(locales, icu_plugin_locales_value,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Space delimited list of ICU locales to use.",
  icu_plugin_locales_check, NULL, "");

static MYSQL_SYSVAR_STR(custom_collation_file, icu_plugin_collation_file_value,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Path to file containing custom ICU collation rules.",
  NULL, NULL, "");

static MYSQL_SYSVAR_STR(custom_wordbreak_file, icu_plugin_wordbreak_file_value,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Path to file containing custom ICU word break rules.",
  NULL, NULL, "");


static struct st_mysql_sys_var* icu_plugin_system_variables[]= {
  MYSQL_SYSVAR(locales),
  MYSQL_SYSVAR(custom_collation_file),
  MYSQL_SYSVAR(custom_wordbreak_file),
  NULL
};

static struct st_mysql_ftparser mysql_icu_parser_descriptor =
{
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  mysql_icu_parser_parse,              /* parsing function       */
  mysql_icu_parser_init,               /* parser init function   */
  mysql_icu_parser_deinit              /* parser deinit function */
};

/*
  Plugin status variables for SHOW STATUS
*/

static struct st_mysql_show_var icu_plugin_status[]=
{
  {"icu_name",     (char *)"ICU plugin",     SHOW_CHAR},
  {"icu_called",     (char *)&number_of_calls, SHOW_LONG},
  {0,0,0}
};

mysql_declare_plugin(ft_icu)
{
  MYSQL_FTPARSER_PLUGIN,      /* type                            */
  &mysql_icu_parser_descriptor,  /* descriptor                   */
  "icu",                   /* name                               */
  "Keith Stribley http://www.thanlwinsoft.org", /* author        */
  "ICU Collation + Parser", /* description                       */
  PLUGIN_LICENSE_GPL,
  mysql_icu_plugin_init,  /* init function (when loaded)         */
  mysql_icu_plugin_deinit,/* deinit function (when unloaded)     */
  0x0001,                     /* version                         */
  icu_plugin_status,               /* status variables           */
  icu_plugin_system_variables,     /* system variables                                  */
  NULL
}
mysql_declare_plugin_end;
