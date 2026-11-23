# IPSOS Window API for TCC

`<window.h>` exposes the Window Manager without exposing its internal `Window_t`
layout. Applications receive an opaque `window_t` handle and draw in client-area
coordinates (the origin is below the title bar).

## Minimal example

```c
#include <window.h>
#include <task.h>

static void paint(window_t win, int width, int height)
{
    window_draw_clear(win, 0x001D2633);
    window_draw_text(win, 12, 12, "Hello IPSOS!", 0x00FFFFFF);
    window_draw_rect(win, 12, 40, width - 24, 3, 0x003D91C7);
}

int main(void)
{
    window_config_t config = {
        .title = "My first window",
        .x = 100, .y = 100,
        .width = 400, .height = 260,
        .visible = 1
    };
    window_t win = window_create(&config);
    if (!win) return 1;

    window_set_render_callback(win, paint);
    while (window_is_open(win))
        task_sleep(20);

    window_destroy(win);
    return 0;
}
```

Compile and run from IPSOS:

```sh
tcc window_demo.c -o window_demo
window_demo
```

## API

- `window_create(config)` — create a window; `NULL` config uses a default 320x220 window.
- `window_destroy(win)` — remove it from the WM and release its API slot.
- `window_show` / `window_hide` — show or hide it.
- `window_is_open` — returns nonzero while visible; the title-bar close button hides it.
- `window_set_title`, `window_set_position`, `window_set_size` — update window properties.
- `window_get_client_size` — retrieve drawable client dimensions.
- `window_set_render_callback` — register a callback called by the compositor when the client area needs painting.
- `window_invalidate` — request client-area repaint.
- `window_draw_*` — draw relative to the client-area origin; the compositor's clip prevents drawing outside the damaged client region.

The current API provides drawing and visibility/lifecycle operations. Per-window
keyboard and mouse event queues, independent pixel buffers, and a full event loop
are not implemented yet. Rendering currently uses the compositor's shared
framebuffer with clipping, not an isolated backbuffer per application. Render
callbacks run on the compositor task and must be short and non-blocking; do not
call `task_sleep` or wait for input from inside a render callback.

When an ELF application launched through `exec` returns, IPSOS automatically
destroys windows owned by that task so the compositor cannot call back into an
application that has exited.

## VirGL-backed window rendering

A window may also register a GPU callback with `window_set_gl_render_callback()`.
Unlike the normal 2D render callback, this callback runs **after** the compositor
uploads its software framebuffer. It receives the window handle and the client
rectangle in screen coordinates. Convert the top-left-origin client Y coordinate
to the OpenGL bottom-left origin before calling `glViewport()`:

```c
static void render_gl(window_t window, int x, int y, int width, int height)
{
    (void)window;
    glViewport(x, (GLint)gfx_height() - y - height, width, height);
    glClear(GL_DEPTH_BUFFER_BIT);
    /* Submit GL draw calls, then glFlush(). */
}
```

Current limitation: the VirGL backend renders into the shared scanout resource,
not a private per-window texture. The viewport confines geometry to the client
area, but GPU windows are currently composed as a topmost GPU layer; exact
interleaving with ordinary 2D windows by Z-order and per-window offscreen
framebuffers are not implemented yet. Do not clear `GL_COLOR_BUFFER_BIT` from
this callback: color clear currently affects the whole scanout resource.
