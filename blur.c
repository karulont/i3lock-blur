#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <GL/glxext.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

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
static const char *FRAG_SHADER_P1 =
"#version 130\n"
"varying vec2 v_Coordinates;\n"
 
"uniform vec2 u_Scale;\n"
"uniform sampler2D u_Texture0;\n";
 
static const char *FRAG_SHADER_F1 = "const vec2 gaussFilter[%d] = \n"
"vec2[](\n";
static const char *FRAG_SHADER_F2 = "vec2 (%f, %f),\n";
static const char *FRAG_SHADER_F3 = "vec2 (%f, %f)\n"
");\n";

static const char *FRAG_SHADER_P3 =
"void main()\n"
"{\n"
    "vec4 color = vec4(0.0,0.0,0.0,0.0);\n";
static const char *FRAG_SHADER_F4 = "for( int i = 0; i < %d; i++ )\n";
static const char *FRAG_SHADER_P4 =
    "{\n"
        "color += texture2D( u_Texture0, vec2( v_Coordinates.x+gaussFilter[i].x*u_Scale.x, v_Coordinates.y+gaussFilter[i].x*u_Scale.y ) )*gaussFilter[i].y;\n"
    "}\n"
    "gl_FragColor = color;\n"
"}\n";

static char *generate_fragment_shader(int blur_radius, float sigma) {
    // First, generate the normal Gaussian weights for a given sigma
    float *standardGaussianWeights = calloc(blur_radius + 1, sizeof(float));
    float sumOfWeights = 0.0;
    for (int currentGaussianWeightIndex = 0; currentGaussianWeightIndex < blur_radius + 1; currentGaussianWeightIndex++)
    {
        standardGaussianWeights[currentGaussianWeightIndex] = (1.0 / sqrt(2.0 * M_PI * pow(sigma, 2.0))) * exp(-pow(currentGaussianWeightIndex, 2.0) / (2.0 * pow(sigma, 2.0)));

        if (currentGaussianWeightIndex == 0)
        {
            sumOfWeights += standardGaussianWeights[currentGaussianWeightIndex];
        }
        else
        {
            sumOfWeights += 2.0 * standardGaussianWeights[currentGaussianWeightIndex];
        }
    }

    // Next, normalize these weights to prevent the clipping of the Gaussian curve at the end of the discrete samples from reducing luminance
    for (int currentGaussianWeightIndex = 0; currentGaussianWeightIndex < blur_radius + 1; currentGaussianWeightIndex++)
    {
        standardGaussianWeights[currentGaussianWeightIndex] = standardGaussianWeights[currentGaussianWeightIndex] / sumOfWeights;
    }

    // From these weights we calculate the offsets to read interpolated values from
    int numberOfOptimizedOffsets = blur_radius / 2 + (blur_radius % 2);
    float *optimizedGaussianOffsets = calloc(numberOfOptimizedOffsets, sizeof(float));
    float *optimizedGaussianWeights = calloc(numberOfOptimizedOffsets, sizeof(float));

    for (int currentOptimizedOffset = 0; currentOptimizedOffset < numberOfOptimizedOffsets; currentOptimizedOffset++)
    {
        float firstWeight = standardGaussianWeights[currentOptimizedOffset*2 + 1];
        float secondWeight = standardGaussianWeights[currentOptimizedOffset*2 + 2];

        float optimizedWeight = firstWeight + secondWeight;

        optimizedGaussianWeights[currentOptimizedOffset] = optimizedWeight;        
        optimizedGaussianOffsets[currentOptimizedOffset] = (firstWeight * (currentOptimizedOffset*2 + 1) + secondWeight * (currentOptimizedOffset*2 + 2)) / optimizedWeight;
    }
    int size = sizeof(char) * (450 + 56 * numberOfOptimizedOffsets);
    char *output = (char*)malloc(size);
    char buf[512];
    strcpy(output, FRAG_SHADER_P1);
    sprintf(buf, FRAG_SHADER_F1, 2*numberOfOptimizedOffsets+1);
    strcat(output, buf);
    sprintf(buf, FRAG_SHADER_F2, 0.0, standardGaussianWeights[0]);
    strcat(output, buf);
    for (int i=0; i!=numberOfOptimizedOffsets-1; ++i) {
        sprintf(buf, FRAG_SHADER_F2, optimizedGaussianOffsets[i], optimizedGaussianWeights[i]);
        strcat(output, buf);
        sprintf(buf, FRAG_SHADER_F2, -optimizedGaussianOffsets[i], optimizedGaussianWeights[i]);
        strcat(output, buf);
    }
    sprintf(buf, FRAG_SHADER_F2, optimizedGaussianOffsets[numberOfOptimizedOffsets-1], optimizedGaussianWeights[numberOfOptimizedOffsets-1]);
    strcat(output, buf);
    sprintf(buf, FRAG_SHADER_F3, -optimizedGaussianOffsets[numberOfOptimizedOffsets-1], optimizedGaussianWeights[numberOfOptimizedOffsets-1]);
    strcat(output, buf);
    strcat(output, FRAG_SHADER_P3);
    sprintf(buf, FRAG_SHADER_F4, 2*numberOfOptimizedOffsets+1);
    strcat(output, buf);
    strcat(output, FRAG_SHADER_P4);
    free(standardGaussianWeights);
    free(optimizedGaussianWeights);
    free(optimizedGaussianOffsets);
    return output;
}

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

void glx_init(int scr, int w, int h, int radius, float sigma) {
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
    char *fragment_shader = generate_fragment_shader(radius, sigma);
    GLchar const* files[] = {fragment_shader};
    glShaderSource(f_shader, 1, files, NULL);
    free(fragment_shader);
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

void blur_image_gl(int scr, Pixmap pixmap, int width, int height, int radius, float sigma) {
    if (configs == NULL ) {
        glx_init(scr, width, height, radius, sigma);
    }


    glx_pixmap = glXCreatePixmap(display, configs[0], pixmap, pixmap_attribs);

    for (uint8_t i=0;i<2;++i) {
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
        glOrtho(-1.0, 1.0, -1.0, 1.0, -1., 1);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

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
        glTexCoord2f(0.0, 0.0); glVertex2f(-1.0,  1.0);
        glTexCoord2f(1.0, 0.0); glVertex2f( 1.0,  1.0);
        glTexCoord2f(1.0, 1.0); glVertex2f( 1.0, -1.0);
        glTexCoord2f(0.0, 1.0); glVertex2f(-1.0, -1.0);
        glEnd(); 
        glFlush();
    }
    GC gc = XCreateGC(display, pixmap, 0, NULL);
    XCopyArea(display, tmp1, pixmap, gc, 0, 0, width, height, 0, 0);
    XFreeGC(display, gc);
    glXDestroyPixmap(display, glx_pixmap);
}
