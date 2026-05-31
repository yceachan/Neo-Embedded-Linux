/*
 * render.c —— GLES2 时变渐变着色器
 *
 * 设计选择：
 *  - vertex shader 只做 NDC 透传 + 把顶点位置传给 fragment 当作 uv
 *  - fragment shader 用 u_time + uv 算径向渐变叠加正弦波
 *  - 一个 GL_TRIANGLE_STRIP 全屏四边形，最少 draw call
 *
 * 这样可以确保：
 *  (1) 每一帧都需要 GPU 真正执行像素着色 (而不是 glClear 一刷了事)
 *  (2) shader 编译/链接错误会立刻显形 —— 验证 libmali 的 shader 编译链路
 *  (3) 颜色随时间变化 → 截图/录屏能看出"动"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES2/gl2.h>

#include "render.h"

static const char VS_SRC[] =
	"attribute vec2 a_pos;\n"
	"varying vec2 v_uv;\n"
	"void main() {\n"
	"    v_uv = a_pos * 0.5 + 0.5;\n"
	"    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
	"}\n";

static const char FS_SRC[] =
	"precision mediump float;\n"
	"varying vec2 v_uv;\n"
	"uniform float u_time;\n"
	"uniform vec2  u_resolution;\n"
	"void main() {\n"
	"    vec2 uv = v_uv;\n"
	"    /* 修正宽高比，使径向圆心不被拉成椭圆 */\n"
	"    float aspect = u_resolution.x / max(u_resolution.y, 1.0);\n"
	"    vec2 p = (uv - 0.5);\n"
	"    p.x *= aspect;\n"
	"    float r = length(p);\n"
	"\n"
	"    /* 三通道相位错开 → 旋转色环效果 */\n"
	"    float a = 6.2831 * r - u_time * 1.2;\n"
	"    vec3 col = vec3(\n"
	"        0.5 + 0.5 * sin(a),\n"
	"        0.5 + 0.5 * sin(a + 2.094),\n"
	"        0.5 + 0.5 * sin(a + 4.188)\n"
	"    );\n"
	"\n"
	"    /* 亮度随时间脉动 + vignette */\n"
	"    float pulse = 0.85 + 0.15 * sin(u_time * 2.5);\n"
	"    float vign  = 1.0 - smoothstep(0.55, 0.95, r);\n"
	"    col *= pulse * vign;\n"
	"\n"
	"    /* 中央留一个亮点暗示 'TSPI' 焦点 */\n"
	"    float core = exp(-pow(r * 6.0, 2.0));\n"
	"    col += vec3(core * 0.4);\n"
	"\n"
	"    gl_FragColor = vec4(col, 1.0);\n"
	"}\n";

static GLuint g_program;
static GLuint g_vbo;
static GLint  g_loc_pos;
static GLint  g_loc_time;
static GLint  g_loc_res;

static GLuint compile_shader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
		char *log = malloc(len > 0 ? (size_t)len : 1);
		if (log) {
			glGetShaderInfoLog(s, len, NULL, log);
			fprintf(stderr, "[gl] shader (%s) compile failed:\n%s\n",
			        type == GL_VERTEX_SHADER ? "VS" : "FS", log);
			free(log);
		}
		glDeleteShader(s);
		return 0;
	}
	return s;
}

int render_init(void)
{
	GLuint vs = compile_shader(GL_VERTEX_SHADER,   VS_SRC);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FS_SRC);
	if (!vs || !fs) return -1;

	g_program = glCreateProgram();
	glAttachShader(g_program, vs);
	glAttachShader(g_program, fs);
	glLinkProgram(g_program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = 0;
	glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		glGetProgramiv(g_program, GL_INFO_LOG_LENGTH, &len);
		char *log = malloc(len > 0 ? (size_t)len : 1);
		if (log) {
			glGetProgramInfoLog(g_program, len, NULL, log);
			fprintf(stderr, "[gl] program link failed:\n%s\n", log);
			free(log);
		}
		return -1;
	}

	g_loc_pos  = glGetAttribLocation(g_program,  "a_pos");
	g_loc_time = glGetUniformLocation(g_program, "u_time");
	g_loc_res  = glGetUniformLocation(g_program, "u_resolution");

	/* 全屏 NDC 四边形，TRIANGLE_STRIP 顺序 */
	static const GLfloat QUAD[] = {
		-1.f, -1.f,
		 1.f, -1.f,
		-1.f,  1.f,
		 1.f,  1.f,
	};
	glGenBuffers(1, &g_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD), QUAD, GL_STATIC_DRAW);

	printf("[gl]  program ok, vbo=%u, locs: pos=%d time=%d res=%d\n",
	       g_program, g_loc_pos, g_loc_time, g_loc_res);
	fflush(stdout);
	return 0;
}

void render_frame(int width, int height, float t)
{
	glViewport(0, 0, width, height);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(g_program);
	glUniform1f(g_loc_time, t);
	glUniform2f(g_loc_res, (float)width, (float)height);

	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glEnableVertexAttribArray(g_loc_pos);
	glVertexAttribPointer(g_loc_pos, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(g_loc_pos);
}

void render_fini(void)
{
	if (g_vbo)     { glDeleteBuffers(1, &g_vbo); g_vbo = 0; }
	if (g_program) { glDeleteProgram(g_program); g_program = 0; }
}
