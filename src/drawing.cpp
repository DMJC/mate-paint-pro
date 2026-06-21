#include "drawing.h"
#include "utils.h"
#include "selection.h"
#include "palette.h"

#include <cmath>
#include <queue>

guint32 read_pixel(int x, int y) {
    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);
    guint32* row = reinterpret_cast<guint32*>(data + y * stride);
    return row[x];
}


void pick_color_at(int x, int y, bool set_background) {
    if (!point_in_canvas(x, y)) return;

    cairo_surface_flush(app_state.surface);
    GdkRGBA sampled = pixel_to_rgba(read_pixel(x, y));
    if (set_background) {
        app_state.bg_color = sampled;
    } else {
        app_state.fg_color = sampled;
    }
    update_color_indicators();
}

void flood_fill_at(int start_x, int start_y) {
    if (!point_in_canvas(start_x, start_y)) return;

    cairo_surface_flush(app_state.surface);
    guint32 target = read_pixel(start_x, start_y);
    guint32 replacement = rgba_to_pixel(get_active_color());
    if (target == replacement) return;

    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);

    std::queue<std::pair<int, int>> pixels;
    pixels.push({start_x, start_y});

    while (!pixels.empty()) {
        std::pair<int, int> current = pixels.front();
        pixels.pop();

        int x = current.first;
        int y = current.second;

        if (!point_in_canvas(x, y)) continue;

        guint32* row = reinterpret_cast<guint32*>(data + y * stride);
        if (row[x] != target) continue;

        row[x] = replacement;
        pixels.push({x - 1, y});
        pixels.push({x + 1, y});
        pixels.push({x, y - 1});
        pixels.push({x, y + 1});
    }

    cairo_surface_mark_dirty(app_state.surface);
}

void gradient_fill_at(int start_x, int start_y, int end_x, int end_y, bool circular_gradient) {
    if (!point_in_canvas(start_x, start_y) || !point_in_canvas(end_x, end_y)) return;

    cairo_surface_flush(app_state.surface);
    guint32 target = read_pixel(start_x, start_y);
    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);

    std::queue<std::pair<int, int>> pixels;
    std::vector<bool> visited(app_state.canvas_width * app_state.canvas_height, false);
    pixels.push({start_x, start_y});
    visited[start_y * app_state.canvas_width + start_x] = true;

    double dx = static_cast<double>(end_x - start_x);
    double dy = static_cast<double>(end_y - start_y);
    double length_sq = dx * dx + dy * dy;
    double radius = std::sqrt(length_sq);

    while (!pixels.empty()) {
        std::pair<int, int> current = pixels.front();
        pixels.pop();

        int x = current.first;
        int y = current.second;
        guint32* row = reinterpret_cast<guint32*>(data + y * stride);
        if (row[x] != target) continue;

        double t = 0.0;
        if (circular_gradient) {
            if (radius > 0.0) {
                t = std::hypot(static_cast<double>(x - start_x), static_cast<double>(y - start_y)) / radius;
            }
        } else if (length_sq > 0.0) {
            t = ((x - start_x) * dx + (y - start_y) * dy) / length_sq;
        }
        t = clamp_double(t, 0.0, 1.0);

        GdkRGBA color;
        color.red = app_state.fg_color.red + (app_state.bg_color.red - app_state.fg_color.red) * t;
        color.green = app_state.fg_color.green + (app_state.bg_color.green - app_state.fg_color.green) * t;
        color.blue = app_state.fg_color.blue + (app_state.bg_color.blue - app_state.fg_color.blue) * t;
        color.alpha = app_state.fg_color.alpha + (app_state.bg_color.alpha - app_state.fg_color.alpha) * t;
        row[x] = rgba_to_pixel(color);

        const int neighbors[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const auto& neighbor : neighbors) {
            int nx = x + neighbor[0];
            int ny = y + neighbor[1];
            if (!point_in_canvas(nx, ny)) continue;

            int index = ny * app_state.canvas_width + nx;
            if (visited[index]) continue;
            visited[index] = true;
            pixels.push({nx, ny});
        }
    }

    cairo_surface_mark_dirty(app_state.surface);
}

