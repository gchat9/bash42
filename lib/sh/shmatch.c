/*
 * shmatch.c -- shell interface to posix regular expression matching.
 */

/* Copyright (C) 2003 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if defined (HAVE_POSIX_REGEXP)

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "bashansi.h"

#include <stdio.h>
#include <regex.h>

#include "shell.h"
#include "variables.h"
#include "externs.h"

extern int glob_ignore_case, match_ignore_case;

#if defined (__dietlibc__)
/* dietlibc's regex has two gaps, both fixed here before regcomp():
   - POSIX character classes ([[:alpha:]] and the like) never match.  Spell
     them out as ASCII ranges.
   - regex_t.re_nsub is left at 0, which hides every capture group from
     BASH_REMATCH.  Count the groups: unescaped "(" outside brackets. */
static const struct { const char *name; const char *ranges; } dietlibc_posix_classes[] = {
  { "alpha", "a-zA-Z" }, { "digit", "0-9" }, { "alnum", "a-zA-Z0-9" },
  { "upper", "A-Z" }, { "lower", "a-z" }, { "xdigit", "0-9A-Fa-f" },
  { "space", " \t\n\r\f\v" }, { "blank", " \t" }, { "punct", "!-/:-@[-`{-~" },
  { "print", " -~" }, { "graph", "!-~" }, { "cntrl", "\001-\037\177" }
};
#define DIETLIBC_NCLASSES (sizeof (dietlibc_posix_classes) / sizeof (dietlibc_posix_classes[0]))

static char *
dietlibc_regex_pattern (pattern)
     const char *pattern;
{
  char *out, *o;
  const char *p, *r;
  size_t i, n;
  int found;

  out = (char *)malloc (strlen (pattern) * 2 + 1);
  o = out;
  for (p = pattern; *p; )
    {
      if (*p == '\\' && p[1])
	{
	  *o++ = *p++;
	  *o++ = *p++;
	  continue;
	}
      found = 0;
      if (p[0] == '[' && p[1] == ':')
	for (i = 0; i < DIETLIBC_NCLASSES; i++)
	  {
	    n = strlen (dietlibc_posix_classes[i].name);
	    if (strncmp (p + 2, dietlibc_posix_classes[i].name, n) == 0 &&
		p[2 + n] == ':' && p[3 + n] == ']')
	      {
		for (r = dietlibc_posix_classes[i].ranges; *r; r++)
		  *o++ = *r;
		p += 4 + n;
		found = 1;
		break;
	      }
	  }
      if (found == 0)
	*o++ = *p++;
    }
  *o = '\0';
  return out;
}

static size_t
dietlibc_count_subexpressions (pattern)
     const char *pattern;
{
  size_t n;
  const char *p;

  n = 0;
  for (p = pattern; *p; p++)
    {
      if (*p == '\\')
	p += (p[1] != '\0');
      else if (*p == '[')
	{
	  p++;
	  if (*p == '^')
	    p++;
	  if (*p == ']')
	    p++;
	  while (*p && *p != ']')
	    p++;
	  if (*p == '\0')
	    break;
	}
      else if (*p == '(')
	n++;
    }
  return n;
}
#endif /* __dietlibc__ */

int
sh_regmatch (string, pattern, flags)
     const char *string;
     const char *pattern;
     int flags;
{
  regex_t regex = { 0 };
  regmatch_t *matches;
  char *cpattern;
  int rflags;
#if defined (ARRAY_VARS)
  SHELL_VAR *rematch;
  ARRAY *amatch;
  int subexp_ind;
  char *subexp_str;
  int subexp_len;
#endif
  int result;

#if defined (ARRAY_VARS)
  rematch = (SHELL_VAR *)NULL;
#endif

  rflags = REG_EXTENDED;
  if (glob_ignore_case || match_ignore_case)
    rflags |= REG_ICASE;
#if !defined (ARRAY_VARS)
  rflags |= REG_NOSUB;
#endif

#if defined (__dietlibc__)
  cpattern = dietlibc_regex_pattern (pattern);
#else
  cpattern = (char *)pattern;
#endif

  if (regcomp (&regex, cpattern, rflags))
    {
#if defined (__dietlibc__)
      free (cpattern);
#endif
      return 2;		/* flag for printing a warning here. */
    }

#if defined (__dietlibc__)
  regex.re_nsub = dietlibc_count_subexpressions (cpattern);
  free (cpattern);
#endif

#if defined (ARRAY_VARS)
  matches = (regmatch_t *)malloc (sizeof (regmatch_t) * (regex.re_nsub + 1));
#else
  matches = NULL;
#endif

  if (regexec (&regex, string, regex.re_nsub + 1, matches, 0))
    result = EXECUTION_FAILURE;
  else
    result = EXECUTION_SUCCESS;		/* match */

#if defined (ARRAY_VARS)
  subexp_len = strlen (string) + 10;
  subexp_str = malloc (subexp_len + 1);

  /* Store the parenthesized subexpressions in the array BASH_REMATCH.
     Element 0 is the portion that matched the entire regexp.  Element 1
     is the part that matched the first subexpression, and so on. */
  unbind_variable ("BASH_REMATCH");
  rematch = make_new_array_variable ("BASH_REMATCH");
  amatch = array_cell (rematch);

  if ((flags & SHMAT_SUBEXP) && result == EXECUTION_SUCCESS && subexp_str)
    {
      for (subexp_ind = 0; subexp_ind <= regex.re_nsub; subexp_ind++)
	{
	  memset (subexp_str, 0, subexp_len);
	  strncpy (subexp_str, string + matches[subexp_ind].rm_so,
		     matches[subexp_ind].rm_eo - matches[subexp_ind].rm_so);
	  array_insert (amatch, subexp_ind, subexp_str);
	}
    }

  VSETATTR (rematch, att_readonly);

  free (subexp_str);
  free (matches);
#endif /* ARRAY_VARS */

  regfree (&regex);

  return result;
}

#endif /* HAVE_POSIX_REGEXP */
