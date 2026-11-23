#include <window.h>
#include <task.h>

static void draw_demo(window_t window, int width, int height)
{
    window_draw_clear(window, 0x001D2633u);
    window_draw_text(window, 14, 14, "Hello from TCC!", 0x00FFFFFFu);
    window_draw_text(window, 14, 34, "This is an IPSOS window.", 0x00B8D8FFu);
    window_draw_rect(window, 14, 64, width - 28, 3, 0x003D91C7u);
    window_draw_rect_outline(window, 14, 82, width - 28, height - 98,
                             0x007F9DB9u);
}

int main(void)
{
    window_config_t config = {
        .title = "TCC Window Demo",
        .x = 120,
        .y = 100,
        .width = 420,
        .height = 280,
        .visible = 1
    };
    window_t window = window_create(&config);

    if (!window)
        return 1;

    window_set_render_callback(window, draw_demo);
    while (window_is_open(window))
        task_sleep(20);

    window_destroy(window);
    return 0;
}
