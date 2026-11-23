# IPSOS Graphics API

Header: `gfx.h` (available to programs compiled by the built-in TCC in `/include/gfx.h`). The public API contains more than 15 core functions, so this reference is kept in the project root.

The display backend is VirtIO-GPU. The 2D API creates a GPU resource backed by page-aligned guest RAM and assigns it to scanout 0. Primitive rasterization, 8x16 bitmap font rendering, and decoded-image blits are still performed by the CPU. `gfx_present()` transfers only merged dirty rectangles when possible, then flushes those regions.

When QEMU exposes VirGL, the driver negotiates the VIRGL feature, queries the VirGL capset, creates a context, and exposes raw Gallium/VirGL command submission. `gl.h` currently implements a deliberately small accelerated subset: context setup, viewport, color clear, and flush. It is not a complete Khronos OpenGL implementation; shader compilation, general draw calls, depth buffers, and GL texture upload are not implemented yet. See `API_VIRGL_GL.md`.

The desktop uses the VirtIO-GPU hardware cursor queue when the device supports it, with a software cursor fallback. VBE BIOS mode setting and the VBE driver have been removed.

## Color format

Colors use `0x00RRGGBB`. The top byte is unused. Use `GFX_RGB(r, g, b)` to construct a color. For example:

```c
uint32_t orange = GFX_RGB(255, 128, 0);
gfx_draw_rect(20, 20, 100, 50, orange);
```

## Image structure

```c
typedef struct GfxImage {
    uint32_t width;
    uint32_t height;
    const uint32_t *pixels;
    uint32_t flags;
    uint32_t transparent_color;
} GfxImage;
```

`pixels` points to `width * height` row-major 32-bit pixels in `0x00RRGGBB` format. Set `flags` to `GFX_IMAGE_COLORKEY` to skip pixels matching `transparent_color`. The pixel storage remains owned by the caller and must stay valid while the image is drawn. This function draws an already-decoded pixel buffer; it does not decode PNG, JPEG, or BMP files.

## API

| Function | Description |
|---|---|
| `int gfx_init(void)` | Initializes the VirtIO-GPU PCI device, control queue, 2D resource and scanout. Returns `0` on success and `-1` on failure. |
| `int gfx_present(void)` | Transfers merged dirty regions to the host GPU and flushes them to scanout. A full clear marks the whole screen dirty. Call after a batch of drawing operations. |
| `uint32_t gfx_driver_error(void)` | Returns the last VirtIO-GPU initialization/command error code; `0` means no recorded error. |
| `int gfx_ready(void)` | Returns nonzero when the renderer is initialized. |
| `uint32_t gfx_width(void)` | Screen width in pixels. |
| `uint32_t gfx_height(void)` | Screen height in pixels. |
| `void gfx_clear(uint32_t color)` | Fills the complete screen; ignores the current clip rectangle. |
| `void gfx_draw_pixel(int x, int y, uint32_t color)` | Draws one pixel. |
| `uint32_t gfx_get_pixel(int x, int y)` | Reads one pixel; returns zero for invalid coordinates. Reading is not restricted by the clip rectangle. |
| `void gfx_draw_hline(int x, int y, int width, uint32_t color)` | Draws a horizontal line. |
| `void gfx_draw_vline(int x, int y, int height, uint32_t color)` | Draws a vertical line. |
| `void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color)` | Draws a clipped line using Bresenham rasterization. |
| `void gfx_draw_rect(int x, int y, int width, int height, uint32_t color)` | Draws a filled rectangle. This preserves the historical IPSOS meaning of `gfx_draw_rect`. |
| `void gfx_draw_square(int x, int y, int size, uint32_t color)` | Draws a filled square. |
| `void gfx_draw_rect_outline(int x, int y, int width, int height, uint32_t color)` | Draws only the rectangle border. |
| `void gfx_draw_circle(int cx, int cy, int radius, uint32_t color)` | Draws a circle outline. |
| `void gfx_fill_circle(int cx, int cy, int radius, uint32_t color)` | Draws a filled circle. |
| `void gfx_draw_char(int x, int y, char character, uint32_t color)` | Draws one transparent 8×16 glyph from the built-in bitmap font. |
| `void gfx_draw_char_bg(int x, int y, char character, uint32_t foreground, uint32_t background)` | Draws an 8×16 glyph over a solid background. |
| `void gfx_draw_text(int x, int y, const char *text, uint32_t color)` | Draws text using the 8×16 font. Supports newline and tab. |
| `int gfx_draw_image(int x, int y, const GfxImage *image)` | Draws a row-major 32-bit image. Returns `0` on success and `-1` for invalid input. |
| `void gfx_blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y)` | Copies a rectangle within the framebuffer. Overlapping copies are handled safely. |
| `void gfx_set_clip(int x, int y, int width, int height)` | Sets the global drawing clip rectangle, intersected with screen bounds. |
| `void gfx_reset_clip(void)` | Resets clipping to the full screen. |
| `void gfx_get_clip(int *x, int *y, int *width, int *height)` | Reads the current global clip rectangle. |
| `int gfx_cursor_hardware_ready(void)` | Returns nonzero when the VirtIO-GPU hardware cursor queue is active. |
| `int gfx_cursor_move(int x, int y)` | Moves the hardware cursor when available. |

