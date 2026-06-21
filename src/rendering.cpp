#include "rendering.h"
#include "utils.h"
#include "drawing.h"
#include "selection.h"

#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

// Draw text box overlay
void draw_text_overlay(cairo_t* cr) {
    if (!app_state.text_active) return;

    // Draw text box with ant path
    draw_ant_path(cr);
    cairo_rectangle(cr, app_state.text_x, app_state.text_y,
                   app_state.text_box_width, app_state.text_box_height);
    cairo_stroke(cr);

    // Draw text preview
    if (!app_state.text_content.empty()) {
        cairo_select_font_face(cr, app_state.text_font_family.c_str(),
                              CAIRO_FONT_SLANT_NORMAL,
                              CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, app_state.text_font_size);
        cairo_set_source_rgba(cr,
            app_state.fg_color.red,
            app_state.fg_color.green,
            app_state.fg_color.blue,
            app_state.fg_color.alpha
        );

        // Simple preview (first line)
        std::string text = app_state.text_content;
        double y = app_state.text_y + app_state.text_font_size + 5;
        double x = app_state.text_x + 5;

        cairo_text_extents_t extents;
        std::string word;
        std::string line;

        for (size_t i = 0; i <= text.length(); i++) {
            if (i == text.length() || text[i] == ' ' || text[i] == '\n') {
                if (!word.empty()) {
                    std::string test_line = line.empty() ? word : line + " " + word;
                    cairo_text_extents(cr, test_line.c_str(), &extents);

                    if (extents.width > app_state.text_box_width - 10) {
                        if (!line.empty()) {
                            cairo_move_to(cr, x, y);
                            cairo_show_text(cr, line.c_str());
                            y += app_state.text_font_size + 2;
                            line = word;
                        } else {
                            cairo_move_to(cr, x, y);
                            cairo_show_text(cr, word.c_str());
                            y += app_state.text_font_size + 2;
                            line.clear();
                        }
                    } else {
                        line = test_line;
                    }
                    word.clear();
                }

                if (i < text.length() && text[i] == '\n') {
                    if (!line.empty()) {
                        cairo_move_to(cr, x, y);
                        cairo_show_text(cr, line.c_str());
                        y += app_state.text_font_size + 2;
                        line.clear();
                    }
                }

                if (y > app_state.text_y + app_state.text_box_height) break;
            } else {
                word += text[i];
            }
        }

        if (!line.empty() && y <= app_state.text_y + app_state.text_box_height) {
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, line.c_str());
        }
    }
}

// Draw selection overlay
void draw_selection_overlay(cairo_t* cr) {
    if (!app_state.has_selection) return;

    draw_ant_path(cr);

    if (app_state.selection_has_mask && !app_state.selection_mask.empty()) {
        int width = app_state.canvas_width;
        int height = app_state.canvas_height;

        auto selected_at = [&](int x, int y) -> bool {
            if (x < 0 || y < 0 || x >= width || y >= height) return false;
            return app_state.selection_mask[y * width + x];
        };

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (!selected_at(x, y)) continue;
                if (!selected_at(x, y - 1)) {
                    cairo_move_to(cr, x, y);
                    cairo_line_to(cr, x + 1, y);
                }
                if (!selected_at(x + 1, y)) {
                    cairo_move_to(cr, x + 1, y);
                    cairo_line_to(cr, x + 1, y + 1);
                }
                if (!selected_at(x, y + 1)) {
                    cairo_move_to(cr, x, y + 1);
                    cairo_line_to(cr, x + 1, y + 1);
                }
                if (!selected_at(x - 1, y)) {
                    cairo_move_to(cr, x, y);
                    cairo_line_to(cr, x, y + 1);
                }
            }
        }
        cairo_stroke(cr);
    } else if (app_state.selection_is_rect) {
        double x1 = fmin(app_state.selection_x1, app_state.selection_x2);
        double y1 = fmin(app_state.selection_y1, app_state.selection_y2);
        double x2 = fmax(app_state.selection_x1, app_state.selection_x2);
        double y2 = fmax(app_state.selection_y1, app_state.selection_y2);

        cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
        cairo_stroke(cr);
    } else if (app_state.selection_path.size() > 1) {
        cairo_move_to(cr, app_state.selection_path[0].first, app_state.selection_path[0].second);
        for (size_t i = 1; i < app_state.selection_path.size(); i++) {
            cairo_line_to(cr, app_state.selection_path[i].first, app_state.selection_path[i].second);
        }
        cairo_close_path(cr);
        cairo_stroke(cr);
    }
}

