# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Mate-Paint Pro is a GTK+3 paint/image-editing application for the MATE Desktop, written in C++11. The codebase uses a global `AppState` struct and free functions — there are no classes or OOP patterns.

## Build Commands

```bash
meson setup build          # first-time setup
ninja -C build             # build
sudo ninja -C build install  # install (needed for icon paths to resolve)
./build/mate-paint-pro     # run without installing (icons may not load)
```

Dependencies: `gtk+-3.0`, `pkg-config`, `meson`, `ninja`.

## Source Layout

All source files live in `src/`:

| File | Purpose |
|------|---------|
| `app_state.h` | Central types: `Tool` enum, `Layer`, `AppState`, structs, constants, `extern app_state` |
| `main.cpp` | `main()`, window/toolbar/palette/layer panel setup |
| `utils.*` | Coordinate math, color conversion, tool queries, surface helpers, shape geometry |
| `events.*` | GTK input handlers: button press/release, motion, key press/release |
| `drawing.*` | Drawing primitives: pencil, brush, airbrush, eraser, smudge, shapes, fill, color pick |
| `rendering.*` | `on_draw` pipeline, preview overlays, guides, grid background |
| `selection.*` | Selection ops, clipboard, floating selection, mask, marching ants |
| `layers.*` | Layer add/delete/move/merge/duplicate, panel UI, compositing |
| `undo.*` | Undo/redo stack management |
| `filters.*` | Image filters (blur, sharpen, noise, cartoonify), color balance, brightness/contrast, line patterns |
| `file_io.*` | Save/open dialogs, file format handling, new canvas dialog |
| `image_ops.*` | Scale, resize, rotate, flip, crop |
| `text_tool.*` | Text tool window, finalize/cancel text |
| `palette.*` | Color palette UI, custom palette persistence, color button creation |
| `ui_widgets.*` | Tool buttons, line thickness/zoom buttons, polygon/star side dialogs |
| `menus.*` | Menu bar construction, menu callbacks, view/guide toggles |

## Architecture

**Global state**: A single `AppState app_state` global (defined in `main.cpp`) holds all mutable state — canvas surfaces, tool selection, UI widget pointers, selection/clipboard/undo state, layer data, and zoom/guide settings.

**Tool system**: The `Tool` enum defines all tools (pencil, brush, eraser, shapes, selection, crop, text, etc.). Tool behavior is dispatched through `on_button_press`, `on_motion_notify`, and `on_button_release` event handlers via switch/if chains on `app_state.current_tool`.

**Layer system**: `std::vector<Layer>` in AppState. Each layer owns a `cairo_surface_t*`. Compositing happens in `compose_visible_layers_surface()`. The active layer's surface is the direct drawing target for all tools.

**Selection system**: Supports rectangular, lasso, fuzzy (flood-fill), and color-based selection. Selections can become floating (copied pixels that can be dragged). A pixel-level `selection_mask` vector enables non-rectangular selections. Marching ants animation via `ant_path_timer`.

**Undo/redo**: Snapshot-based — `push_undo_state()` copies the entire active layer surface. Capped at 50 entries (`max_undo_steps`).

**Rendering pipeline**: `on_draw` composites: grid background → layers (bottom-to-top with opacity) → selection overlay → text overlay → preview shapes → hover indicator → guides. All drawing uses Cairo.

**Filters**: Convolution-based (blur, sharpen) via `apply_convolution_to_active_layer`, plus noise and cartoonify. Color balance and brightness/contrast use live-preview dialogs with original-surface snapshots.

**i18n**: Uses gettext with `_()` macro. Translations in `po/`. Text domain is `mate-paint`. All user-visible strings must be wrapped in `_()`.

**Config persistence**: Custom palette colors saved to `~/.config/mate/mate-paint-pro/mate-paint.cfg` via GKeyFile.

## Key Conventions

- All UI strings must use `_("...")` for translation
- Cairo surfaces use ARGB32 format with premultiplied alpha
- Pixel manipulation uses `guint32` with layout: `(A << 24) | (R << 16) | (G << 8) | B`
- Shape constraint (shift key): lines snap to 45° angles, ellipses become circles, rectangles become squares
- The compile defines `ICON_INSTALL_DIR`, `GETTEXT_PACKAGE`, and `LOCALEDIR` are set in `meson.build`
