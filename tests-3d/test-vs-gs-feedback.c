
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include "test-util-3d.h"

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
static GLuint program;
const char *vertex_shader_source =
		"#version 310 es\n"
		" out vec4 vPosition; \n"
		"void main() {                 \n"
		"   //vPosition = vec4(0.5, 0.5, 0, 1); \n"
		"}                            \n";

const char *geometry_shader_source =
		"#version 310 es\n"
		"#extension GL_EXT_geometry_shader :enable\n"
		"layout(lines) in;\n"
		"layout(points, max_vertices = 1) out;\n"
		"\n"
		"out int primitive_id;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  //gl_Position = gl_in[0].gl_Position;\n"
		"  primitive_id = gl_PrimitiveIDIn;\n"
		"  EmitVertex();\n"
		"}\n";

const char *fragment_shader_source =
		"#version 310 es\n"
		"precision mediump float;     \n"
		"                             \n"
		" out vec4 FragColor;  \n"
		"void main()                  \n"
		"{                            \n"
		"    FragColor = vec4(1.0);   \n"
		"}                            \n";


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

#define LONGEST_INPUT_SEQUENCE 12

/* Sum of 2, 3, ..., LONGEST_INPUT_SEQUENCE + 1. */
#define NUM_ELEMENTS \
	(LONGEST_INPUT_SEQUENCE * (LONGEST_INPUT_SEQUENCE + 3) / 2)

/* Sum of 1, 2, ..., LONGEST_INPUT_SEQUENCE. */
#define MAX_TOTAL_PRIMS \
	(LONGEST_INPUT_SEQUENCE * (LONGEST_INPUT_SEQUENCE + 1) / 2)

static const char *varyings[] = { "primitive_id" };

void test_triangle_smoothed()
{
	GLint width, height;

	GLfloat vVertices[] = {
			 0.0f,  0.5f, 0.0f,
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f };
	GLfloat vColors[] = {
			1.0f, 0.0f, 0.0f, 1.0f,
			0.0f, 1.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 1.0f};
	EGLSurface surface;

	RD_START("triangle-smoothed", "");

	display = get_display();

	/* get an appropriate EGL frame buffer configuration */
	ECHK(eglChooseConfig(display, config_attribute_list, &config, 1, &num_config));
	DEBUG_MSG("num_config: %d", num_config);

	/* create an EGL rendering context */
	ECHK(context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribute_list));

	surface = make_window(display, config, 400, 240);

	ECHK(eglQuerySurface(display, surface, EGL_WIDTH, &width));
	ECHK(eglQuerySurface(display, surface, EGL_HEIGHT, &height));

	DEBUG_MSG("Buffer: %dx%d", width, height);

	/* connect the context to the surface */
	ECHK(eglMakeCurrent(display, surface, surface, context));

	glEnable(GL_RASTERIZER_DISCARD);

	program = get_vs_gs_program(vertex_shader_source, geometry_shader_source);

	glTransformFeedbackVaryings(program, 1, varyings, GL_INTERLEAVED_ATTRIBS);

	link_program(program);

	GLubyte prim_restart_index = 0xff;
	char *gs_text;
	GLuint prog, vs, gs, vao, xfb_buf, element_buf, generated_query,
		primitives_generated;
	GLubyte *elements;
	int i, j;
	GLsizei num_elements;
	GLuint *readback;

	/* Set up other GL state */
	GCHK(glGenVertexArrays(1, &vao));
	GCHK(glBindVertexArray(vao));
	GCHK(glGenBuffers(1, &xfb_buf));
	GCHK(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb_buf));
	GCHK(glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,
		     MAX_TOTAL_PRIMS * sizeof(GLint), NULL,
		     GL_STREAM_READ));
	GCHK(glGenQueries(1, &generated_query));

	GCHK(glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
	// glPrimitiveRestartIndex(prim_restart_index);

	GCHK(glViewport(0, 0, width, height)));

	GCHK(glGenBuffers(1, &element_buf));
	GCHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buf));
	GCHK(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		     sizeof(GLubyte) * NUM_ELEMENTS, NULL,
		     GL_STATIC_DRAW));
	GCHK(elements = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(GLubyte) * NUM_ELEMENTS, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
	num_elements = 0;
	for (i = 1; i <= LONGEST_INPUT_SEQUENCE; i++) {
		for (j = 0; j < i; j++) {
			/* Every element that isn't the primitive restart index
			 * can have any value as far as it is not the primitive
			 * restart index since we don't care about the actual
			 * vertex data.
			 *
			 * NOTE: repeating the indices for all elements but the
			 * primitive restart index causes a GPU hang in Intel's
			 * Sandy Bridge platform, likely due to a hardware bug,
			 * so make sure that we do not repeat the indices.
			 *
			 * More information:
			 *
			 * http://lists.freedesktop.org/archives/mesa-dev/2014-July/064221.html
			 */
			elements[num_elements++] =
				j != prim_restart_index ? j : j + 1;
		}
		elements[num_elements++] = prim_restart_index;
	}
	GCHK(glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER));

	GCHK(glBeginQuery(GL_PRIMITIVES_GENERATED, generated_query));
	GCHK(glBeginTransformFeedback(GL_POINTS));
	GCHK(glDrawElements(GL_LINES, num_elements, GL_UNSIGNED_BYTE, NULL));
	GCHK(glEndTransformFeedback());
	GCHK(glEndQuery(GL_PRIMITIVES_GENERATED));

	/* Find out how many times the GS got invoked so we'll know
	 * how many transform feedback outputs to examine.
	 */
	GCHK(glGetQueryObjectuiv(generated_query, GL_QUERY_RESULT,
			    &primitives_generated));
	if (primitives_generated > MAX_TOTAL_PRIMS) {
		printf("Expected no more than %d primitives, got %d\n",
		       MAX_TOTAL_PRIMS, primitives_generated);
		// pass = false;

		/* Set primitives_generated to MAX_TOTAL_PRIMS so that
		 * we don't overflow the buffer in the loop below.
		 */
		primitives_generated = MAX_TOTAL_PRIMS;
	}

        if (primitives_generated) {
                /* Check transform feedback outputs. */
                GCHK(readback = glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, MAX_TOTAL_PRIMS * sizeof(GLint), GL_MAP_READ_BIT));

				printf("readback: ");
				for (i = 0; i < primitives_generated; i++) {
					printf("%d ", readback[i]);
				}
				printf("\n");

                for (i = 0; i < primitives_generated; i++) {
                        if (readback[i] != i) {
                                printf("Expected primitive %d to have gl_PrimitiveIDIn"
                                       " = %d, got %d instead\n", i, i, readback[i]);
                                // pass = false;
                        }
                }
                GCHK(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER));
        } else {
                printf("Expected at least 1 primitive, got 0\n");
                // pass = false;
        }

	ECHK(eglSwapBuffers(display, surface));
	GCHK(glFlush());

	ECHK(eglDestroySurface(display, surface));

	ECHK(eglTerminate(display));

	RD_END();
}

int main(int argc, char *argv[])
{
	TEST_START();
	TEST(test_triangle_smoothed());
	TEST_END();

	return 0;
}