void draw_black_outline_circle(cairo_t* cr, double x, double y, double radius) {
    cairo_save(cr);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_arc(cr, x, y, radius, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void draw_hover_indicator(cairo_t* cr) {
    if (!app_state.hover_in_canvas) {
        return;
    }

    if (tool_shows_brush_hover_outline(app_state.current_tool) && !app_state.is_drawing) {
        double radius = app_state.line_width;
        if (app_state.current_tool == TOOL_ERASER) {
            radius = app_state.line_width * 1.5;
        } else if (app_state.current_tool == TOOL_AIRBRUSH) {
            radius = app_state.line_width * 5.0;
        }
        draw_black_outline_circle(cr, app_state.hover_x, app_state.hover_y, radius);
        return;
    }

    if (tool_shows_vertex_hover_markers(app_state.current_tool) && !app_state.is_drawing) {
        draw_black_outline_circle(cr, app_state.hover_x, app_state.hover_y, 5.0);
    }
}

// Draw preview overlays with ant paths
void draw_preview(cairo_t* cr) {
    if (!app_state.is_drawing) return;

    // Dragging an existing floating selection should only show the active
    // selection outline, not a new preview marquee from the selection tool.
    if (app_state.dragging_selection) return;

    cairo_save(cr);

    double preview_x = app_state.current_x;
    double preview_y = app_state.current_y;

    if (app_state.shift_pressed && !app_state.ellipse_center_mode) {
        if (app_state.current_tool == TOOL_LINE) {
            constrain_line(app_state.start_x, app_state.start_y, preview_x, preview_y);
        } else if (app_state.current_tool == TOOL_ELLIPSE) {
            constrain_to_circle(app_state.start_x, app_state.start_y, preview_x, preview_y);
        } else if (app_state.current_tool == TOOL_RECTANGLE ||
                   app_state.current_tool == TOOL_ROUNDED_RECT ||
                   app_state.current_tool == TOOL_RECT_SELECT ||
                   app_state.current_tool == TOOL_CROP) {
            constrain_to_square(app_state.start_x, app_state.start_y, preview_x, preview_y);
        }
    }

    switch (app_state.current_tool) {
        case TOOL_CURVE: {
            if (app_state.curve_active) {
                draw_ant_path(cr);

                draw_black_outline_circle(cr, app_state.curve_start_x, app_state.curve_start_y, 5.0);
                if (app_state.curve_has_end) {
                    draw_black_outline_circle(cr, app_state.curve_end_x, app_state.curve_end_y, 5.0);
                }

                if (app_state.curve_has_end) {
                    if (app_state.curve_has_control) {
                        cairo_move_to(cr, app_state.curve_start_x, app_state.curve_start_y);
                        cairo_curve_to(
                            cr,
                            app_state.curve_control_x,
                            app_state.curve_control_y,
                            app_state.curve_control_x,
                            app_state.curve_control_y,
                            app_state.curve_end_x,
                            app_state.curve_end_y
                        );
                    } else {
                        cairo_move_to(cr, app_state.curve_start_x, app_state.curve_start_y);
                        cairo_line_to(cr, app_state.curve_end_x, app_state.curve_end_y);
                    }
                    cairo_stroke(cr);
                }
            }
            break;
        }

        case TOOL_RECT_SELECT:
        case TOOL_CROP: {
            double x = fmin(app_state.start_x, preview_x);
            double y = fmin(app_state.start_y, preview_y);
            double w = fabs(preview_x - app_state.start_x);
            double h = fabs(preview_y - app_state.start_y);

            draw_ant_path(cr);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);
            break;
        }

        case TOOL_LASSO_SELECT: {
            if (app_state.lasso_points.size() > 1) {
                draw_ant_path(cr);
                cairo_move_to(cr, app_state.lasso_points[0].first, app_state.lasso_points[0].second);
                for (size_t i = 1; i < app_state.lasso_points.size(); i++) {
                    cairo_line_to(cr, app_state.lasso_points[i].first, app_state.lasso_points[i].second);
                }
                if (app_state.lasso_polygon_mode) {
                    cairo_line_to(cr, preview_x, preview_y);
                }
                cairo_stroke(cr);
            }
            if (app_state.lasso_polygon_mode) {
                for (const auto& point : app_state.lasso_points) {
                    draw_black_outline_circle(cr, point.first, point.second, 5.0);
                }
            }
            break;
        }

        case TOOL_LINE: {
            draw_ant_path(cr);
            cairo_move_to(cr, app_state.start_x, app_state.start_y);
            cairo_line_to(cr, preview_x, preview_y);
            cairo_stroke(cr);
            draw_black_outline_circle(cr, app_state.start_x, app_state.start_y, 5.0);
            draw_black_outline_circle(cr, preview_x, preview_y, 5.0);
            break;
        }

        case TOOL_RECTANGLE: {
            double x = fmin(app_state.start_x, preview_x);
            double y = fmin(app_state.start_y, preview_y);
            double w = fabs(preview_x - app_state.start_x);
            double h = fabs(preview_y - app_state.start_y);

            draw_ant_path(cr);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);
            break;
        }

        case TOOL_ELLIPSE: {
            double cx;
            double cy;
            double rx;
            double ry;

            if (app_state.ellipse_center_mode) {
                double radius = std::hypot(preview_x - app_state.start_x, preview_y - app_state.start_y);
                cx = app_state.start_x;
                cy = app_state.start_y;
                rx = radius;
                ry = radius;
            } else {
                cx = (app_state.start_x + preview_x) / 2.0;
                cy = (app_state.start_y + preview_y) / 2.0;
                rx = fabs(preview_x - app_state.start_x) / 2.0;
                ry = fabs(preview_y - app_state.start_y) / 2.0;
            }

            if (rx > 0.1 && ry > 0.1) {
                draw_ant_path(cr);
                cairo_save(cr);
                cairo_translate(cr, cx, cy);
                cairo_scale(cr, rx, ry);
                cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
                cairo_restore(cr);
                cairo_stroke(cr);
            }
            break;
        }

        case TOOL_REGULAR_POLYGON: {
            std::vector<std::pair<double, double>> points;
            build_regular_polygon_points(
                app_state.start_x,
                app_state.start_y,
                preview_x,
                preview_y,
                app_state.ctrl_pressed,
                app_state.shift_pressed,
                app_state.regular_polygon_sides,
                points
            );

            if (points.size() >= 3) {
                draw_ant_path(cr);
                cairo_move_to(cr, points[0].first, points[0].second);
                for (size_t i = 1; i < points.size(); ++i) {
                    cairo_line_to(cr, points[i].first, points[i].second);
                }
                cairo_close_path(cr);
                cairo_stroke(cr);
            }
            break;
        }

        case TOOL_STAR: {
            std::vector<std::pair<double, double>> points;
            build_star_points(
                app_state.start_x,
                app_state.start_y,
                preview_x,
                preview_y,
                app_state.ctrl_pressed,
                app_state.shift_pressed,
                app_state.star_points,
                points
            );

            if (points.size() >= 6) {
                draw_ant_path(cr);
                cairo_move_to(cr, points[0].first, points[0].second);
                for (size_t i = 1; i < points.size(); ++i) {
                    cairo_line_to(cr, points[i].first, points[i].second);
                }
                cairo_close_path(cr);
                cairo_stroke(cr);
            }
            break;
        }

        case TOOL_ROUNDED_RECT: {
            double x = fmin(app_state.start_x, preview_x);
            double y = fmin(app_state.start_y, preview_y);
            double w = fabs(preview_x - app_state.start_x);
            double h = fabs(preview_y - app_state.start_y);
            double r = fmin(w, h) * 0.1;

            if (w > 1 && h > 1) {
                draw_ant_path(cr);
                cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
                cairo_arc(cr, x + w - r, y + r, r, 3 * M_PI / 2, 0);
                cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
                cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
                cairo_close_path(cr);
                cairo_stroke(cr);
            }
            break;
        }

        case TOOL_GRADIENT_FILL: {
            draw_ant_path(cr);
            draw_black_outline_circle(cr, app_state.start_x, app_state.start_y, 5.0);
            draw_black_outline_circle(cr, preview_x, preview_y, 5.0);

            if (app_state.gradient_fill_circular) {
                double radius = std::hypot(preview_x - app_state.start_x, preview_y - app_state.start_y);
                cairo_arc(cr, app_state.start_x, app_state.start_y, radius, 0, 2 * M_PI);
            } else {
                cairo_move_to(cr, app_state.start_x, app_state.start_y);
                cairo_line_to(cr, preview_x, preview_y);
            }
            cairo_stroke(cr);
            break;
        }

        case TOOL_POLYGON: {
            if (app_state.polygon_points.size() > 0) {
                draw_ant_path(cr);
                cairo_move_to(cr, app_state.polygon_points[0].first, app_state.polygon_points[0].second);
                for (size_t i = 1; i < app_state.polygon_points.size(); i++) {
                    cairo_line_to(cr, app_state.polygon_points[i].first, app_state.polygon_points[i].second);
                }

                if (app_state.polygon_finished) {
                    cairo_close_path(cr);
                } else {
                    cairo_line_to(cr, preview_x, preview_y);
                }

                cairo_stroke(cr);

                for (const auto& point : app_state.polygon_points) {
                    draw_black_outline_circle(cr, point.first, point.second, 5.0);
                }
            }
            break;
        }
    }

    cairo_restore(cr);
}

