
#include "test-util-3d.h"


static EGLint const config_attribute_list[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_DEPTH_SIZE, 8,
	EGL_STENCIL_SIZE, 8,
	EGL_NONE
};

static const EGLint context_attribute_list[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

static EGLDisplay display;
static EGLConfig config;
static EGLint num_config;
static EGLContext context;
static EGLSurface surface;
static GLuint program;
static GLint width, height;
const char *vertex_shader_source =
	"attribute vec4 aPosition;    \n"
	"                             \n"
	"void main()                  \n"
	"{                            \n"
	"    gl_Position = aPosition; \n"
	"}                            \n";
const char *fragment_shader_source =
	"precision highp float;       \n"
	"uniform vec4 uColor;         \n"
	"                             \n"
	"void main()                  \n"
	"{                            \n"
	"    gl_FragColor = uColor;   \n"
	"}                            \n";


static void
test_viewport(GLint x,
 	GLint y,
 	GLsizei width,
 	GLsizei height)
{
	DEBUG_MSG("test_viewport x: %d, y: %d, width: %d, height: %d", x, y, width, height);

	display = get_display();

	/* get an appropriate EGL frame buffer configuration */
	ECHK(eglChooseConfig(display, config_attribute_list, &config, 1, &num_config));
	DEBUG_MSG("num_config: %d", num_config);

	/* create an EGL rendering context */
	ECHK(context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribute_list));

	surface = make_window(display, config, 300, 300);

	// ECHK(eglQuerySurface(display, surface, EGL_WIDTH, &width));
	// ECHK(eglQuerySurface(display, surface, EGL_HEIGHT, &height));

	// DEBUG_MSG("Buffer: %dx%d", width, height);

	/* connect the context to the surface */
	ECHK(eglMakeCurrent(display, surface, surface, context));

	program = get_program(vertex_shader_source, fragment_shader_source);

	GCHK(glBindAttribLocation(program, 0, "aPosition"));

	link_program(program);

	GCHK(glViewport(x, y, width, height));

	GCHK(glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (GLfloat[]){
		20.0, 5.0, 0.0, 1.0, 30.0, 5.0, 0.0, 1.0, 20.0, 15.0, 0.0, 1.0, 30.0, 15.0, 0.0, 1.0
	}));
	GCHK(glEnableVertexAttribArray(0));

	// GCHK(glClearColor(clearcolor[0], clearcolor[1], clearcolor[2], clearcolor[3]));
	// GCHK(glClearStencil(clearstencil));
	// GCHK(glClearDepthf(cleardepth));
	GCHK(glClear(GL_COLOR_BUFFER_BIT));

	GCHK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

	GCHK(glFlush());
	ECHK(eglSwapBuffers(display, surface));
	GCHK(glFlush());

	usleep(1000000);

	eglTerminate(display);

	RD_END();
}

int main(int argc, char *argv[])
{
	TEST_START();
	TEST(test_viewport(16384, 0, 70000, 300));
	// TEST(test_viewport(2, 350, 400, 333));
	// TEST(test_viewport(-100, 2, 1000, 200));
	// TEST(test_viewport(-300, 0, 16684, 200));
	// TEST(test_viewport(3, 3, 10, 10));
	TEST_END();
	return 0;
}

