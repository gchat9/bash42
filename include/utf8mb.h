/* UTF-8 multibyte conversion for dietlibc builds.

   dietlibc's locale support is a stub: MB_CUR_MAX is 5, so readline and bash
   take their multibyte paths, but mbrtowc() decodes each byte of a UTF-8
   sequence as its own Latin-1 character.  "é" (C3 A9) becomes two characters,
   each one column wide, and line editing goes wrong.  These functions decode
   and encode UTF-8 only, and are stateless (the mbstate_t argument is
   accepted and ignored).  config-bot.h redirects the libc names to them under
   __dietlibc__; their definitions are in utf8mb.c, included from general.c. */
#ifndef UTF8MB_H
#define UTF8MB_H

#include <stddef.h>
#include <wchar.h>

extern size_t utf8_mbrtowc (wchar_t *, const char *, size_t, mbstate_t *);
extern size_t utf8_mbrlen (const char *, size_t, mbstate_t *);
extern size_t utf8_wcrtomb (char *, wchar_t, mbstate_t *);
extern size_t utf8_mbsrtowcs (wchar_t *, const char **, size_t, mbstate_t *);
extern size_t utf8_wcsrtombs (char *, const wchar_t **, size_t, mbstate_t *);
extern size_t utf8_mbstowcs (wchar_t *, const char *, size_t);
extern size_t utf8_wcstombs (char *, const wchar_t *, size_t);
extern int utf8_mblen (const char *, size_t);
extern int utf8_mbtowc (wchar_t *, const char *, size_t);
extern int utf8_wctomb (char *, wchar_t);
extern int utf8_mbsinit (const mbstate_t *);

#endif /* UTF8MB_H */
