#include <cairo.h>
#include <xcb/xcb.h>
#include <X11/Xlib.h>
void blur_image_surface (cairo_surface_t *surface, int radius);
void blur_image_gl(int scr, Pixmap pixmap, int width, int height);
void glx_init(int scr, int w, int h);
void glx_deinit(void);
void glx_resize(int w, int h);
