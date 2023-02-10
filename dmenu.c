/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"
#include "get-suggested-width.h"

/* macros */
#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                             * MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTW(X)              (drw_fontset_getwidth(drw, (X)) + lrpad)

struct item {
  char *text;
  struct item *left, *right;
  int out;
};

static char text[BUFSIZ] = "";
static char *embed;
static int bh, mw, mh;
static int edgeoffset = -1;
static int columns = -1;
static int defaultitempos = 0;
static int inputw = 500, promptw;
static int lrpad; /* sum of left and right padding */
static size_t cursor;
static struct item *items = NULL;
static struct item *matches, *matchend;
static struct item *prev, *curr, *next, *sel;
static int mon = -1, screen;

static Atom clip, utf8;
static Display *dpy;
static Window root, parentwin, win;
static XIC xic;

static Drw *drw;

#include "config.h"
static Clr *scheme[SchemeLast];
static enum Palette palette = PaletteBlue;

static int (*fstrncmp)(const char *, const char *, size_t) = strncmp;
static char *(*fstrstr)(const char *, const char *) = strstr;

static unsigned int
textw_clamp(const char *str, unsigned int n)
{
  unsigned int w = drw_fontset_getwidth_clamp(drw, str, n) + lrpad;
  return MIN(w, n);
}

void
repeat_for_all_item_widths (FunctionToRepeat f, void *extra_arguments)
{
  if (!items)
    return;

  for (struct item *item = items; item->text; item++)
    f(TEXTW(item->text), extra_arguments);
}

int
get_suggested_width()
{
  return get_typical_width(10, repeat_for_all_item_widths);
}

static void
appenditem(struct item *item, struct item **list, struct item **last)
{
  if (*last)
    (*last)->right = item;
  else
    *list = item;

  item->left = *last;
  item->right = NULL;
  *last = item;
}

static void
calcoffsets(void)
{
  int i, n;
  if (columns < 0)
    die("columns is still negative when calling calcoffsets");

  if (lines > 0)
    n = lines * bh;
  else
    n = mw - (promptw + inputw + TEXTW("<") + TEXTW(">"));
  /* calculate which items will begin the next page and previous page */
  for (i = 0, next = curr; next; next = next->right)
    if ((i += (lines > 0) ? bh : textw_clamp(next->text, n)) > n * columns)
      break;
  for (i = 0, prev = curr; prev && prev->left; prev = prev->left)
    if ((i += (lines > 0) ? bh : textw_clamp(prev->left->text, n)) > n * columns)
      break;
}

static void
cleanup(void)
{
  size_t i;

  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  for (i = 0; i < SchemeLast; i++)
    free(scheme[i]);
  for (i = 0; items && items[i].text; ++i)
    free(items[i].text);
  free(items);
  drw_free(drw);
  XSync(dpy, False);
  XCloseDisplay(dpy);
}

static char *
cistrstr(const char *h, const char *n)
{
  size_t i;

  if (!n[0])
    return (char *)h;

  for (; *h; ++h) {
    for (i = 0; n[i] && tolower((unsigned char)n[i]) ==
                tolower((unsigned char)h[i]); ++i)
      ;
    if (n[i] == '\0')
      return (char *)h;
  }
  return NULL;
}

static int
drawitem(struct item *item, int x, int y, int w, int is_odd)
{
  if (item == sel)
    drw_setscheme(drw, scheme[SchemeSel]);
  else if (item->out)
    drw_setscheme(drw, scheme[SchemeOut]);
  else if (is_odd)
    drw_setscheme(drw, scheme[SchemeNorm]);
  else
    drw_setscheme(drw, scheme[SchemeOdd]);

  return drw_text(drw, x, y, w, bh, lrpad / 2, item->text, 0);
}

static void
drawgridinp(int starting_x, int starting_y, struct item *item)
{
  int left_most = starting_x, top_most = starting_y + bh;
  int col = 0, row = 0;
  int colw = (mw - left_most) / columns;

  for (item = curr; item != next; item = item->right) {
    if (row == lines) {
      row = 0;
      col += 1;
    }
    int x = left_most + colw * col;
    int y = top_most + bh * row;
    drawitem(item, x, y, colw, (col + row) % 2);
    row++;
  }
}