bool pixel_matches_with_tolerance(guint32 pixel, guint32 target, int tolerance) {
    int pixel_a = (pixel >> 24) & 0xFF;
    int pixel_r = (pixel >> 16) & 0xFF;
    int pixel_g = (pixel >> 8) & 0xFF;
    int pixel_b = pixel & 0xFF;

    int target_a = (target >> 24) & 0xFF;
    int target_r = (target >> 16) & 0xFF;
    int target_g = (target >> 8) & 0xFF;
    int target_b = target & 0xFF;

    return std::abs(pixel_a - target_a) <= tolerance &&
           std::abs(pixel_r - target_r) <= tolerance &&
           std::abs(pixel_g - target_g) <= tolerance &&
           std::abs(pixel_b - target_b) <= tolerance;
}

bool select_pixels_by_color(int start_x, int start_y, bool contiguous_only, int tolerance) {
    if (!point_in_canvas(start_x, start_y) || !app_state.surface) return false;

    cairo_surface_flush(app_state.surface);
    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);
    guint32 target = read_pixel(start_x, start_y);

    bool found = false;
    int min_x = app_state.canvas_width;
    int min_y = app_state.canvas_height;
    int max_x = 0;
    int max_y = 0;

    std::vector<bool> selection_mask(app_state.canvas_width * app_state.canvas_height, false);

    if (contiguous_only) {
        std::queue<std::pair<int, int>> pixels;
        std::vector<bool> visited(app_state.canvas_width * app_state.canvas_height, false);
        pixels.push({start_x, start_y});
        visited[start_y * app_state.canvas_width + start_x] = true;

        while (!pixels.empty()) {
            std::pair<int, int> current = pixels.front();
            pixels.pop();

            int x = current.first;
            int y = current.second;
            guint32* row = reinterpret_cast<guint32*>(data + y * stride);
            if (!pixel_matches_with_tolerance(row[x], target, tolerance)) continue;

            found = true;
            selection_mask[y * app_state.canvas_width + x] = true;
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x);
            max_y = std::max(max_y, y);

            const int neighbors[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (const auto& neighbor : neighbors) {
                int nx = x + neighbor[0];
                int ny = y + neighbor[1];
                if (!point_in_canvas(nx, ny)) continue;

                int index = ny * app_state.canvas_width + nx;
                if (visited[index]) continue;
                visited[index] = true;
                pixels.push({nx, ny});
            }
        }
    } else {
        for (int y = 0; y < app_state.canvas_height; y++) {
            guint32* row = reinterpret_cast<guint32*>(data + y * stride);
            for (int x = 0; x < app_state.canvas_width; x++) {
                if (!pixel_matches_with_tolerance(row[x], target, tolerance)) continue;
                found = true;
                selection_mask[y * app_state.canvas_width + x] = true;
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }

    if (!found) {
        clear_selection();
        return false;
    }

    app_state.has_selection = true;
    app_state.selection_is_rect = true;
    app_state.floating_selection_active = false;
    app_state.selection_path.clear();
    app_state.selection_mask = std::move(selection_mask);
    app_state.selection_has_mask = true;
    app_state.selection_x1 = min_x;
    app_state.selection_y1 = min_y;
    app_state.selection_x2 = max_x + 1;
    app_state.selection_y2 = max_y + 1;
    start_ant_animation();
    return true;
}

// Drawing functions for each tool
void draw_line(cairo_t* cr, double x1, double y1, double x2, double y2) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, app_state.line_width);
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    cairo_stroke(cr);
}

void draw_rectangle(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);

    double x = fmin(x1, x2);
    double y = fmin(y1, y2);
    double w = fabs(x2 - x1);
    double h = fabs(y2 - y1);

    cairo_rectangle(cr, x, y, w, h);

    if (filled) {
        cairo_fill(cr);
    } else {
        cairo_set_line_width(cr, app_state.line_width);
        cairo_stroke(cr);
    }
}

void draw_ellipse(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);

    double cx = (x1 + x2) / 2.0;
    double cy = (y1 + y2) / 2.0;
    double rx = fabs(x2 - x1) / 2.0;
    double ry = fabs(y2 - y1) / 2.0;

    if (rx < 0.1 || ry < 0.1) return;

    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx, ry);
    cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
    cairo_restore(cr);

    if (filled) {
        cairo_fill(cr);
    } else {
        cairo_set_line_width(cr, app_state.line_width);
        cairo_stroke(cr);
    }
}

