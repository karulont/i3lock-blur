#ifndef _XCB_H
#define _XCB_H

#include <xcb/xcb.h>
#include <xcb/dpms.h>
#include <time.h>
#define TIMER_START struct timespec before, after; \
    clock_gettime(CLOCK_MONOTONIC, &before);

#define TIMER_END clock_gettime(CLOCK_MONOTONIC, &after); \
    after.tv_sec -= before.tv_sec; \
    long result_ns = after.tv_sec * 1000000000 + after.tv_nsec \
        - before.tv_nsec; \
    printf("%s\ttook %f ms\n", __FUNCTION__, (float)result_ns / 1000000.0f);

extern xcb_connection_t *conn;
extern xcb_screen_t *screen;

xcb_visualtype_t *get_root_visual_type(xcb_screen_t *s);
xcb_pixmap_t create_fg_pixmap(xcb_connection_t *conn, xcb_screen_t *scr, u_int32_t* resolution);
xcb_pixmap_t create_bg_pixmap(xcb_connection_t *conn, xcb_screen_t *scr, u_int32_t *resolution, char *color);
xcb_window_t open_overlay_window(xcb_connection_t *conn, xcb_screen_t *scr);
xcb_window_t open_fullscreen_window(xcb_connection_t *conn, xcb_screen_t *scr, char *color, xcb_pixmap_t pixmap);
void grab_pointer_and_keyboard(xcb_connection_t *conn, xcb_screen_t *screen, xcb_cursor_t cursor);
void dpms_set_mode(xcb_connection_t *conn, xcb_dpms_dpms_mode_t mode);
xcb_cursor_t create_cursor(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win, int choice);

#endif