static void
drawhorizinp(int x, int w, struct item *item)
{
    x += inputw;
    w = TEXTW("<");
    if (curr->left) {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, "<", 0);
    }
    x += w;
    int i = 0;
    for (item = curr; item != next; item = item->right)
      x = drawitem(item, x, 0, textw_clamp(item->text, mw - x - TEXTW(">")), i++ % 2);
    if (next) {
      w = TEXTW(">");
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, mw - w, 0, w, bh, lrpad / 2, ">", 0);
    }
}

static void
drawmenu(void)
{
  unsigned int curpos;
  struct item *item;
  int x = 0, y = 0, w;

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, mw, mh, 1, 1);

  if (prompt && *prompt) {
    drw_setscheme(drw, scheme[SchemeSel]);
    x = drw_text(drw, x, 0, promptw, bh, lrpad / 2, prompt, 0);
  }
  /* draw input field */
  w = (lines > 0 || !matches) ? mw - x : inputw;
  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_text(drw, x, 0, w, bh, lrpad / 2, text, 0);

  curpos = TEXTW(text) - TEXTW(&text[cursor]);
  if ((curpos += lrpad / 2 - 1) < w) {
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_rect(drw, x + curpos, 2, 2, bh - 4, 1, 0);
  }

  if (lines > 0)
    drawgridinp(x, y, item);
  else if (matches)
    drawhorizinp(x, w, item);

  drw_map(drw, win, 0, 0, mw, mh);
}

static void
grabfocus(void)
{
  struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000  };
  Window focuswin;
  int i, revertwin;

  for (i = 0; i < 100; ++i) {
    XGetInputFocus(dpy, &focuswin, &revertwin);
    if (focuswin == win)
      return;
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    nanosleep(&ts, NULL);
  }
  die("cannot grab focus");
}

static void
grabkeyboard(void)
{
  struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000  };
  int i;

  if (embed)
    return;
  /* try to grab keyboard, we may have to wait for another process to ungrab */
  for (i = 0; i < 1000; i++) {
    if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync,
                      GrabModeAsync, CurrentTime) == GrabSuccess)
      return;
    nanosleep(&ts, NULL);
  }
  die("cannot grab keyboard");
}

static void
match(void)
{
  static char **tokv = NULL;
  static int tokn = 0;

  char buf[sizeof text], *s;
  int i, tokc = 0;
  size_t len, textsize;
  struct item *item, *lprefix, *lsubstr, *prefixend, *substrend;

  strcpy(buf, text);
  /* separate input text into tokens to be matched individually */
  for (s = strtok(buf, " "); s; tokv[tokc - 1] = s, s = strtok(NULL, " "))
    if (++tokc > tokn && !(tokv = realloc(tokv, ++tokn * sizeof *tokv)))
      die("cannot realloc %zu bytes:", tokn * sizeof *tokv);
  len = tokc ? strlen(tokv[0]) : 0;

  matches = lprefix = lsubstr = matchend = prefixend = substrend = NULL;
  textsize = strlen(text) + 1;
  for (item = items; item && item->text; item++) {
    for (i = 0; i < tokc; i++)
      if (!fstrstr(item->text, tokv[i]))
        break;
    if (i != tokc) /* not all tokens match */
      continue;
    /* exact matches go first, then prefixes, then substrings */
    if (!tokc || !fstrncmp(text, item->text, textsize))
      appenditem(item, &matches, &matchend);
    else if (!fstrncmp(tokv[0], item->text, len))
      appenditem(item, &lprefix, &prefixend);
    else
      appenditem(item, &lsubstr, &substrend);
  }
  if (lprefix) {
    if (matches) {
      matchend->right = lprefix;
      lprefix->left = matchend;
    } else
      matches = lprefix;
    matchend = prefixend;
  }
  if (lsubstr) {
    if (matches) {
      matchend->right = lsubstr;
      lsubstr->left = matchend;
    } else
      matches = lsubstr;
    matchend = substrend;
  }
  if (text[0])
    curr = sel = matches;
  else {
    curr = items;
    sel = items + defaultitempos;
  }
  calcoffsets();
}

static void
insert(const char *str, ssize_t n)
{
  if (strlen(text) + n > sizeof text - 1)
    return;
  /* move existing text out of the way, insert new text, and update cursor */
  memmove(&text[cursor + n], &text[cursor], sizeof text - cursor - MAX(n, 0));
  if (n > 0)
    memcpy(&text[cursor], str, n);
  cursor += n;
  match();
}