void draw_rounded_rectangle(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);

    double x = fmin(x1, x2);
    double y = fmin(y1, y2);
    double w = fabs(x2 - x1);
    double h = fabs(y2 - y1);
    double r = fmin(w, h) * 0.1;

    if (w < 1 || h < 1) return;

    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_arc(cr, x + w - r, y + r, r, 3 * M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_close_path(cr);

    if (filled) {
        cairo_fill(cr);
    } else {
        cairo_set_line_width(cr, app_state.line_width);
        cairo_stroke(cr);
    }
}

void draw_polygon(cairo_t* cr, const std::vector<std::pair<double, double>>& points) {
    if (points.size() < 2) return;

    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, app_state.line_width);

    cairo_move_to(cr, points[0].first, points[0].second);
    for (size_t i = 1; i < points.size(); i++) {
        cairo_line_to(cr, points[i].first, points[i].second);
    }
    cairo_close_path(cr);
    cairo_stroke(cr);
}

void draw_regular_polygon(cairo_t* cr, const std::vector<std::pair<double, double>>& points) {
    draw_polygon(cr, points);
}

void draw_curve(cairo_t* cr, double start_x, double start_y, double control_x, double control_y, double end_x, double end_y) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, app_state.line_width);
    cairo_move_to(cr, start_x, start_y);
    cairo_curve_to(cr, control_x, control_y, control_x, control_y, end_x, end_y);
    cairo_stroke(cr);
}

void draw_pencil(cairo_t* cr, double x, double y) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (app_state.last_x != 0 && app_state.last_y != 0) {
        cairo_move_to(cr, app_state.last_x, app_state.last_y);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);
    }
}

void draw_paintbrush(cairo_t* cr, double x, double y) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, app_state.line_width * 2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (app_state.last_x != 0 && app_state.last_y != 0) {
        cairo_move_to(cr, app_state.last_x, app_state.last_y);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);
    }
}

void draw_airbrush(cairo_t* cr, double x, double y) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    double spray_radius = app_state.line_width * 5.0;

    for (int i = 0; i < 20; i++) {
        double angle = g_random_double() * 2 * M_PI;
        double radius = g_random_double() * spray_radius;
        int px = static_cast<int>(std::round(x + cos(angle) * radius));
        int py = static_cast<int>(std::round(y + sin(angle) * radius));
        cairo_rectangle(cr, px, py, 1, 1);
    }
    cairo_fill(cr);
}

void draw_eraser(cairo_t* cr, double x, double y) {
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_line_width(cr, app_state.line_width * 3);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (app_state.last_x != 0 && app_state.last_y != 0) {
        cairo_move_to(cr, app_state.last_x, app_state.last_y);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
}

void draw_smudge(double x, double y) {
    if (app_state.last_x == 0 && app_state.last_y == 0) {
        return;
    }

    int source_x = static_cast<int>(std::round(app_state.last_x));
    int source_y = static_cast<int>(std::round(app_state.last_y));
    int target_x = static_cast<int>(std::round(x));
    int target_y = static_cast<int>(std::round(y));

    cairo_surface_flush(app_state.surface);
    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);

    const int radius = static_cast<int>(std::round(app_state.line_width * 2.0));
    const double strength = 0.35;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }

            int sx = source_x + dx;
            int sy = source_y + dy;
            int tx = target_x + dx;
            int ty = target_y + dy;

            if (!point_in_canvas(sx, sy) || !point_in_canvas(tx, ty)) {
                continue;
            }

            guint32* source_row = reinterpret_cast<guint32*>(data + sy * stride);
            guint32* target_row = reinterpret_cast<guint32*>(data + ty * stride);
            GdkRGBA source = pixel_to_rgba(source_row[sx]);
            GdkRGBA target = pixel_to_rgba(target_row[tx]);

            GdkRGBA mixed;
            mixed.red = target.red + (source.red - target.red) * strength;
            mixed.green = target.green + (source.green - target.green) * strength;
            mixed.blue = target.blue + (source.blue - target.blue) * strength;
            mixed.alpha = target.alpha + (source.alpha - target.alpha) * strength;

            target_row[tx] = rgba_to_pixel(mixed);
        }
    }

    cairo_surface_mark_dirty(app_state.surface);
}
