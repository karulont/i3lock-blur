/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2009 Chris Wilson
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <math.h>
#include <stdint.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <GL/glxext.h>

#include "blur.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

/* Performs a simple 2D Gaussian blur of radius @radius on surface @surface. */
void
blur_image_surface (cairo_surface_t *surface, int radius)
{
    cairo_surface_t *tmp;
    int width, height;
    int src_stride, dst_stride;
    int x, y, z, w;
    uint8_t *src, *dst;
    uint32_t *s, *d, a, p;
    int i, j, k;
    uint8_t kernel[17];
    const int size = ARRAY_LENGTH (kernel);
    const int half = size / 2;

    if (cairo_surface_status (surface))
	return;

    width = cairo_image_surface_get_width (surface);
    height = cairo_image_surface_get_height (surface);

    switch (cairo_image_surface_get_format (surface)) {
    case CAIRO_FORMAT_A1:
    default:
	/* Don't even think about it! */
	return;

    case CAIRO_FORMAT_A8:
	/* Handle a8 surfaces by effectively unrolling the loops by a
	 * factor of 4 - this is safe since we know that stride has to be a
	 * multiple of uint32_t. */
	width /= 4;
	break;

    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	break;
    }

    tmp = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status (tmp))
	return;

    src = cairo_image_surface_get_data (surface);
    src_stride = cairo_image_surface_get_stride (surface);

    dst = cairo_image_surface_get_data (tmp);
    dst_stride = cairo_image_surface_get_stride (tmp);

    a = 0;
    for (i = 0; i < size; i++) {
	double f = i - half;
	a += kernel[i] = exp (- f * f / 30.0) * 80;
    }

    /* Horizontally blur from surface -> tmp */
    for (i = 0; i < height; i++) {
	s = (uint32_t *) (src + i * src_stride);
	d = (uint32_t *) (dst + i * dst_stride);
	for (j = 0; j < width; j++) {
	    if (radius < j && j < width - radius) {
		d[j] = s[j];
		continue;
	    }

	    x = y = z = w = 0;
	    for (k = 0; k < size; k++) {
		if (j - half + k < 0 || j - half + k >= width)
		    continue;

		p = s[j - half + k];

		x += ((p >> 24) & 0xff) * kernel[k];
		y += ((p >> 16) & 0xff) * kernel[k];
		z += ((p >>  8) & 0xff) * kernel[k];
		w += ((p >>  0) & 0xff) * kernel[k];
	    }
	    d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
	}
    }

    /* Then vertically blur from tmp -> surface */
    for (i = 0; i < height; i++) {
	s = (uint32_t *) (dst + i * dst_stride);
	d = (uint32_t *) (src + i * src_stride);
	for (j = 0; j < width; j++) {
	    if (radius <= i && i < height - radius) {
		d[j] = s[j];
		continue;
	    }

	    x = y = z = w = 0;
	    for (k = 0; k < size; k++) {
		if (i - half + k < 0 || i - half + k >= height)
		    continue;

		s = (uint32_t *) (dst + (i - half + k) * dst_stride);
		p = s[j];

		x += ((p >> 24) & 0xff) * kernel[k];
		y += ((p >> 16) & 0xff) * kernel[k];
		z += ((p >>  8) & 0xff) * kernel[k];
		w += ((p >>  0) & 0xff) * kernel[k];
	    }
	    d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
	}
    }

    cairo_surface_destroy (tmp);
    cairo_surface_mark_dirty (surface);
}

void printShaderInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

    glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
        //printf("shader_infolog: %s\n",infoLog);
        free(infoLog);
    }
}

void printProgramInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

    glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
        //printf("program_infolog: %s\n",infoLog);
        free(infoLog);
    }
}

/* Variables for GL */
static const char *VERT_SHADER =
"varying vec2 v_Coordinates;\n"

"void main(void) {\n"
    "gl_Position = ftransform();\n"
    "v_Coordinates = vec2(gl_MultiTexCoord0);\n"
"}\n";
static const char *FRAG_SHADER =
"#version 130\n"
"/////////////////////////////////////////////////\n"
"// 7x1 gaussian blur fragment shader\n"
"/////////////////////////////////////////////////\n"
 
"varying vec2 v_Coordinates;\n"
 
"uniform vec2 u_Scale;\n"
"uniform sampler2D u_Texture0;\n"
 
"const vec2 gaussFilter[7] = \n"
"vec2[](\n"
    "vec2 (-3.0,       0.015625),\n"
    "vec2 (-2.0,       0.09375),\n"
    "vec2 (-1.0,       0.234375),\n"
    "vec2 (0.0,        0.3125),\n"
    "vec2 (1.0,        0.234375),\n"
    "vec2 (2.0,        0.09375),\n"
    "vec2 (3.0,        0.015625)\n"
");\n"

"void main()\n"
"{\n"
    "vec4 color = vec4(0.0,0.0,0.0,0.0);\n"
    "for( int i = 0; i < 7; i++ )\n"
    "{\n"
        "color += texture2D( u_Texture0, vec2( v_Coordinates.x+gaussFilter[i].x*u_Scale.x, v_Coordinates.y+gaussFilter[i].x*u_Scale.y ) )*gaussFilter[i].y;\n"
    "}\n"
    "gl_FragColor = color;\n"
"}\n";