bool is_close_to_guide_blue(guint8 r, guint8 g, guint8 b) {
    const int target_r = 0;
    const int target_g = 0;
    const int target_b = 255;
    return std::abs((int)r - target_r) <= 10 && std::abs((int)g - target_g) <= 10 && std::abs((int)b - target_b) <= 10;
}

void get_visible_composited_rgb(int x, int y, guint8& out_r, guint8& out_g, guint8& out_b) {
    double r = 1.0, g = 1.0, b = 1.0, a = 1.0;
    for (int i = 0; i < (int)app_state.layers.size(); ++i) {
        const Layer& layer = app_state.layers[i];
        if (!layer.visible || !layer.surface) continue;
        int lw = cairo_image_surface_get_width(layer.surface);
        int lh = cairo_image_surface_get_height(layer.surface);
        if (x < 0 || y < 0 || x >= lw || y >= lh) continue;
        cairo_surface_flush(layer.surface);
        unsigned char* data = cairo_image_surface_get_data(layer.surface);
        int stride = cairo_image_surface_get_stride(layer.surface);
        guint32 pixel = *reinterpret_cast<guint32*>(data + y * stride + x * 4);
        double sa = (((pixel >> 24) & 0xFF) / 255.0) * layer.opacity;
        double sr = ((pixel >> 16) & 0xFF) / 255.0;
        double sg = ((pixel >> 8) & 0xFF) / 255.0;
        double sb = (pixel & 0xFF) / 255.0;
        r = sr * sa + r * (1.0 - sa);
        g = sg * sa + g * (1.0 - sa);
        b = sb * sa + b * (1.0 - sa);
    }
    out_r = (guint8)std::round(std::max(0.0, std::min(1.0, r)) * 255.0);
    out_g = (guint8)std::round(std::max(0.0, std::min(1.0, g)) * 255.0);
    out_b = (guint8)std::round(std::max(0.0, std::min(1.0, b)) * 255.0);
}

