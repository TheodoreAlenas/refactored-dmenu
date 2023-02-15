#ifndef X_INTERFACE_H
#define X_INTERFACE_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#include "scheme_types.h"
#include "drw.h"

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

  char *embed;
  int mw, mh;
  Clr **scheme;
  Display *dpy;
  XIC xic;
  Window parentwin, win;
  Drw *drw;
  void (*grabfocus) (void);
};


void initwinandinput(struct SetupData *s);

#endif
