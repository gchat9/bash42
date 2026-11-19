/* utf8mb.c - UTF-8 multibyte conversion for dietlibc (see include/utf8mb.h).
   Included from general.c, so the definitions are in the main program and
   every library linked into bash can call them.  Stateless: the mbstate_t
   argument is ignored, and each call is given the bytes it needs. */

#include <errno.h>
#include "include/utf8mb.h"

/* Decode one character from s (at most n bytes).  Returns its length in
   bytes (0 for NUL), (size_t)-2 if s ends in the middle of a character, or
   (size_t)-1 with errno = EILSEQ for invalid input. */
size_t
utf8_mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
  const unsigned char *p = (const unsigned char *) s;
  unsigned long cp;
  size_t len, i;

  (void) ps;
  if (s == 0)
    return 0;
  if (n == 0)
    return (size_t) -2;
  if (p[0] < 0x80)
    {
      if (pwc)
	*pwc = p[0];
      return p[0] ? 1 : 0;
    }

  if (p[0] >= 0xC2 && p[0] <= 0xDF)
    { len = 2; cp = p[0] & 0x1F; }
  else if (p[0] >= 0xE0 && p[0] <= 0xEF)
    { len = 3; cp = p[0] & 0x0F; }
  else if (p[0] >= 0xF0 && p[0] <= 0xF4)
    { len = 4; cp = p[0] & 0x07; }
  else
    { errno = EILSEQ; return (size_t) -1; }

  for (i = 1; i < len; i++)
    {
      if (i >= n)
	return (size_t) -2;
      if ((p[i] & 0xC0) != 0x80)
	{ errno = EILSEQ; return (size_t) -1; }
      cp = (cp << 6) | (p[i] & 0x3F);
    }

  /* Reject overlong forms, surrogates and values past U+10FFFF. */
  if ((len == 3 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) ||
      (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)))
    { errno = EILSEQ; return (size_t) -1; }

  if (pwc)
    *pwc = (wchar_t) cp;
  return len;
}

size_t
utf8_mbrlen (const char *s, size_t n, mbstate_t *ps)
{
  return utf8_mbrtowc ((wchar_t *) 0, s, n, ps);
}

/* Encode wc into s (at least 4 bytes).  Returns the number of bytes, or
   (size_t)-1 with errno = EILSEQ.  s == NULL asks for the length of the
   reset sequence, which is 1. */
size_t
utf8_wcrtomb (char *s, wchar_t wc, mbstate_t *ps)
{
  unsigned long c = (unsigned long) wc;
  size_t n, i;

  (void) ps;
  if (s == 0)
    return 1;
  if (c < 0x80)
    { s[0] = (char) c; return 1; }
  if (c < 0x800)
    n = 2;
  else if (c >= 0xD800 && c <= 0xDFFF)
    { errno = EILSEQ; return (size_t) -1; }
  else if (c < 0x10000)
    n = 3;
  else if (c <= 0x10FFFF)
    n = 4;
  else
    { errno = EILSEQ; return (size_t) -1; }

  for (i = n - 1; i > 0; i--)
    {
      s[i] = (char) (0x80 | (c & 0x3F));
      c >>= 6;
    }
  s[0] = (char) ((n == 2 ? 0xC0 : n == 3 ? 0xE0 : 0xF0) | c);
  return n;
}

size_t
utf8_mbsrtowcs (wchar_t *dest, const char **src, size_t len, mbstate_t *ps)
{
  const char *p = *src;
  size_t count = 0, r;
  wchar_t wc;

  for (;;)
    {
      if (dest && count == len)
	break;
      r = utf8_mbrtowc (&wc, p, 4, ps);
      if (r == (size_t) -1 || r == (size_t) -2)
	return r;
      if (r == 0)
	{
	  if (dest)
	    { dest[count] = 0; *src = 0; }
	  return count;
	}
      if (dest)
	dest[count] = wc;
      p += r;
      count++;
    }
  if (dest)
    *src = p;
  return count;
}

size_t
utf8_wcsrtombs (char *dest, const wchar_t **src, size_t len, mbstate_t *ps)
{
  const wchar_t *w = *src;
  size_t count = 0, r;
  char tmp[4];

  for (;;)
    {
      if (*w == 0)
	{
	  if (dest)
	    {
	      if (count < len)
		{ dest[count] = 0; *src = 0; }
	      else
		*src = w;
	    }
	  return count;
	}
      r = utf8_wcrtomb (tmp, *w, ps);
      if (r == (size_t) -1)
	return r;
      if (dest)
	{
	  size_t i;
	  if (count + r > len)
	    break;
	  for (i = 0; i < r; i++)
	    dest[count + i] = tmp[i];
	}
      count += r;
      w++;
    }
  if (dest)
    *src = w;
  return count;
}

size_t
utf8_mbstowcs (wchar_t *dest, const char *src, size_t n)
{
  const char *p = src;
  return utf8_mbsrtowcs (dest, &p, n, (mbstate_t *) 0);
}

size_t
utf8_wcstombs (char *dest, const wchar_t *src, size_t n)
{
  const wchar_t *p = src;
  return utf8_wcsrtombs (dest, &p, n, (mbstate_t *) 0);
}

int
utf8_mblen (const char *s, size_t n)
{
  size_t r;

  if (s == 0)
    return 0;
  r = utf8_mbrtowc ((wchar_t *) 0, s, n, (mbstate_t *) 0);
  return (r == (size_t) -1 || r == (size_t) -2) ? -1 : (int) r;
}

int
utf8_mbtowc (wchar_t *pwc, const char *s, size_t n)
{
  size_t r;

  if (s == 0)
    return 0;
  r = utf8_mbrtowc (pwc, s, n, (mbstate_t *) 0);
  return (r == (size_t) -1 || r == (size_t) -2) ? -1 : (int) r;
}

int
utf8_wctomb (char *s, wchar_t wc)
{
  size_t r;

  if (s == 0)
    return 0;
  r = utf8_wcrtomb (s, wc, (mbstate_t *) 0);
  return (r == (size_t) -1) ? -1 : (int) r;
}

int
utf8_mbsinit (const mbstate_t *ps)
{
  (void) ps;
  return 1;
}
