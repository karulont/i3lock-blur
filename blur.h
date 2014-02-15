#include <cairo.h>
#include <xcb/xcb.h>
#include <X11/Xlib.h>
void blur_image_surface (cairo_surface_t *surface, int radius);
void blur_image_gl(Display *dpy, int scr, Pixmap pixmap, int width, int height);
void glx_init(Display *dpy, int scr, int w, int h);
