#include <gl.h>
#include <gfx.h>
#include <task.h>

/* VirGL receives TGSI text. This is not GLSL. */
static const char *vertex_tgsi =
    "VERT\n"
    "DCL IN[0]\n"
    "DCL IN[1]\n"
    "DCL OUT[0], POSITION\n"
    "DCL OUT[1], GENERIC[0]\n"
    "0: MOV OUT[1], IN[1]\n"
    "1: MOV OUT[0], IN[0]\n"
    "2: END\n";

static const char *fragment_tgsi =
    "FRAG\n"
    "DCL IN[0], GENERIC[0], LINEAR\n"
    "DCL OUT[0], COLOR\n"
    "0: MOV OUT[0], IN[0]\n"
    "1: END\n";

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

int main(void)
{
    static const struct Vertex vertices[3] = {
        { -0.72f, -0.62f, 0.0f, 1.0f, 0.15f, 0.08f, 1.0f },
        {  0.72f, -0.62f, 0.0f, 0.08f, 0.85f, 0.25f, 1.0f },
        {  0.00f,  0.72f, 0.0f, 0.10f, 0.35f, 1.0f, 1.0f }
    };
    GLuint buffer, vs, fs, program;

    if (glInit() != 0)
        return 1;

    glViewport(0, 0, (GLsizei)gfx_width(), (GLsizei)gfx_height());
    glClearColor(0.025f, 0.035f, 0.06f, 1.0f);
    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    vs = glCreateShader(GL_VERTEX_SHADER);
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!vs || !fs || glShaderSourceTGSI(vs, vertex_tgsi) != 0 ||
        glShaderSourceTGSI(fs, fragment_tgsi) != 0 ||
        glCompileShader(vs) != 0 || glCompileShader(fs) != 0)
        return 2;

    program = glCreateProgram();
    if (!program || glAttachShader(program, vs) != 0 ||
        glAttachShader(program, fs) != 0 || glLinkProgram(program) != 0)
        return 3;

    glUseProgram(program);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void *)(3u * sizeof(float)));
    glEnableVertexAttribArray(1);

    while (1) {
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glFlush();
        task_sleep(16);
    }
    return 0;
}
