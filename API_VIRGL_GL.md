# IPSOS VirGL / GL API

## Current status

IPSOS negotiates the VirtIO-GPU VIRGL feature, queries the VirGL capset, creates a
VirGL context, and submits Gallium/VirGL command dwords through `SUBMIT_3D`.

The GL bridge now has a first programmable triangle pipeline:

- buffer resources: `glGenBuffers`, `glBindBuffer`, `glBufferData`, `glBufferSubData`;
- vertex inputs: `glVertexAttribPointer`, `glEnableVertexAttribArray`;
- TGSI shader objects: `glCreateShader`, `glShaderSourceTGSI`, `glCompileShader`;
- guest-side program pairing: `glCreateProgram`, `glAttachShader`, `glLinkProgram`, `glUseProgram`;
- primitive submission: `glDrawArrays` and `glDrawElements` for points, lines, line loops/strips, triangles, triangle strips and fans;
- index buffers: `GL_ELEMENT_ARRAY_BUFFER` with unsigned 8-, 16- or 32-bit indices;
- fixed blend/rasterizer state and a `Z32_FLOAT` depth attachment;
- depth clear and depth-test state: `glClearDepth`, `glEnable(GL_DEPTH_TEST)`,
  `glDisable(GL_DEPTH_TEST)`, `glDepthFunc`, and `glDepthMask`.

**Important scope boundary:** this is an experimental OpenGL-style subset, not a
complete Khronos OpenGL implementation. VirGL's shader protocol consumes TGSI,
not GLSL. `glShaderSourceTGSI()` makes this explicit; a GLSL parser/compiler is
not included. The current milestone supports `GL_FLOAT` vertex attributes, array and element
buffers, non-indexed and indexed draws, vertex/fragment TGSI shader stages, a
color target, a depth target and one context. Depth testing uses a `Z32_FLOAT`
attachment. It does not provide matrix stacks, uniform uploads, texture/sampler
objects, configurable blend functions, scissor state, shader logs or a full GL
state machine. Buffer objects are statically bounded (8 buffers, 8 shaders and
8 programs) and cannot yet be resized or explicitly deleted. `glLinkProgram()`
validates and pairs the two TGSI shader stages on the CPU; the driver binds those
stages separately as expected by VirGL. Vertex shaders must currently output
clip-space positions themselves; there is no GLSL compiler or matrix/uniform API.

The display surface currently targets the VirtIO-GPU display resource directly.
The CPU compositor can overwrite GPU-rendered pixels when it repaints the same
area, so this is currently a direct-display rendering test path, not yet a
composited per-window GL context.

See `examples/triangle3d.c` for a basic colored triangle,
`examples/indexed_triangle3d.c` for indexed geometry, and `examples/cube3d.c`
for a perspective-projected cube using the GPU depth buffer. The cube's vertex
transformation is calculated on the CPU; vertex processing, clipping, rasterizing,
fragment shading and depth testing are performed by the VirGL pipeline.

## QEMU setup

The default `make run` target uses:

```sh
-device virtio-gpu-gl,xres=1280,yres=800,edid=on -display gtk,gl=on,zoom-to-fit=off
```

This requires a QEMU build with VirGL support and a host display backend capable of GL. For 2D-only fallback:

```sh
make run QEMU_GPU=virtio-gpu-pci,xres=1280,yres=800,edid=on QEMU_DISPLAY=gtk,zoom-to-fit=off
```

In 2D-only mode `gfx_*` continues to work, but `virgl_init()` / `glInit()` report that VirGL is unavailable.

## VirGL API (`virgl.h`)

- `virgl_init()` — query capset and create the default context (ID 1).
- `virgl_ready()` — report context readiness.
- `virgl_get_caps(VirglCaps *)` — return capset ID, maximum version and size.
- `virgl_get_capset(version, buffer, capacity, actual_size)` — retrieve the host VirGL capset.
- `virgl_context_create()` / `virgl_context_destroy()` — context lifecycle primitives.
- `virgl_resource_create_3d()` — issue `RESOURCE_CREATE_3D`.
- `virgl_resource_attach()` — attach a resource to a context.
- `virgl_submit()` — submit a bounded dword stream using `SUBMIT_3D`.
- `virgl_last_error()` — report the last VirGL initialization/transport error.

The current kernel uses one default context (ID 1). Resource format, target and bind values are Gallium/VirGL protocol values, not arbitrary OpenGL enums.

## GL bridge (`gl.h`)

The GL bridge exposes an experimental buffer/shader/program/draw API. The shader text entry point is `glShaderSourceTGSI`; it accepts TGSI, not GLSL. Indexed drawing is available through `glDrawElements`; the `indices` argument is a byte offset into the currently bound `GL_ELEMENT_ARRAY_BUFFER`, not a host pointer. A `Z32_FLOAT` depth attachment is available through `glEnable(GL_DEPTH_TEST)`, `glDepthFunc`, `glDepthMask`, and `glClear(GL_DEPTH_BUFFER_BIT)`. Uniforms, textures, a GLSL compiler, matrix helpers and full OpenGL state are not implemented.

## 2D API (`gfx.h`)

The 2D API remains backend-independent. It provides rectangles, lines, circles, 8x16 bitmap text, decoded 32-bit images, color-key transparency, clipping, blitting, and `gfx_present()`.

Image input is a decoded `GfxImage` pixel array. PNG/JPEG/BMP file decoding is not part of the graphics driver.

## Dirty rectangles

The software 2D renderer records up to 16 merged dirty rectangles. `gfx_present()` transfers and flushes only those regions. A full clear marks the whole screen dirty. If too many regions accumulate, they are coalesced into one bounding rectangle to keep the command count bounded.

## Hardware cursor

The driver configures VirtIO-GPU queue 1 when available, uploads a 64x64 cursor resource, then uses `UPDATE_CURSOR` / `MOVE_CURSOR`. If the device does not expose a working cursor queue, the desktop keeps its software cursor fallback.

## VBE removal

The bootloader no longer calls VBE BIOS services. The old VBE driver/header and TCC `vbe_get_width/height` symbols have been removed. Screen dimensions now come from VirtIO-GPU display information.