static size_t
nextrune(int inc)
{
  ssize_t n;

  /* return location of next utf8 rune in the given direction (+1 or -1) */
  for (n = cursor + inc; n + inc >= 0 && (text[n] & 0xc0) == 0x80; n += inc)
    ;
  return n;
}

static void
movewordedge(int dir)
{
  if (dir < 0) { /* move cursor to the start of the word*/
    while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
      cursor = nextrune(-1);
    while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
      cursor = nextrune(-1);
  } else { /* move cursor to the end of the word */
    while (text[cursor] && strchr(worddelimiters, text[cursor]))
      cursor = nextrune(+1);
    while (text[cursor] && !strchr(worddelimiters, text[cursor]))
      cursor = nextrune(+1);
  }
}

static void
keypress(XKeyEvent *ev)
{
  char buf[32];
  int len;
  KeySym ksym;
  Status status;

  len = XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
  switch (status) {
  default: /* XLookupNone, XBufferOverflow */
    return;
  case XLookupChars:
    goto insert;
  case XLookupKeySym:
  case XLookupBoth:
    break;
  }

  if (ev->state & ControlMask) {
    switch(ksym) {
    case XK_a: ksym = XK_Home;      break;
    case XK_b: ksym = XK_Left;      break;
    case XK_c: ksym = XK_Escape;    break;
    case XK_d: ksym = XK_Delete;    break;
    case XK_e: ksym = XK_End;       break;
    case XK_f: ksym = XK_Right;     break;
    case XK_g: ksym = XK_Escape;    break;
    case XK_h: ksym = XK_BackSpace; break;
    case XK_i: ksym = XK_Tab;       break;
    case XK_j: /* fallthrough */
    case XK_J: /* fallthrough */
    case XK_m: /* fallthrough */
    case XK_M: ksym = XK_Return; ev->state &= ~ControlMask; break;
    case XK_n: ksym = XK_Down;      break;
    case XK_p: ksym = XK_Up;        break;

    case XK_k: /* delete right */
      text[cursor] = '\0';
      match();
      break;
    case XK_u: /* delete left */
      insert(NULL, 0 - cursor);
      break;
    case XK_w: /* delete word */
      while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
        insert(NULL, nextrune(-1) - cursor);
      while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
        insert(NULL, nextrune(-1) - cursor);
      break;
    case XK_y: /* paste selection */
    case XK_Y:
      XConvertSelection(dpy, (ev->state & ShiftMask) ? clip : XA_PRIMARY,
                        utf8, utf8, win, CurrentTime);
      return;
    case XK_Left:
    case XK_KP_Left:
      movewordedge(-1);
      goto draw;
    case XK_Right:
    case XK_KP_Right:
      movewordedge(+1);
      goto draw;
    case XK_Return:
    case XK_KP_Enter:
      break;
    case XK_bracketleft:
      cleanup();
      exit(1);
    case XK_v: ksym = XK_Next; break;
    default:
      return;
    }
  } else if (ev->state & Mod1Mask) {
    switch(ksym) {
    case XK_b:
      movewordedge(-1);
      goto draw;
    case XK_f:
      movewordedge(+1);
      goto draw;
    case XK_g: ksym = XK_Home;  break;
    case XK_G: ksym = XK_End;   break;
    case XK_h: ksym = XK_Up;    break;
    case XK_v: ksym = XK_Prior; break;
    case XK_l: ksym = XK_Down;  break;
    default:
      return;
    }
  }

  switch(ksym) {
  default:
insert:
    if (!iscntrl((unsigned char)*buf))
      insert(buf, len);
    break;
  case XK_Delete:
  case XK_KP_Delete:
    if (text[cursor] == '\0')
      return;
    cursor = nextrune(+1);
    /* fallthrough */
  case XK_BackSpace:
    if (cursor == 0)
      return;
    insert(NULL, nextrune(-1) - cursor);
    break;
  case XK_End:
  case XK_KP_End:
    if (text[cursor] != '\0') {
      cursor = strlen(text);
      break;
    }
    if (next) {
      /* jump to end of list and position items in reverse */
      curr = matchend;
      calcoffsets();
      curr = prev;
      calcoffsets();
      while (next && (curr = curr->right))
        calcoffsets();
    }
    sel = matchend;
    break;
  case XK_Escape:
    cleanup();
    exit(1);
  case XK_Home:
  case XK_KP_Home:
    if (sel == matches) {
      cursor = 0;
      break;
    }
    sel = curr = matches;
    calcoffsets();
    break;
  case XK_Left:
  case XK_KP_Left:
    if (cursor > 0 && (!sel || !sel->left || lines > 0)) {
      cursor = nextrune(-1);
      break;
    }
    if (lines > 0)
      return;
    /* fallthrough */
  case XK_Up:
  case XK_KP_Up:
    if (sel && sel->left && (sel = sel->left)->right == curr) {
      curr = prev;
      calcoffsets();
    }
    break;
  case XK_Next:
  case XK_KP_Next:
    if (!next)
      return;
    sel = curr = next;
    calcoffsets();
    break;
  case XK_Prior:
  case XK_KP_Prior:
    if (!prev)
      return;
    sel = curr = prev;
    calcoffsets();
    break;
  case XK_Return:
  case XK_KP_Enter:
    puts((sel && !(ev->state & ShiftMask)) ? sel->text : text);
    if (!(ev->state & ControlMask)) {
      cleanup();
      exit(0);
    }
    if (sel)
      sel->out = 1;
    break;
  case XK_Right:
  case XK_KP_Right:
    if (text[cursor] != '\0') {
      cursor = nextrune(+1);
      break;
    }
    if (lines > 0)
      return;
    /* fallthrough */
  case XK_Down:
  case XK_KP_Down:
    if (sel && sel->right && (sel = sel->right) == next) {
      curr = next;
      calcoffsets();
    }
    break;
  case XK_Tab:
    if (!sel)
      return;
    cursor = strnlen(sel->text, sizeof text - 1);
    memcpy(text, sel->text, cursor);
    text[cursor] = '\0';
    match();
    break;
  }

draw:
  drawmenu();
}

