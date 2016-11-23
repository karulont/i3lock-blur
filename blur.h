#include <X11/Xlib.h>
#include <cairo.h>
#include <xcb/xcb.h>
void blur_image_surface(cairo_surface_t *surface, int radius);
void blur_image_gl(int scr, Pixmap pixmap, int width, int height, int radius,
                   float sigma);
void glx_init(int scr, int w, int h, int radius, float sigma);
void glx_deinit(void);
void glx_resize(int w, int h);
