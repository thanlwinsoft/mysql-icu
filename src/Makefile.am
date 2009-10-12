INCLUDES = 
METASOURCES = AUTO
pkgplugindir=$(MYSQL_PLUGIN_DIR)
pkgplugin_LTLIBRARIES = libmysql-icu.la

# This may need modifying if you don't have ucs2 enabled
# check if which collations you have with
# mysql> show collation;
EXTRA_CFLAGS = -DHAVE_CHARSET_utf8 -DHAVE_CHARSET_ucs2 -DMYSQL_DYNAMIC_PLUGIN

libmysql_icu_la_CFLAGS = $(ICU_CFLAGS) $(MYSQL_CFLAGS) $(EXTRA_CFLAGS)
libmysql_icu_la_LDFLAGS = $(ICU_LDFLAGS) $(MYSQL_LDFLAGS)
libmysql_icu_la_SOURCES = ctype-icu.c mysql-icu-plugin.c word-brk-iter.cpp
noinst_HEADERS = word-brk-iter.h
