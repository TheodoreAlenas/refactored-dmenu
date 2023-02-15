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
    s->grabfocus();
  }
  drw_resize(s->drw, s->mw, s->mh);
}

