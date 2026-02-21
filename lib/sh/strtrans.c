/* strtrans.c - Translate and untranslate strings with ANSI-C escape sequences. */

/* Copyright (C) 2000-2010 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Bash is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Bash.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <bashansi.h>
#include <stdio.h>
#include <chartypes.h>
#include <string.h>

#include "shell.h"

#ifdef ESC
#undef ESC
#endif
#define ESC '\033'	/* ASCII */

#if defined (HANDLE_MULTIBYTE)
static int
st_is_utf8_locale (void)
{
  const char *loc;
  const char *dot;

  loc = getenv ("LC_ALL");
  if (loc == 0 || *loc == 0)
    loc = getenv ("LC_CTYPE");
  if (loc == 0 || *loc == 0)
    loc = getenv ("LANG");
  if (loc == 0 || *loc == 0)
    return 0;

  if (STREQ (loc, "UTF-8") || STREQ (loc, "utf8"))
    return 1;

  dot = strrchr (loc, '.');
  if (dot && dot[1])
    {
      dot++;
      if ((dot[0] == 'U' || dot[0] == 'u') &&
	  (dot[1] == 'T' || dot[1] == 't') &&
	  (dot[2] == 'F' || dot[2] == 'f') &&
	  dot[3] == '-' &&
	  dot[4] == '8' &&
	  dot[5] == '\0')
	return 1;
      if ((dot[0] == 'U' || dot[0] == 'u') &&
	  (dot[1] == 'T' || dot[1] == 't') &&
	  (dot[2] == 'F' || dot[2] == 'f') &&
	  dot[3] == '8' &&
	  dot[4] == '\0')
	return 1;
    }

  return 0;
}

static int
st_utf8_mblen (s, n)
     const char *s;
     size_t n;
{
  unsigned char c, c1, c2, c3;

  if (s == 0 || n == 0)
    return -1;

  c = (unsigned char)s[0];
  if (c < 0x80)
    return 1;
  if (c < 0xC2)
    return -1;
  if (c < 0xE0)
    {
      if (n < 2)
	return -2;
      c1 = (unsigned char)s[1];
      return ((c1 & 0xC0) == 0x80) ? 2 : -1;
    }
  if (c < 0xF0)
    {
      if (n < 3)
	return -2;
      c1 = (unsigned char)s[1];
      c2 = (unsigned char)s[2];
      if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
	return -1;
      if ((c == 0xE0 && c1 < 0xA0) || (c == 0xED && c1 >= 0xA0))
	return -1;
      return 3;
    }
  if (c < 0xF5)
    {
      if (n < 4)
	return -2;
      c1 = (unsigned char)s[1];
      c2 = (unsigned char)s[2];
      c3 = (unsigned char)s[3];
      if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
	return -1;
      if ((c == 0xF0 && c1 < 0x90) || (c == 0xF4 && c1 >= 0x90))
	return -1;
      return 4;
    }
  return -1;
}
#endif /* HANDLE_MULTIBYTE */

/* Convert STRING by expanding the escape sequences specified by the
   ANSI C standard.  If SAWC is non-null, recognize `\c' and use that
   as a string terminator.  If we see \c, set *SAWC to 1 before
   returning.  LEN is the length of STRING.  If (FLAGS&1) is non-zero,
   that we're translating a string for `echo -e', and therefore should not
   treat a single quote as a character that may be escaped with a backslash.
   If (FLAGS&2) is non-zero, we're expanding for the parser and want to
   quote CTLESC and CTLNUL with CTLESC.  If (flags&4) is non-zero, we want
   to remove the backslash before any unrecognized escape sequence. */