static void
paste(void)
{
  char *p, *q;
  int di;
  unsigned long dl;
  Atom da;

  /* we have been given the current selection, now insert it into input */
  if (XGetWindowProperty(dpy, win, utf8, 0, (sizeof text / 4) + 1, False,
                     utf8, &da, &di, &dl, &dl, (unsigned char **)&p)
      == Success && p) {
    insert(p, (q = strchr(p, '\n')) ? q - p : (ssize_t)strlen(p));
    XFree(p);
  }
  drawmenu();
}

int
get_suggested_columns()
{
  int suggested_width = get_suggested_width();
  if (suggested_width == 0)
    return 1;

  int enough_fields = inputw / suggested_width;

  if (enough_fields > 4)
    return enough_fields;
  else if (suggested_width < 200)
    return 4;
  else if (suggested_width < 600)
    return 3;
  else if (suggested_width < 800)
    return 2;
  return 1;
}

static void
autosetcolumns()
{
  columns = 1;
  columns = get_suggested_columns();
}

unsigned int
calcneededlines(size_t inputs)
{
  if (inputs % columns)
    return inputs / columns + 1;
  return inputs / columns;
}

int
readstdingetnumlines(void)
{
  char *line = NULL;
  size_t i, junk, size = 0;
  ssize_t len;

  /* read each line from stdin and add it to the item list */
  for (i = 0; (len = getline(&line, &junk, stdin)) != -1; i++, line = NULL) {
    if (i + 1 >= size / sizeof *items)
      if (!(items = realloc(items, (size += BUFSIZ))))
        die("cannot realloc %zu bytes:", size);
    if (line[len - 1] == '\n')
      line[len - 1] = '\0';
    items[i].text = line;
    items[i].out = 0;
  }
  if (items)
    items[i].text = NULL;
  return i;
}

static void
run(void)
{
  XEvent ev;

  while (!XNextEvent(dpy, &ev)) {
    if (XFilterEvent(&ev, win))
      continue;
    switch(ev.type) {
    case DestroyNotify:
      if (ev.xdestroywindow.window != win)
        break;
      cleanup();
      exit(1);
    case Expose:
      if (ev.xexpose.count == 0)
        drw_map(drw, win, 0, 0, mw, mh);
      break;
    case FocusIn:
      /* regrab focus from parent window */
      if (ev.xfocus.window != win)
        grabfocus();
      break;
    case KeyPress:
      keypress(&ev.xkey);
      break;
    case SelectionNotify:
      if (ev.xselection.property == utf8)
        paste();
      break;
    case VisibilityNotify:
      if (ev.xvisibility.state != VisibilityUnobscured)
        XRaiseWindow(dpy, win);
      break;
    }
  }
}

