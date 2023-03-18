#include <time.h>
#include "x-interface.h"
#include "util.h"

void
initwinandinput(struct SetupData *s)
{
  /* create menu window */
  s->swa.override_redirect = True;
  s->swa.background_pixel = s->scheme[SchemeNorm][ColBg].pixel;
  s->swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
  s->win = XCreateWindow(s->dpy, s->parentwin, s->x, s->y, s->mw, s->mh, 0,
                      CopyFromParent, CopyFromParent, CopyFromParent,
                      CWOverrideRedirect | CWBackPixel | CWEventMask, &(s->swa));
  XSetClassHint(s->dpy, s->win, s->ch);


  /* input methods */
  if ((s->xim = XOpenIM(s->dpy, NULL, NULL, NULL)) == NULL)
    die("XOpenIM failed: could not open input device");

  s->xic = XCreateIC(s->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                  XNClientWindow, s->win, XNFocusWindow, s->win, NULL);

  XMapRaised(s->dpy, s->win);
  if (s->embed) {
    XSelectInput(s->dpy, s->parentwin, FocusChangeMask | SubstructureNotifyMask);
    if (XQueryTree(s->dpy, s->parentwin, &(s->dw), &(s->w), &(s->dws), &(s->du)) && s->dws) {
      for (s->i = 0; s->i < s->du && s->dws[s->i] != s->win; ++s->i)
        XSelectInput(s->dpy, s->dws[s->i], FocusChangeMask);
      XFree(s->dws);
    }
    grabfocus(s);
  }
  drw_resize(s->drw, s->mw, s->mh);
}

void
grabfocus(struct SetupData *s)
{
  struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000  };
  Window focuswin;
  int i, revertwin;

  for (i = 0; i < 100; ++i) {
    XGetInputFocus(s->dpy, &focuswin, &revertwin);
    if (focuswin == s->win)
      return;
    XSetInputFocus(s->dpy, s->win, RevertToParent, CurrentTime);
    nanosleep(&ts, NULL);
  }
  die("cannot grab focus");
}

void
grabkeyboard(const struct SetupData *s)
{
  struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000  };
  int i;

  if (s->embed)
    return;
  /* try to grab keyboard, we may have to wait for another process to ungrab */
  for (i = 0; i < 1000; i++) {
    if (XGrabKeyboard(s->dpy, DefaultRootWindow(s->dpy), True, GrabModeAsync,
                      GrabModeAsync, CurrentTime) == GrabSuccess)
      return;
    nanosleep(&ts, NULL);
  }
  die("cannot grab keyboard");
}