char *
ansicstr (string, len, flags, sawc, rlen)
     char *string;
     int len, flags, *sawc, *rlen;
{
  int c, temp;
  char *ret, *r, *s;
  unsigned long v;

  if (string == 0 || *string == '\0')
    return ((char *)NULL);

#if defined (HANDLE_MULTIBYTE)
  ret = (char *)xmalloc (4*len + 1);
#else
  ret = (char *)xmalloc (2*len + 1);	/* 2*len for possible CTLESC */
#endif
  for (r = ret, s = string; s && *s; )
    {
      c = *s++;
      if (c != '\\' || *s == '\0')
	*r++ = c;
      else
	{
	  switch (c = *s++)
	    {
#if defined (__STDC__)
	    case 'a': c = '\a'; break;
	    case 'v': c = '\v'; break;
#else
	    case 'a': c = '\007'; break;
	    case 'v': c = (int) 0x0B; break;
#endif
	    case 'b': c = '\b'; break;
	    case 'e': case 'E':		/* ESC -- non-ANSI */
	      c = ESC; break;
	    case 'f': c = '\f'; break;
	    case 'n': c = '\n'; break;
	    case 'r': c = '\r'; break;
	    case 't': c = '\t'; break;
	    case '1': case '2': case '3':
	    case '4': case '5': case '6':
	    case '7':
#if 1
	      if (flags & 1)
		{
		  *r++ = '\\';
		  break;
		}
	    /*FALLTHROUGH*/
#endif
	    case '0':
	      /* If (FLAGS & 1), we're translating a string for echo -e (or
		 the equivalent xpg_echo option), so we obey the SUSv3/
		 POSIX-2001 requirement and accept 0-3 octal digits after
		 a leading `0'. */
	      temp = 2 + ((flags & 1) && (c == '0'));
	      for (c -= '0'; ISOCTAL (*s) && temp--; s++)
		c = (c * 8) + OCTVALUE (*s);
	      c &= 0xFF;
	      break;
	    case 'x':			/* Hex digit -- non-ANSI */
	      if ((flags & 2) && *s == '{')
		{
		  flags |= 16;		/* internal flag value */
		  s++;
		}
	      /* Consume at least two hex characters */
	      for (temp = 2, c = 0; ISXDIGIT ((unsigned char)*s) && temp--; s++)
		c = (c * 16) + HEXVALUE (*s);
	      /* DGK says that after a `\x{' ksh93 consumes ISXDIGIT chars
		 until a non-xdigit or `}', so potentially more than two
		 chars are consumed. */
	      if (flags & 16)
		{
		  for ( ; ISXDIGIT ((unsigned char)*s); s++)
		    c = (c * 16) + HEXVALUE (*s);
		  flags &= ~16;
		  if (*s == '}')
		    s++;
	        }
	      /* \x followed by non-hex digits is passed through unchanged */
	      else if (temp == 2)
		{
		  *r++ = '\\';
		  c = 'x';
		}
	      c &= 0xFF;
	      break;
#if defined (HANDLE_MULTIBYTE)
	    case 'u':
	    case 'U':
	      temp = (c == 'u') ? 4 : 8;	/* \uNNNN \UNNNNNNNN */
	      for (v = 0; ISXDIGIT ((unsigned char)*s) && temp--; s++)
		v = (v * 16) + HEXVALUE (*s);
	      if (temp == ((c == 'u') ? 4 : 8))
		{
		  *r++ = '\\';	/* c remains unchanged */
		  break;
		}
	      else if (v <= UCHAR_MAX)
		{
		  c = v;
		  break;
		}
	      else
		{
		  temp = u32cconv (v, r);
		  r += temp;
		  continue;
		}
#endif
	    case '\\':
	      break;
	    case '\'': case '"': case '?':
	      if (flags & 1)
		*r++ = '\\';
	      break;
	    case 'c':
	      if (sawc)
		{
		  *sawc = 1;
		  *r = '\0';
		  if (rlen)
		    *rlen = r - ret;
		  return ret;
		}
	      else if ((flags & 1) == 0 && *s == 0)
		;		/* pass \c through */
	      else if ((flags & 1) == 0 && (c = *s))
		{
		  s++;
		  if ((flags & 2) && c == '\\' && c == *s)
		    s++;	/* Posix requires $'\c\\' do backslash escaping */
		  c = TOCTRL(c);
		  break;
		}
		/*FALLTHROUGH*/
	    default:
		if ((flags & 4) == 0)
		  *r++ = '\\';
		break;
	    }
	  if ((flags & 2) && (c == CTLESC || c == CTLNUL))
	    *r++ = CTLESC;
	  *r++ = c;
	}
    }
  *r = '\0';
  if (rlen)
    *rlen = r - ret;
  return ret;
}

