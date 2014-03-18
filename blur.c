#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <GL/glxext.h>

#include "xcb.h"
#include "blur.h"

extern Display *display;

#if DEBUG_GL
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
        printf("shader_infolog: %s\n",infoLog);
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
        printf("program_infolog: %s\n",infoLog);
        free(infoLog);
    }
}
#endif

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
 
"const vec2 gaussFilter[5] = \n"
"vec2[](\n"
    "vec2 (-3.2307692308, 0.0702702703),\n"
    "vec2 (-1.3846153846, 0.3162162162),\n"
    "vec2 (0.0,           0.2270270270),\n"
    "vec2 (1.3846153846,  0.3162162162),\n"
    "vec2 (3.2307692308,  0.0702702703)\n"
");\n"

"void main()\n"
"{\n"
    "vec4 color = vec4(0.0,0.0,0.0,0.0);\n"
    "for( int i = 0; i < 5; i++ )\n"
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

void glx_init(int scr, int w, int h) {
    int i;
    configs = glXChooseFBConfig(display, scr, pixmap_config, &i);
    vis = glXGetVisualFromFBConfig(display, configs[0]);
    ctx = glXCreateContext(display, vis, NULL, True);

    glXBindTexImageEXT = (PFNGLXBINDTEXIMAGEEXTPROC)
        glXGetProcAddress((GLubyte *) "glXBindTexImageEXT"); 
    glXReleaseTexImageEXT = (PFNGLXRELEASETEXIMAGEEXTPROC)
        glXGetProcAddress((GLubyte *) "glXReleaseTexImageEXT"); 


    tmp = XCreatePixmap(display, RootWindow(display, vis->screen), w, h, vis->depth);
    glx_tmp = glXCreatePixmap(display, configs[0], tmp, pixmap_attribs);
    glXMakeCurrent(display, glx_tmp, ctx);

    tmp1 = XCreatePixmap(display, RootWindow(display, vis->screen), w, h, vis->depth);
    glx_tmp1 = glXCreatePixmap(display, configs[0], tmp1, pixmap_attribs);

    v_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v_shader, 1, &VERT_SHADER, NULL);
    glCompileShader(v_shader);
    glGetShaderiv(v_shader, GL_COMPILE_STATUS, &i);
#if DEBUG_GL
    printf("V Shader: %d\n", i);
    printShaderInfoLog(v_shader);
#endif
    f_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_shader, 1, &FRAG_SHADER, NULL);
    glCompileShader(f_shader);
    glGetShaderiv(f_shader, GL_COMPILE_STATUS, &i);
#if DEBUG_GL
    printf("F Shader: %d\n", i);
    printShaderInfoLog(f_shader);
#endif
    shader_prog = glCreateProgram();
    glAttachShader(shader_prog, v_shader);
    glAttachShader(shader_prog, f_shader);
    glLinkProgram(shader_prog);
    glGetShaderiv(f_shader, GL_LINK_STATUS, &i);
#if DEBUG_GL
    printf("Program: %d\n", i);
    printShaderInfoLog(f_shader);
    printProgramInfoLog(shader_prog);
#endif
}

static void glx_free_pixmaps(void) {
    glXDestroyPixmap(display, glx_tmp);
    glXDestroyPixmap(display, glx_tmp1);
    XFreePixmap(display, tmp);
    XFreePixmap(display, tmp1);
}

void glx_resize(int w, int h) {
    /* free old pixmaps */
    glx_free_pixmaps();

    /* create new pixmaps */
    tmp = XCreatePixmap(display, RootWindow(display, vis->screen), w, h, vis->depth);
    glx_tmp = glXCreatePixmap(display, configs[0], tmp, pixmap_attribs);
    glXMakeCurrent(display, glx_tmp, ctx);
    tmp1 = XCreatePixmap(display, RootWindow(display, vis->screen), w, h, vis->depth);
    glx_tmp1 = glXCreatePixmap(display, configs[0], tmp1, pixmap_attribs);
}

void glx_deinit(void) {
    glx_free_pixmaps();
    glDetachShader(shader_prog, v_shader);
    glDetachShader(shader_prog, f_shader);
    glDeleteShader(v_shader);
    glDeleteShader(f_shader);
    glDeleteProgram(shader_prog);
}

void blur_image_gl(int scr, Pixmap pixmap, int width, int height) {
    TIMER_START
    if (configs == NULL ) {
        glx_init(scr, width, height);
    }


    glx_pixmap = glXCreatePixmap(display, configs[0], pixmap, pixmap_attribs);

    for (uint8_t i=0;i<4;++i) {
    if ((i & 1) == 0) {
        glXMakeCurrent(display, glx_tmp, ctx);
    }
    else {
        glXMakeCurrent(display, glx_tmp1, ctx);
    }
    glEnable(GL_TEXTURE_2D);
    if (i==0) {
        glXBindTexImageEXT(display, glx_pixmap, GLX_FRONT_EXT, NULL);
    } else {
        glXBindTexImageEXT(display, (i & 1) == 1 ? glx_tmp : glx_tmp1, GLX_FRONT_EXT, NULL);
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
    if ( (i & 1) == 0) {
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
    GC gc = XCreateGC(display, pixmap, 0, NULL);
    XCopyArea(display, tmp1, pixmap, gc, 0, 0, width, height, 0, 0);
    XFreeGC(display, gc);
    glXDestroyPixmap(display, glx_pixmap);
    TIMER_END
}