static void
shrinkandcenter(int *x, int *y, int wa_width, int wa_height)
{
  int fullpromptw = (prompt && *prompt) ? TEXTW(prompt) : 0;
  int list_width = columns * get_suggested_width();
  int mintotalw = MAX(fullpromptw + inputw, list_width);

  if (edgeoffset >= 0)
    mw -= 2 * edgeoffset * mw / 100;
  else
    mw = MIN(wa_width, MAX(mintotalw, 500));

  *y = (wa_height - mh) * 3 / 4;
  *x = (wa_width - mw) / 2;
}

struct
SetupData
{
  int x, y, i, j;
  unsigned int du;
  XSetWindowAttributes swa;
  XIM xim;
  Window w, dw, *dws;
  XWindowAttributes wa;
  XClassHint *ch;
#ifdef XINERAMA
  XineramaScreenInfo *info;
  Window pw;
  int a, di, n, area;
#endif
};

static void
initwinandinput(struct SetupData *s)
{
  /* create menu window */
  s->swa.override_redirect = True;
  s->swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
  s->swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
  win = XCreateWindow(dpy, parentwin, s->x, s->y, mw, mh, 0,
                      CopyFromParent, CopyFromParent, CopyFromParent,
                      CWOverrideRedirect | CWBackPixel | CWEventMask, &(s->swa));
  XSetClassHint(dpy, win, s->ch);


  /* input methods */
  if ((s->xim = XOpenIM(dpy, NULL, NULL, NULL)) == NULL)
    die("XOpenIM failed: could not open input device");

  xic = XCreateIC(s->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                  XNClientWindow, win, XNFocusWindow, win, NULL);

  XMapRaised(dpy, win);
  if (embed) {
    XSelectInput(dpy, parentwin, FocusChangeMask | SubstructureNotifyMask);
    if (XQueryTree(dpy, parentwin, &(s->dw), &(s->w), &(s->dws), &(s->du)) && s->dws) {
      for (s->i = 0; s->i < s->du && s->dws[s->i] != win; ++s->i)
        XSelectInput(dpy, s->dws[s->i], FocusChangeMask);
      XFree(s->dws);
    }
    grabfocus();
  }
  drw_resize(drw, mw, mh);
  drawmenu();
}

