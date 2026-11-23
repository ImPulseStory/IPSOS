#include <gl.h>
#include <gfx.h>
#include <task.h>
#include <window.h>
#include <stdint.h>

/* The shader is TGSI, which VirGL consumes natively. It is not GLSL. */
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

typedef struct {
    float x, y, z, w;
    float r, g, b, a;
} Vertex;

static const float cube_positions[8][3] = {
    {-0.65f, -0.65f, -0.65f}, { 0.65f, -0.65f, -0.65f},
    { 0.65f,  0.65f, -0.65f}, {-0.65f,  0.65f, -0.65f},
    {-0.65f, -0.65f,  0.65f}, { 0.65f, -0.65f,  0.65f},
    { 0.65f,  0.65f,  0.65f}, {-0.65f,  0.65f,  0.65f}
};

static const float cube_colors[8][4] = {
    {1.0f, 0.25f, 0.20f, 1.0f}, {1.0f, 0.70f, 0.20f, 1.0f},
    {0.25f, 0.85f, 0.35f, 1.0f}, {0.20f, 0.55f, 1.0f, 1.0f},
    {0.85f, 0.25f, 0.95f, 1.0f}, {0.20f, 0.85f, 0.85f, 1.0f},
    {0.95f, 0.90f, 0.25f, 1.0f}, {0.95f, 0.45f, 0.55f, 1.0f}
};

static const uint16_t cube_indices[36] = {
    4, 5, 6, 4, 6, 7, /* front */
    1, 0, 3, 1, 3, 2, /* back */
    0, 4, 7, 0, 7, 3, /* left */
    5, 1, 2, 5, 2, 6, /* right */
    3, 7, 6, 3, 6, 2, /* top */
    0, 1, 5, 0, 5, 4  /* bottom */
};

/* A fixed model rotation plus a perspective projection, calculated on CPU.
   Vertex transformation is intentionally kept out of the first TGSI shader. */
static void build_projected_cube(Vertex *out)
{
    const float sin_x = 0.50f, cos_x = 0.8660254f;
    const float sin_y = 0.60f, cos_y = 0.80f;
    const float focal = 1.45f;
    const float near_z = 0.10f, far_z = 10.0f;
    const float proj_a = (far_z + near_z) / (near_z - far_z);
    const float proj_b = (2.0f * far_z * near_z) / (near_z - far_z);

    for (uint32_t i = 0; i < 8; ++i) {
        float x = cube_positions[i][0];
        float y = cube_positions[i][1];
        float z = cube_positions[i][2];
        float y1 = y * cos_x - z * sin_x;
        float z1 = y * sin_x + z * cos_x;
        float tx = x * cos_y + z1 * sin_y;
        float rz = -x * sin_y + z1 * cos_y;
        float view_z = rz - 3.0f;
        float clip_w = -view_z;

        out[i].x = focal * tx;
        out[i].y = focal * y1;
        out[i].z = proj_a * view_z + proj_b;
        out[i].w = clip_w;
        out[i].r = cube_colors[i][0];
        out[i].g = cube_colors[i][1];
        out[i].b = cube_colors[i][2];
        out[i].a = cube_colors[i][3];
    }
}

static window_t cube_window;
static const char *window_status = "Initializing VirGL...";

static void draw_cube_window_2d(window_t window, int width, int height)
{
    (void)width;
    (void)height;
    window_draw_clear(window, 0x00101828u);
    window_draw_text(window, 8, 8, window_status, 0x00D8E8FFu);
}

/* This callback runs after the software desktop has been presented. The
   current GL backend renders into the shared scanout resource, not a private
   per-window texture, so the viewport is restricted to this client rectangle. */
static void render_cube_window(window_t window, int client_x, int client_y,
                               int client_width, int client_height)
{
    (void)window;
    if (!glIsReady()) return;

    /* OpenGL viewport coordinates use a bottom-left origin. */
    glViewport(client_x,
               (GLint)gfx_height() - client_y - client_height,
               client_width, client_height);

    /* Color is painted by the 2D WM pass. Clearing color here would clear the
       entire shared scanout, because this GL subset has no scissor test yet. */
    glClear(GL_DEPTH_BUFFER_BIT);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, (const void *)0);
    glFlush();
}

static void wait_until_window_closes(void)
{
    while (window_is_open(cube_window)) {
        window_invalidate(cube_window);
        task_sleep(100);
    }
}

int main(void)
{
    Vertex vertices[8];
    GLuint buffers[2], vs, fs, program;
    window_config_t config;

    config.title = "IPSOS 3D Cube";
    config.x = 180;
    config.y = 100;
    config.width = 560;
    config.height = 440;
    config.visible = 1;
    cube_window = window_create(&config);
    if (!cube_window) {
        return 1;
    }
    window_set_render_callback(cube_window, draw_cube_window_2d);

    if (glInit() != 0) {
        window_status = "VirGL initialization failed";
        window_set_title(cube_window, "IPSOS 3D Cube - VirGL unavailable");
        wait_until_window_closes();
        return 1;
    }

    build_projected_cube(vertices);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClearDepth(1.0);

    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

    vs = glCreateShader(GL_VERTEX_SHADER);
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!vs || !fs || glShaderSourceTGSI(vs, vertex_tgsi) != 0 ||
        glShaderSourceTGSI(fs, fragment_tgsi) != 0 ||
        glCompileShader(vs) != 0 || glCompileShader(fs) != 0) {
        window_status = "Shader compilation failed";
        wait_until_window_closes();
        return 2;
    }

    program = glCreateProgram();
    if (!program || glAttachShader(program, vs) != 0 ||
        glAttachShader(program, fs) != 0 || glLinkProgram(program) != 0) {
        window_status = "Shader link failed";
        wait_until_window_closes();
        return 3;
    }
    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (const void *)(4u * sizeof(float)));
    glEnableVertexAttribArray(1);

    window_status = "Rendering on VirGL";
    window_set_gl_render_callback(cube_window, render_cube_window);

    /* The compositor invokes render_cube_window after uploading its 2D frame.
       Invalidating the client area requests the next frame. */
    while (window_is_open(cube_window)) {
        window_invalidate(cube_window);
        task_sleep(16);
    }

    window_destroy(cube_window);
    return 0;
}
