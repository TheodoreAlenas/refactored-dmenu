/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

static int topbar = 0;                      /* -b  option; if 0, dmenu appears at bottom     */
/* -fn option overrides fonts[0]; default X11 font or font set */
static const char *fonts[] = {
	"Source Code Pro:size=18"
};
static const char *prompt      = NULL;      /* -p  option; prompt to the left of input field */

/* enums */
enum { SchemeNorm, SchemeOdd, SchemeSel, SchemeOut, SchemeLast }; /* color schemes */
enum Palette { PaletteBlue, PaletteRed, PaletteLast }; /* color palettes */
#define NUM_OF_SCHEMES 2

/* foreground first, background second */
static const char *palettes[PaletteLast][SchemeLast][2]  = {
  [PaletteBlue] = {
    [SchemeNorm] = { "#d9e5f1", "#0d1925" },
    [SchemeOdd] = { "#d9e5f1", "#0f1b29" },
    [SchemeSel] = { "#eeeeee", "#005577" },
    [SchemeOut] = { "#000000", "#00ffff" },
  },
  [PaletteRed] = {
    [SchemeNorm] = { "#ff4455", "#0a0a0f" },
    [SchemeOdd] = { "#ff4455", "#0d0d13" },
    [SchemeSel] = { "#ffeeaa", "#aa1111" },
    [SchemeOut] = { "#000000", "#00ffff" },
  }
};

/* -l option; if nonzero, dmenu uses vertical list with given number of lines */
static unsigned int lines      = 20;

/*
 * Characters not considered part of a word while deleting words
 * for example: " /?\"&[]"
 */
static const char worddelimiters[] = " \t/.,!@#$%^&*(){}[]/=?+-_";