/* Take a string STR, possibly containing non-printing characters, and turn it
   into a $'...' ANSI-C style quoted string.  Returns a new string. */
char *
ansic_quote (str, flags, rlen)
     char *str;
     int flags, *rlen;
{
  char *r, *ret, *s;
  int l, rsize;
  unsigned char c;

  if (str == 0 || *str == 0)
    return ((char *)0);

  l = strlen (str);
  rsize = 4 * l + 4;
  r = ret = (char *)xmalloc (rsize);

  *r++ = '$';
  *r++ = '\'';

  for (s = str, l = 0; *s; s++)
    {
      c = *s;
      l = 1;		/* 1 == add backslash; 0 == no backslash */
      switch (c)
	{
	case ESC: c = 'E'; break;
#ifdef __STDC__
	case '\a': c = 'a'; break;
	case '\v': c = 'v'; break;
#else
	case '\007': c = 'a'; break;
	case 0x0b: c = 'v'; break;
#endif

	case '\b': c = 'b'; break;
	case '\f': c = 'f'; break;
	case '\n': c = 'n'; break;
	case '\r': c = 'r'; break;
	case '\t': c = 't'; break;
	case '\\':
	case '\'':
	  break;
	default:
	  if (ISPRINT (c) == 0)
	    {
	      *r++ = '\\';
	      *r++ = TOCHAR ((c >> 6) & 07);
	      *r++ = TOCHAR ((c >> 3) & 07);
	      *r++ = TOCHAR (c & 07);
	      continue;
	    }
	  l = 0;
	  break;
	}
      if (l)
	*r++ = '\\';
      *r++ = c;
    }

  *r++ = '\'';
  *r = '\0';
  if (rlen)
    *rlen = r - ret;
  return ret;
}

/* return 1 if we need to quote with $'...' because of non-printing chars. */
int
ansic_shouldquote (string)
     const char *string;
{
  const char *s;
  unsigned char c;
#if defined (HANDLE_MULTIBYTE)
  int l;
  int utf8locale;
#endif

  if (string == 0)
    return 0;

#if defined (HANDLE_MULTIBYTE)
  utf8locale = st_is_utf8_locale ();
#endif

  for (s = string; c = *s; )
    {
#if defined (HANDLE_MULTIBYTE)
      if (utf8locale && (c & 0x80))
	{
	  l = st_utf8_mblen (s, strlen (s));
	  if (l < 0)
	    return 1;
	  s += l;
	  continue;
	}
#endif
      if (ISPRINT (c) == 0)
	return 1;
      s++;
    }

  return 0;
}

/* $'...' ANSI-C expand the portion of STRING between START and END and
   return the result.  The result cannot be longer than the input string. */
char *
ansiexpand (string, start, end, lenp)
     char *string;
     int start, end, *lenp;
{
  char *temp, *t;
  int len, tlen;

  temp = (char *)xmalloc (end - start + 1);
  for (tlen = 0, len = start; len < end; )
    temp[tlen++] = string[len++];
  temp[tlen] = '\0';

  if (*temp)
    {
      t = ansicstr (temp, tlen, 2, (int *)NULL, lenp);
      free (temp);
      return (t);
    }
  else
    {
      if (lenp)
	*lenp = 0;
      return (temp);
    }
}
