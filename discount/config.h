/*
 * configuration for markdown, generated Mon Nov 23 01:15:14 MSK 2009
 * by dmitry@Dmitry-Chestnykhs-MacBook-Air.local
 */
#ifndef __AC_MARKDOWN_D
#define __AC_MARKDOWN_D 1


#define OS_DARWIN 1
#define DWORD	unsigned int
#define WORD	unsigned short
#define BYTE	unsigned char
#define HAVE_PWD_H 1
#define HAVE_GETPWUID 1
#define HAVE_SRANDOM 1
#define INITRNG(x) srandom((unsigned int)x)
#define HAVE_RANDOM 1
#define COINTOSS() (random()&1)
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_FCHDIR 1
#define TABSTOP 4
#define RELAXED_EMPHASIS 1
#define DL_TAG_EXTENSION 1
#define PANDOC_HEADER 1
#define PATH_SED "/usr/bin/sed"

#endif/* __AC_MARKDOWN_D */
