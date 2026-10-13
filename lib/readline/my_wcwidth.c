/* Minimal wcwidth() replacement for dietlibc, whose wcwidth() is a stub.
   Single definition: #included once, from lib/readline/mbutil.c (no Makefile
   change needed).

   Default build recognizes only: emoji (double width), and the generic
   zero-width characters emoji text needs (ZWJ/ZWSP, variation selectors,
   skin-tone modifiers, common combining diacritics).

   Optional ranges, enabled at compile time (e.g. via CFLAGS, or by
   uncommenting the lines below):
     MY_WCWIDTH_CJK               CJK, Hangul, fullwidth forms (double width)
     MY_WCWIDTH_ARABIC_HEBREW_THAI  combining marks of those scripts (zero width)
*/

/* #define MY_WCWIDTH_CJK */
/* #define MY_WCWIDTH_ARABIC_HEBREW_THAI */

/* All tables are sorted and non-overlapping.  BMP tables and the plane-1
   tables store 16-bit values (4 bytes per range); plane-1 entries hold
   the low 16 bits (codepoint - 0x10000). */
struct my_wcw_r16 { unsigned short lo, hi; };

/* ---- zero width, BMP ---- */
static const struct my_wcw_r16 my_wcw_zero16[] = {
  {0x0300,0x036F},{0x0483,0x0489},
#ifdef MY_WCWIDTH_ARABIC_HEBREW_THAI
  {0x0591,0x05BD},{0x05BF,0x05BF},{0x05C1,0x05C2},{0x05C4,0x05C5},
  {0x05C7,0x05C7},{0x0610,0x061A},{0x064B,0x065F},{0x0670,0x0670},
  {0x06D6,0x06DC},{0x06DF,0x06E4},
  {0x0E31,0x0E31},{0x0E34,0x0E3A},{0x0E47,0x0E4E},
#endif
  {0x1AB0,0x1AFF},{0x1DC0,0x1DFF},{0x200B,0x200F},{0x2028,0x202E},
  {0x2060,0x2064},{0x20D0,0x20FF},{0xFE00,0xFE0F},{0xFE20,0xFE2F},
  {0xFEFF,0xFEFF}
};

/* ---- double width, BMP ---- */
static const struct my_wcw_r16 my_wcw_wide16[] = {
#ifdef MY_WCWIDTH_CJK
  {0x1100,0x115F},
#endif
  {0x231A,0x231B},{0x23E9,0x23EC},{0x23F0,0x23F0},
  {0x23F3,0x23F3},{0x25FD,0x25FE},{0x2614,0x2615},{0x2648,0x2653},
  {0x267F,0x267F},{0x2693,0x2693},{0x26A1,0x26A1},{0x26AA,0x26AB},
  {0x26BD,0x26BE},{0x26C4,0x26C5},{0x26CE,0x26CE},{0x26D4,0x26D4},
  {0x26EA,0x26EA},{0x26F2,0x26F3},{0x26F5,0x26F5},{0x26FA,0x26FA},
  {0x26FD,0x26FD},{0x2705,0x2705},{0x270A,0x270B},{0x2728,0x2728},
  {0x274C,0x274C},{0x274E,0x274E},{0x2753,0x2755},{0x2757,0x2757},
  {0x2795,0x2797},{0x27B0,0x27B0},{0x27BF,0x27BF},{0x2B1B,0x2B1C},
  {0x2B50,0x2B50},{0x2B55,0x2B55}
#ifdef MY_WCWIDTH_CJK
  ,{0x2E80,0x303E},{0x3041,0x33FF},{0x3400,0x4DBF},{0x4E00,0x9FFF},
  {0xA000,0xA4CF},{0xAC00,0xD7A3},{0xF900,0xFAFF},{0xFE30,0xFE6F},
  {0xFF00,0xFF60},{0xFFE0,0xFFE6}
#endif
};

/* ---- plane 1 (U+1xxxx), stored as codepoint - 0x10000 ---- */
static const struct my_wcw_r16 my_wcw_zero_p1[] = {
  {0xF3FB,0xF3FF}                       /* U+1F3FB..1F3FF skin-tone modifiers */
};
static const struct my_wcw_r16 my_wcw_wide_p1[] = {
  {0xF300,0xF64F},                      /* U+1F300..1F64F */
  {0xF680,0xF6FC},                      /* U+1F680..1F6FC (1F6FD..1F6FF unassigned) */
  {0xF90C,0xF9FF},                      /* U+1F90C..1F9FF (1F900..1F90B not emoji) */
  {0xFA70,0xFAFA}                       /* U+1FA70..1FAFA (1FAFB..1FAFF unassigned) */
};

#define MY_WCW_N(t) ((int)(sizeof (t) / sizeof ((t)[0])))

/* Keep exactly one copy of the search loop: GCC would otherwise inline or
   clone it per call site, costing more code than the tables save. */
#if defined (__GNUC__)
__attribute__ ((noinline))
#endif
static int
my_wcw_in (unsigned int c, const struct my_wcw_r16 *t, int n)
{
  int lo = 0, hi = n - 1;
  while (lo <= hi)
    {
      int mid = (lo + hi) / 2;
      if (c > t[mid].hi) lo = mid + 1;
      else if (c < t[mid].lo) hi = mid - 1;
      else return 1;
    }
  return 0;
}

/* Takes int rather than wchar_t so no <wchar.h> is needed by the header. */
int
my_wcwidth (int wc)
{
  unsigned int c = (unsigned int)wc;
  const struct my_wcw_r16 *zt, *wt;
  int nz, nw;

  if (c == 0) return 0;
  if (c < 32 || (c >= 0x7F && c < 0xA0)) return -1;
  if (c < 0x300) return 1;

  if (c < 0x10000)
    {
      zt = my_wcw_zero16; nz = MY_WCW_N (my_wcw_zero16);
      wt = my_wcw_wide16; nw = MY_WCW_N (my_wcw_wide16);
    }
  else if (c < 0x20000)
    {
      c &= 0xFFFF;
      zt = my_wcw_zero_p1; nz = MY_WCW_N (my_wcw_zero_p1);
      wt = my_wcw_wide_p1; nw = MY_WCW_N (my_wcw_wide_p1);
    }
  else
    {
#ifdef MY_WCWIDTH_CJK
      if (c <= 0x3FFFD) return 2;       /* CJK extensions B.. (planes 2-3) */
#endif
      if (c >= 0xE0100 && c <= 0xE01EF) return 0;  /* variation selectors suppl. */
      return 1;
    }

  if (my_wcw_in (c, zt, nz)) return 0;
  if (my_wcw_in (c, wt, nw)) return 2;
  return 1;
}