GLXFBConfig *configs = NULL;
GLXContext ctx;
Pixmap tmp;
GLXPixmap glx_tmp;
Pixmap tmp1;
GLXPixmap glx_tmp1;
GLXPixmap glx_pixmap;
XVisualInfo *vis;
GLuint shader_prog;
GLuint v_shader;
GLuint f_shader;
static PFNGLXBINDTEXIMAGEEXTPROC glXBindTexImageEXT = NULL;
static PFNGLXRELEASETEXIMAGEEXTPROC glXReleaseTexImageEXT = NULL;
const int pixmap_config[] = {
    GLX_BIND_TO_TEXTURE_RGBA_EXT, True,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
    GLX_DOUBLEBUFFER, False,
    GLX_Y_INVERTED_EXT, GLX_DONT_CARE,
    None
};
const int pixmap_attribs[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
    None
};

void glx_init(Display *dpy, int scr, int w, int h) {
    int i;
    configs = glXChooseFBConfig(dpy, scr, pixmap_config, &i);
    vis = glXGetVisualFromFBConfig(dpy, configs[0]);
    ctx = glXCreateContext(dpy, vis, NULL, True);

    glXBindTexImageEXT = (PFNGLXBINDTEXIMAGEEXTPROC)
        glXGetProcAddress((GLubyte *) "glXBindTexImageEXT"); 
    glXReleaseTexImageEXT = (PFNGLXRELEASETEXIMAGEEXTPROC)
        glXGetProcAddress((GLubyte *) "glXReleaseTexImageEXT"); 


    tmp = XCreatePixmap(dpy, RootWindow(dpy, vis->screen), w, h, vis->depth);
    glx_tmp = glXCreatePixmap(dpy, configs[0], tmp, pixmap_attribs);
    glXMakeCurrent(dpy, glx_tmp, ctx);

    tmp1 = XCreatePixmap(dpy, RootWindow(dpy, vis->screen), w, h, vis->depth);
    glx_tmp1 = glXCreatePixmap(dpy, configs[0], tmp1, pixmap_attribs);

    v_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v_shader, 1, &VERT_SHADER, NULL);
    glCompileShader(v_shader);
    glGetShaderiv(v_shader, GL_COMPILE_STATUS, &i);
    //printf("V Shader: %d\n", i);
    printShaderInfoLog(v_shader);
    f_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_shader, 1, &FRAG_SHADER, NULL);
    glCompileShader(f_shader);
    glGetShaderiv(f_shader, GL_COMPILE_STATUS, &i);
    //printf("F Shader: %d\n", i);
    printShaderInfoLog(f_shader);
    shader_prog = glCreateProgram();
    glAttachShader(shader_prog, v_shader);
    glAttachShader(shader_prog, f_shader);
    glLinkProgram(shader_prog);
    glGetShaderiv(f_shader, GL_LINK_STATUS, &i);
    //printf("Program: %d\n", i);
    printShaderInfoLog(f_shader);
    printProgramInfoLog(shader_prog);
}

void glx_deinit() {
    glDetachShader(shader_prog, v_shader);
    glDetachShader(shader_prog, f_shader);
    glDeleteShader(v_shader);
    glDeleteShader(f_shader);
    glDeleteProgram(shader_prog);
}

void blur_image_gl(Display *dpy, int scr, Pixmap pixmap, int width, int height) {
    if (configs == NULL ) {
        glx_init(dpy, scr, width, height);
    }


    glx_pixmap = glXCreatePixmap(dpy, configs[0], pixmap, pixmap_attribs);

    for (uint8_t i=0;i<4;++i) {
    if ((i & 1) == 0) {
        glXMakeCurrent(dpy, glx_tmp, ctx);
    }
    else {
        glXMakeCurrent(dpy, glx_tmp1, ctx);
    }
    glEnable(GL_TEXTURE_2D);
    if (i==0) {
        glXBindTexImageEXT(dpy, glx_pixmap, GLX_FRONT_EXT, NULL);
    } else {
        glXBindTexImageEXT(dpy, (i & 1) == 1 ? glx_tmp : glx_tmp1, GLX_FRONT_EXT, NULL);
    }
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL); 


    glViewport(0, 0, (GLsizei) width, (GLsizei) height);
    glClearColor(0.3, 0.3, 0.3, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, 1., 20.);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0., 0., 10., 0., 0., 0., 0., 1., 0.);

    glColor3f(0.0, 0.0, 1.0);
    
    glUseProgram(shader_prog);
    GLint u_Scale = glGetUniformLocation(shader_prog, "u_Scale");
    if ( i & 1 == 0) {
        glUniform2f(u_Scale,  1.0 / width, 0);
    }
    else {
        glUniform2f(u_Scale, 0, 1.0 / height);
    }

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0); glVertex3f(-1.0,  1.0, 0.0);
    glTexCoord2f(1.0, 0.0); glVertex3f( 1.0,  1.0, 0.0);
    glTexCoord2f(1.0, 1.0); glVertex3f( 1.0, -1.0, 0.0);
    glTexCoord2f(0.0, 1.0); glVertex3f(-1.0, -1.0, 0.0);
    glEnd(); 
    glFlush();
    }
    GC gc = XCreateGC(dpy, pixmap, 0, NULL);
    XCopyArea(dpy, tmp1, pixmap, gc, 0, 0, width, height, 0, 0);
    XFreeGC(dpy, gc);
    glXDestroyPixmap(dpy, glx_pixmap);
}