## Example: draw an image

```c
#include <gfx.h>

static const uint32_t pixels[] = {
    GFX_RGB(255, 0, 0), GFX_RGB(0, 255, 0),
    GFX_RGB(0, 0, 255), GFX_RGB(255, 255, 0)
};

void draw_demo(void)
{
    GfxImage image = {2, 2, pixels, 0, 0};
    gfx_draw_rect(10, 10, 80, 40, GFX_RGB(32, 48, 64));
    gfx_draw_text(14, 14, "IPSOS graphics", GFX_RGB(255, 255, 255));
    gfx_draw_image(100, 10, &image);
}
```

## VirtIO-GPU diagnostics

`gfx_driver_error()` is useful when `gfx_init()` fails. The driver reports these local codes through the QEMU debug console (`-debugcon stdio`) and through the API:

| Code | Meaning |
|---|---|
| `0xE010` | VirtIO-GPU PCI device `1AF4:1050` not found |
| `0xE011` | Required modern VirtIO PCI common/notify capabilities not found |
| `0xE012` | VirtIO PCI capability region is too small |
| `0xE013` | Device does not offer `VIRTIO_F_VERSION_1` |
| `0xE014` | Device rejected negotiated features |
| `0xE015` | Control virtqueue setup failed |
| `0xE016` | Display-info query failed or scanout 0 is disabled |
| `0xE017` | Reported display dimensions are unsupported |
| `0xE018` | Display backing buffer exceeds the configured limit |
| `0xE019` | Guest RAM allocation for the backing buffer failed |
| `0xE01A` | GPU resource creation, backing attachment, or scanout setup failed |
| `0xE01B` | Invalid notification multiplier |
| `0xE01C` | VirtIO device reset timed out |
| `0xE001` | GPU command timed out |
| `0xE002` | GPU returned an unexpected virtqueue descriptor ID |
| `0xE003` | GPU response was shorter than the expected response |
| `0xE004` | GPU returned an unexpected response type |

GPU protocol errors in the `0x1200` range are preserved as returned by the device.

## Compatibility functions

`gfx_clear_screen`, `gfx_print_char`, `gfx_print_gr_cl`, and `gfx_print_char_gr_cl` remain available for older IPSOS programs. New programs should use `gfx_clear`, `gfx_draw_char`, and `gfx_draw_text`.

## Important notes

- Call `gfx_init()` before using the renderer. The kernel does this during startup; VBE initialization is no longer part of the graphics path.
- `gfx_present()` is not needed after a hardware cursor move; cursor position is updated on the separate VirtIO cursor queue.
- Terminal cell updates and window movement invalidate bounded regions. The compositor repaints only those damaged regions, and the renderer transfers the resulting merged rectangles. A full desktop invalidation still repaints the full screen.
- Do not mix VirGL rendering and CPU `gfx_*` drawing on the same display surface without an explicit synchronization/readback path. `gfx_present()` transfers the CPU backing store and can overwrite a preceding GPU-side GL clear.
- `gfx_set_clip` changes global renderer state. Code that changes it must call `gfx_reset_clip` when finished. The window manager sets a clip rectangle around each window's content renderer.
- `gfx_clear` clears the entire display even when clipping is active.
- Drawing operations update the guest-RAM surface. Call `gfx_present()` after drawing to publish the changes to QEMU. Direct drawing by an application can be overwritten by the next compositor repaint; window content should be drawn from its `WindowRenderFn` callback registered with `wm_set_renderer`.
- Renderer state, including the clip rectangle, is global. Do not draw from multiple tasks concurrently without coordinating them.
- The built-in font is a byte-indexed 8×16 bitmap font; `gfx_draw_text` does not decode UTF-8.
- The Makefile `run` target uses `-device virtio-gpu-gl -display gtk,gl=on` for VirGL. If host GL support is unavailable, use `make run QEMU_GPU=virtio-gpu-pci QEMU_DISPLAY=gtk` for 2D fallback. An ordinary `-vga std` device does not provide this backend.
- The framebuffer is a guest-RAM VirtIO resource; its backing allocation is placed above 1 MiB and the heap skips the reserved PC memory hole at 0xA0000–0xFFFFF; there is no direct write to a VBE physical framebuffer. The host GPU/display backend performs the final scanout.