static void
preparegeometry(struct SetupData *s)
{
  /* init appearance */
  for (s->j = 0; s->j < SchemeLast; s->j++)
    scheme[s->j] = drw_scm_create(drw, palettes[palette][s->j], 2);

  clip = XInternAtom(dpy, "CLIPBOARD",   False);
  utf8 = XInternAtom(dpy, "UTF8_STRING", False);

  /* calculate menu geometry */
  bh = drw->fonts->h + 2;
  lines = MAX(lines, 0);
  mh = (lines + 1) * bh;
#ifdef XINERAMA
  i = 0;
  if (parentwin == root && (info = XineramaQueryScreens(dpy, &n))) {
    XGetInputFocus(dpy, &w, &di);
    if (mon >= 0 && mon < n)
      i = mon;
    else if (w != root && w != PointerRoot && w != None) {
      /* find top-level window containing current input focus */
      do {
        if (XQueryTree(dpy, (pw = w), &dw, &w, &dws, &du) && dws)
          XFree(dws);
      } while (w != root && w != pw);
      /* find xinerama screen with which the window intersects most */
      if (XGetWindowAttributes(dpy, pw, &wa))
        for (j = 0; j < n; j++)
          if ((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
            area = a;
            i = j;
          }
    }
    /* no focused window is on screen, so use pointer location instead */
    if (mon < 0 && !area && XQueryPointer(dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
      for (i = 0; i < n; i++)
        if (INTERSECT(x, y, 1, 1, info[i]) != 0)
          break;

    x = info[i].x_org;
    y = info[i].y_org + (topbar ? 0 : info[i].height - mh);
    mw = info[i].width;
    XFree(info);
  } else
#endif
  {
    if (!XGetWindowAttributes(dpy, parentwin, &(s->wa)))
      die("could not get embedding window attributes: 0x%lx",
          parentwin);
    s->x = 0;
    s->y = topbar ? 0 : s->wa.height - mh;
    mw = s->wa.width;
  }

  promptw = (prompt && *prompt) ? TEXTW(prompt) - lrpad / 4 : 0;
}

static void
set_lines_and_columns(int num_of_lines)
{
  if (columns < 1 && lines > 0)
    autosetcolumns();
  lines = MIN(lines, calcneededlines(num_of_lines));
}

static void
setup(int num_of_lines)
{
  XClassHint ch = { "dmenu", "dmenu" };
  struct SetupData s;
  s.ch = &ch;
#ifdef XINERAMA
  s.area = 0;
#endif

  set_lines_and_columns(num_of_lines);
  preparegeometry(&s);

  match();
  shrinkandcenter(&(s.x), &(s.y), mw, s.wa.height);

  initwinandinput(&s);
}

enum Palette
getpalette(const char *name)
{
  if (!strcmp(name, "red"))
    return PaletteRed;
  if (!strcmp(name, "r"))
    return PaletteRed;
  if (!strcmp(name, "blue"))
    return PaletteBlue;
  if (!strcmp(name, "b"))
    return PaletteBlue;
  return PaletteBlue;
}

static void
usage(void)
{
  die("usage: dmenu [-bfiv] [-l lines] [-o offset] [-g columns]\n"
      "             [-d selected] [-p prompt] [-fn font]\n"
      "             [-m monitor] [-nb color]\n"
      "             [-c palette] [-nf color] [-sb color]\n"
      "             [-sf color] [-w windowid]");
}

int
handleargs(int argc, char *argv[])
{
  int i, fast = 0;
  for (i = 1; i < argc; i++)
    /* these options take no arguments */
    if (!strcmp(argv[i], "-v")) {      /* prints version information */
      puts("dmenu-"VERSION);
      exit(0);
    } else if (!strcmp(argv[i], "-b")) /* appears at the bottom of the screen */
      topbar = 0;
    else if (!strcmp(argv[i], "-f"))   /* grabs keyboard before reading stdin */
      fast = 1;
    else if (!strcmp(argv[i], "-i")) { /* case-insensitive item matching */
      fstrncmp = strncasecmp;
      fstrstr = cistrstr;
    } else if (i + 1 == argc)
      usage();
    /* these options take one argument */
    else if (!strcmp(argv[i], "-l"))   /* number of lines in vertical list */
      lines = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-o"))   /* offset from edges */
      edgeoffset = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-g"))   /* grid columns */
      columns = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-d"))   /* default selected item */
      defaultitempos = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-m"))   /* monitor */
      mon = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-p"))   /* adds prompt to left of input field */
      prompt = argv[++i];
    else if (!strcmp(argv[i], "-c"))   /* baked colorscheme */
      palette = getpalette(argv[++i]);
    else if (!strcmp(argv[i], "-fn"))  /* font or font set */
      fonts[0] = argv[++i];
    else if (!strcmp(argv[i], "-nb"))  /* normal background color */
      palettes[palette][SchemeNorm][ColBg] = argv[++i];
    else if (!strcmp(argv[i], "-nf"))  /* normal foreground color */
      palettes[palette][SchemeNorm][ColFg] = argv[++i];
    else if (!strcmp(argv[i], "-sb"))  /* selected background color */
      palettes[palette][SchemeSel][ColBg] = argv[++i];
    else if (!strcmp(argv[i], "-sf"))  /* selected foreground color */
      palettes[palette][SchemeSel][ColFg] = argv[++i];
    else if (!strcmp(argv[i], "-w"))   /* embedding window id */
      embed = argv[++i];
    else
      usage();

  return fast;
}

static void
initxwin(void) {
  XWindowAttributes wa;

  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(dpy = XOpenDisplay(NULL)))
    die("cannot open display");
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  if (!embed || !(parentwin = strtol(embed, NULL, 0)))
    parentwin = root;
  if (!XGetWindowAttributes(dpy, parentwin, &wa))
    die("could not get embedding window attributes: 0x%lx",
        parentwin);
  drw = drw_create(dpy, screen, root, wa.width, wa.height);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h;
}

static void
considerbsdfail(void)
{
#ifdef __OpenBSD__
  if (pledge("stdio rpath", NULL) == -1)
    die("pledge");
#endif
}

int
grabandreadandgetnumlines(int fast)
{
  if (fast && !isatty(0)) {
    grabkeyboard();
    return readstdingetnumlines();
  } else {
    int n = readstdingetnumlines();
    grabkeyboard();
    return n;
  }
}

int
main(int argc, char *argv[])
{
  int fast = handleargs(argc, argv);
  initxwin();
  considerbsdfail();
  int num_of_lines = grabandreadandgetnumlines(fast);
  setup(num_of_lines);
  run();
  return 1; /* unreachable */
}