void draw_vertical_guide(cairo_t* cr, double x) {
    int xi = (int)std::round(x);
    if (xi < 0 || xi >= app_state.canvas_width) return;
    for (int y = 0; y < app_state.canvas_height; ++y) {
        guint8 r, g, b;
        get_visible_composited_rgb(xi, y, r, g, b);
        if (is_close_to_guide_blue(r, g, b)) {
            cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
        } else {
            cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
        }
        cairo_rectangle(cr, xi, y, 1, 1);
        cairo_fill(cr);
    }
}

void draw_horizontal_guide(cairo_t* cr, double y) {
    int yi = (int)std::round(y);
    if (yi < 0 || yi >= app_state.canvas_height) return;
    for (int x = 0; x < app_state.canvas_width; ++x) {
        guint8 r, g, b;
        get_visible_composited_rgb(x, yi, r, g, b);
        if (is_close_to_guide_blue(r, g, b)) {
            cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
        } else {
            cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
        }
        cairo_rectangle(cr, x, yi, 1, 1);
        cairo_fill(cr);
    }
}

void draw_canvas_grid_background(cairo_t* cr, double width, double height) {
    static cairo_pattern_t* checker_pattern = nullptr;

    if (!checker_pattern) {
        const int checker_size = 1;
        const int pattern_size = checker_size * 2;

        cairo_surface_t* pattern_surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32,
            pattern_size,
            pattern_size
        );
        cairo_t* pattern_cr = cairo_create(pattern_surface);
        configure_crisp_rendering(pattern_cr);

        cairo_set_source_rgb(pattern_cr, 1.0, 1.0, 1.0);
        cairo_paint(pattern_cr);

        cairo_set_source_rgb(pattern_cr, 0.0, 0.0, 0.0);
        cairo_rectangle(pattern_cr, 0, 0, checker_size, checker_size);
        cairo_rectangle(pattern_cr, checker_size, checker_size, checker_size, checker_size);
        cairo_fill(pattern_cr);

        cairo_destroy(pattern_cr);

        checker_pattern = cairo_pattern_create_for_surface(pattern_surface);
        cairo_pattern_set_extend(checker_pattern, CAIRO_EXTEND_REPEAT);
        cairo_pattern_set_filter(checker_pattern, CAIRO_FILTER_NEAREST);
        cairo_surface_destroy(pattern_surface);
    }

    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_set_source(cr, checker_pattern);
    cairo_fill(cr);
    cairo_restore(cr);
}

