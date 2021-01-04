
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include "test-util-3d.h"

#include <stdbool.h> 

static EGLint const config_attribute_list[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_DEPTH_SIZE, 8,
	EGL_NONE
};

static const EGLint context_attribute_list[] = {
	EGL_CONTEXT_CLIENT_VERSION, 3,
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
	"#version 310 es\n"
	"in vec4 piglit_vertex;\n"
	"out float x;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    gl_Position = piglit_vertex;\n"
	"    x = piglit_vertex.x;\n"
	"}\n"
	;
const char *geometry_shader_source =
	"#version 310 es\n"
	"#extension GL_EXT_geometry_shader :enable\n"
	"layout(points) in;\n"
	"layout(points, max_vertices = 1) out;\n"
	"\n"
	"out float x;\n"
	"\n"
	"void main()\n"
	"{\n"
	"  gl_Position = gl_in[0].gl_Position;\n"
	"  x = gl_in[0].gl_Position.x;\n"
	"  EmitVertex();\n"
	"  if (gl_in[0].gl_Position.x > 100.0) {\n"
	"		x = gl_in[0].gl_Position.x + 1.0;\n"
	"  		EmitVertex();\n"
	"  }\n"
	"}\n";
const char *fragment_shader_source =
	"#version 310 es\n"
	"out highp vec4 color;\n"
	"void main() { color = vec4(0); }\n"
	;



static const float data[] = {
	1.0,
	2.0,
	3.0,
	4.0,
	5.0,
	6.0,
	7.0,
	8.0,
	9.0,
	10.0,
	11.0,
	12.0,
};

bool
check_results(unsigned test, unsigned expect_written, const float *expect_data,
	      GLuint q0, GLuint q1)
{
	float *data;
	bool pass = true;
	GLuint written[2];
	GLuint total;

	GCHK(glGetQueryObjectuiv(q0, GL_QUERY_RESULT, &written[0]));

	GCHK(glGetQueryObjectuiv(q0, GL_QUERY_RESULT, &written[1]));

	total = written[0] + written[1];
	if (total != expect_written) {
		fprintf(stderr,
			"XFB %d GL_PRIMITIVES_WRITTEN: "
			"Expected %d, got %d\n",
			test, expect_written, total);
		pass = false;
	}

	GCHK(data = glMapBufferRange(GL_ARRAY_BUFFER, 0, 512, GL_MAP_READ_BIT));
	if (data == NULL) {
		fprintf(stderr,	"XFB %d: Could not map results buffer.\n",
			test);
		pass = false;
	} else {
		unsigned i;

		for (i = 0; i < expect_written; i++) {
			if (data[i] != expect_data[i]) {
				fprintf(stderr,
					"XFB %d data %d: "
					"Expected %f, got %f\n",
					test, i, expect_data[i], data[i]);
				pass = false;
			}
		}
		
		GCHK(glUnmapBuffer(GL_ARRAY_BUFFER));
	}

	return pass;
}

static GLuint
get_vs_gs_program(const char *vertex_shader_source, const char *geometry_shader_source)
{
	GLuint vertex_shader, geometry_shader, fragment_shader, program;

	DEBUG_MSG("vertex shader:\n%s", vertex_shader_source);
	DEBUG_MSG("geometry shader:\n%s", geometry_shader_source);

	RD_WRITE_SECTION(RD_VERT_SHADER,
			vertex_shader_source, strlen(vertex_shader_source));
	RD_WRITE_SECTION(RD_FRAG_SHADER,
			geometry_shader_source, strlen(geometry_shader_source));
	RD_WRITE_SECTION(RD_FRAG_SHADER,
			fragment_shader_source, strlen(fragment_shader_source));

	vertex_shader = get_shader(GL_VERTEX_SHADER, "vertex", vertex_shader_source);
	geometry_shader = get_shader(GL_GEOMETRY_SHADER, "geometry", geometry_shader_source);
	fragment_shader = get_shader(GL_FRAGMENT_SHADER, "geometry", fragment_shader_source);

	GCHK(program = glCreateProgram());

	GCHK(glAttachShader(program, vertex_shader));
	GCHK(glAttachShader(program, geometry_shader));
	GCHK(glAttachShader(program, fragment_shader));

	return program;
}


static void
test_tf_resume()
{
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



	static const char *varyings[] = {"x"};
	GLuint buffers[3];
	GLuint vao;
	GLuint queries[4];
	GLuint xfb[2];

	GCHK(glGenTransformFeedbacks(ARRAY_SIZE(xfb), xfb));
	GCHK(glGenBuffers(ARRAY_SIZE(buffers), buffers));

	GCHK(glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffers[0]));
	GCHK(glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1024, NULL, GL_STREAM_READ));

	GCHK(glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffers[1]));
	GCHK(glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1024, NULL, GL_STREAM_READ));

	GCHK(glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0));

	GCHK(glGenVertexArrays(1, &vao));
	GCHK(glBindVertexArray(vao));
	GCHK(glBindBuffer(GL_ARRAY_BUFFER, buffers[2]));
	GCHK(glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW));
	GCHK(glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 1 * sizeof(GLfloat), 0));
	GCHK(glEnableVertexAttribArray(0));

	GCHK(glGenQueries(ARRAY_SIZE(queries), queries));

	program = get_program(vertex_shader_source, fragment_shader_source);

	GCHK(glTransformFeedbackVaryings(program, 1, varyings, GL_INTERLEAVED_ATTRIBS));

	link_program(program);

	GCHK(glUseProgram(program));
	GCHK(glEnable(GL_RASTERIZER_DISCARD));

	/* Here's the actual test.
	 */
	GCHK(glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb[0]));
	GCHK(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffers[0]));
	GCHK(glBeginTransformFeedback(GL_POINTS));

	GCHK(glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, queries[0]));
	GCHK(glDrawArrays(GL_POINTS, 0, 4));
	GCHK(glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN));

	GCHK(glPauseTransformFeedback());

	GCHK(glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb[1]));
	GCHK(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffers[1]));
	GCHK(glBeginTransformFeedback(GL_POINTS));

	GCHK(glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, queries[1]));
	GCHK(glDrawArrays(GL_POINTS, 4, 2));
	GCHK(glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN));

	GCHK(glPauseTransformFeedback());


	GCHK(glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb[0]));
	GCHK(glResumeTransformFeedback());


	GCHK(glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, queries[2]));
	GCHK(glDrawArrays(GL_POINTS, 6, 4));
	GCHK(glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN));

	GCHK(glEndTransformFeedback());

	GCHK(glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb[1]));
	GCHK(glResumeTransformFeedback());

	GCHK(glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, queries[3]));
	GCHK(glDrawArrays(GL_POINTS, 10, 2));
	GCHK(glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN));

	GCHK(glEndTransformFeedback());

	GCHK(glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0));
	GCHK(glBindVertexArray(0));

	{
		static const float expected_xfb_data[] = {
			1.0, 2.0, 3.0, 4.0, 7.0, 8.0, 9.0, 10.0
		};

		GCHK(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));
		check_results(1, ARRAY_SIZE(expected_xfb_data),
				     expected_xfb_data,
				     queries[0], queries[2]);
	}

	/* The second XFB should have 4 primitives generated, and the buffer
	 * object should contain the values {5.0, 6.0, 11.0, 12.0}.
	 */
	{
		static const float expected_xfb_data[] = {
			5.0, 6.0, 11.0, 12.0
		};

		GCHK(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));
		check_results(2, ARRAY_SIZE(expected_xfb_data),
				     expected_xfb_data,
				     queries[1], queries[3]);
	}


	GCHK(glBindBuffer(GL_ARRAY_BUFFER, 0));

	GCHK(glBindVertexArray(0));
	GCHK(glDeleteVertexArrays(1, &vao));
	GCHK(glDeleteBuffers(ARRAY_SIZE(buffers), buffers));
	GCHK(glDeleteQueries(ARRAY_SIZE(queries), queries));
	GCHK(glDeleteTransformFeedbacks(ARRAY_SIZE(xfb), xfb));

	GCHK(glUseProgram(0));
	GCHK(glDeleteProgram(program));

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
	TEST(test_tf_resume());
	TEST_END();
	return 0;
}

