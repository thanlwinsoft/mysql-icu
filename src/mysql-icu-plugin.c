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
#include <unicode/uversion.h>

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
static char     *icu_plugin_custom_file_value;

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

static int mysql_icu_plugin_init(void *arg __attribute__((unused)))
{
    int i = 0;
    int j = 0;
    int lastUtf8 = 0;
    int lastUcs2 = 0;
    int icuLocaleCount = 0;
    char locale[128];
    UErrorCode ustatus = 0;
    UVersionInfo versionInfo;
    char * data = NULL;
    u_getVersion(versionInfo); /* get ICU version */
    u_setMemoryFunctions(NULL, icu_malloc, icu_realloc, icu_free, &ustatus);

    for (i = 0; i < sizeof(all_charsets) / sizeof(all_charsets[0]); i++)
    {
        if (all_charsets[i])
        {
            fprintf(stderr, "%d %s %s\n", i, all_charsets[i]->csname,
                    all_charsets[i]->name);
            if (strcmp(all_charsets[i]->csname, "utf8") == 0) lastUtf8 = i;
            if (strcmp(all_charsets[i]->csname, "ucs2") == 0) lastUcs2 = i;
        }
    }
    fprintf(stderr, "last utf8 %d last ucs2 %d\n", lastUtf8, lastUcs2);
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
    if (icu_plugin_custom_file_value)
    {
        FILE * icuRules = my_fopen(icu_plugin_custom_file_value, O_WRONLY | O_BINARY, MYF(MY_WME));
        if (icuRules)
        {
            my_off_t fileLen = my_fseek(icuRules, 0L, MY_SEEK_END, MYF(MY_WME));
            my_fseek(icuRules, 0L, MY_SEEK_SET, MYF(MY_WME));
            data = my_malloc(fileLen, MYF(MY_WME));
            if (data && my_fread(icuRules, data, fileLen, MYF(MY_WME)) == fileLen)
            {
                ++icuLocaleCount;

#ifdef HAVE_CHARSET_ucs2
                addLocale(lastUcs2+icuLocaleCount, "custom", &my_charset_ucs2_icu_ci);
                all_charsets[icuLocaleCount]->tailoring = data;
                data = my_malloc(fileLen, MYF(MY_WME));
                if (data) memcpy(data, all_charsets[icuLocaleCount]->tailoring);
#endif
#ifdef HAVE_CHARSET_utf8
                if (data)
                {
                    addLocale(lastUtf8+icuLocaleCount, "custom", &my_charset_utf8_icu_ci);
                    all_charsets[icuLocaleCount]->tailoring = data;
                }
#endif
            }
            else
            {
                fprintf(stderr, "Failed to read ICU rules from %s length %ld\n",
                    icu_plugin_custom_file_value, fileLen);
            }
        }
    }

    return 0;
}

static int mysql_icu_plugin_deinit(void *arg __attribute__((unused)))
{
  return(0);
}


static int mysql_icu_parser_init(MYSQL_FTPARSER_PARAM *param __attribute__((unused)))
{
    return 0;
}
static int mysql_icu_parser_deinit(MYSQL_FTPARSER_PARAM *param __attribute__((unused)))
{
    return 0;
}
static int mysql_icu_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
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
    fprintf(stderr, "Check locales: %s %d\n", str, len);
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
                fprintf(stderr, "Unexpected state %d\n", state);
                return -1;
        }
    }
    fprintf(stderr, "Locale OK\n");
    *(const char**)save=str;
    return 0;
}

static MYSQL_SYSVAR_STR(locales, icu_plugin_locales_value,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Space delimited list of ICU locales to use.",
  icu_plugin_locales_check, NULL, "");

static MYSQL_SYSVAR_STR(custom_file, icu_plugin_custom_file_value,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Path to file containing custom ICU collation rules.",
  NULL, NULL, "");


static struct st_mysql_sys_var* icu_plugin_system_variables[]= {
  MYSQL_SYSVAR(locales),
  MYSQL_SYSVAR(custom_file),
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
  {"static",     (char *)"ICU plugin",     SHOW_CHAR},
  {"called",     (char *)&number_of_calls, SHOW_LONG},
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