// Canvas draw callback
gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    if (app_state.layers.empty()) {
        return FALSE;
    }

    configure_crisp_rendering(cr);
    draw_canvas_grid_background(
        cr,
        app_state.canvas_width * app_state.zoom_factor,
        app_state.canvas_height * app_state.zoom_factor
    );
    cairo_save(cr);
    cairo_scale(cr, app_state.zoom_factor, app_state.zoom_factor);

    for (const Layer& layer : app_state.layers) {
        if (!layer.visible || !layer.surface) continue;
        cairo_set_source_surface(cr, layer.surface, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_paint_with_alpha(cr, layer.opacity);
    }

    if (app_state.floating_selection_active && app_state.floating_surface) {
        double x = std::round(fmin(app_state.selection_x1, app_state.selection_x2));
        double y = std::round(fmin(app_state.selection_y1, app_state.selection_y2));
        cairo_set_source_surface(cr, app_state.floating_surface, x, y);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_paint(cr);
    }

    if (app_state.show_vertical_center_guide) {
        draw_vertical_guide(cr, app_state.canvas_width / 2.0);
    }
    if (app_state.show_horizontal_center_guide) {
        draw_horizontal_guide(cr, app_state.canvas_height / 2.0);
    }
    for (double x : app_state.vertical_guides) draw_vertical_guide(cr, x);
    for (double y : app_state.horizontal_guides) draw_horizontal_guide(cr, y);

    draw_line_pattern_preview_overlay(cr);

    if (app_state.has_selection) {
       draw_selection_overlay(cr);
    }
    if (app_state.text_active) {
        draw_text_overlay(cr);
    }
    if (tool_needs_preview(app_state.current_tool)) {
        draw_preview(cr);
    }
    draw_hover_indicator(cr);
    cairo_restore(cr);
    return FALSE;
}
