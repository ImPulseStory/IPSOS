#include <gl.h>
#include <gfx.h>
#include <task.h>

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

struct Vertex { float x, y, z, r, g, b, a; };

int main(void)
{
    static const struct Vertex vertices[] = {
        {-0.75f, -0.65f, 0.0f, 1.0f, 0.15f, 0.10f, 1.0f},
        { 0.75f, -0.65f, 0.0f, 0.10f, 0.90f, 0.20f, 1.0f},
        { 0.75f,  0.65f, 0.0f, 0.10f, 0.30f, 1.00f, 1.0f},
        {-0.75f,  0.65f, 0.0f, 1.00f, 0.85f, 0.10f, 1.0f}
    };
    static const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
    GLuint buffers[2], vs, fs, program;

    if (glInit() != 0) return 1;
    glViewport(0, 0, (GLsizei)gfx_width(), (GLsizei)gfx_height());
    glClearColor(0.025f, 0.035f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    vs = glCreateShader(GL_VERTEX_SHADER);
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!vs || !fs || glShaderSourceTGSI(vs, vertex_tgsi) ||
        glShaderSourceTGSI(fs, fragment_tgsi) ||
        glCompileShader(vs) || glCompileShader(fs)) return 2;

    program = glCreateProgram();
    if (!program || glAttachShader(program, vs) || glAttachShader(program, fs) ||
        glLinkProgram(program)) return 3;
    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void *)(3u * sizeof(float)));
    glEnableVertexAttribArray(1);

    for (;;) {
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const void *)0);
        glFlush();
        task_sleep(16);
    }
    return 0;
}
