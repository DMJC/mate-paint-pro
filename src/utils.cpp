#include "utils.h"
#include "layers.h"

#include <cmath>
#include <algorithm>

// Coordinate and math helpers

double to_canvas_coordinate(double coordinate) {
    return coordinate / app_state.zoom_factor;
}

double clamp_double(double value, double min_value, double max_value) {
    return fmax(min_value, fmin(value, max_value));
}

double clamp_color_channel(double channel) {
    return fmax(0.0, fmin(1.0, channel));
}

// Canvas status display

void update_canvas_dimensions_label() {
    if (!app_state.canvas_dimensions_label) {
        return;
    }

    gchar* dimensions_text = g_strdup_printf("%dx%d", app_state.canvas_width, app_state.canvas_height);
    gtk_label_set_text(GTK_LABEL(app_state.canvas_dimensions_label), dimensions_text);
    g_free(dimensions_text);
}

void update_cursor_position_label(double canvas_x, double canvas_y, bool cursor_in_canvas) {
    if (!app_state.cursor_position_label) {
        return;
    }

    if (!cursor_in_canvas) {
        gtk_label_set_text(GTK_LABEL(app_state.cursor_position_label), "-");
        return;
    }

    int x = static_cast<int>(std::lround(clamp_double(canvas_x, 0.0, app_state.canvas_width * 1.0)));
    int y = static_cast<int>(std::lround(clamp_double(canvas_y, 0.0, app_state.canvas_height * 1.0)));

    gchar* position_text = g_strdup_printf("%dx%d", x, y);
    gtk_label_set_text(GTK_LABEL(app_state.cursor_position_label), position_text);
    g_free(position_text);
}

// Rendering helpers

void configure_crisp_rendering(cairo_t* cr) {
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
}

// Zoom

void apply_zoom(double zoom_factor, double focus_x, double focus_y) {
    if (!app_state.drawing_area) {
        return;
    }

    app_state.zoom_factor = zoom_factor;
    gtk_widget_set_size_request(
        app_state.drawing_area,
        static_cast<int>(app_state.canvas_width * app_state.zoom_factor),
        static_cast<int>(app_state.canvas_height * app_state.zoom_factor)
    );

    if (app_state.scrolled_window) {
        GtkAdjustment* hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app_state.scrolled_window));
        GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app_state.scrolled_window));

        double h_page = gtk_adjustment_get_page_size(hadj);
        double v_page = gtk_adjustment_get_page_size(vadj);

        double target_h = focus_x * app_state.zoom_factor - h_page / 2.0;
        double target_v = focus_y * app_state.zoom_factor - v_page / 2.0;

        double h_max = fmax(gtk_adjustment_get_lower(hadj),
            gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj));
        double v_max = fmax(gtk_adjustment_get_lower(vadj),
            gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));

        target_h = clamp_double(target_h, gtk_adjustment_get_lower(hadj), h_max);
        target_v = clamp_double(target_v, gtk_adjustment_get_lower(vadj), v_max);

        gtk_adjustment_set_value(hadj, target_h);
        gtk_adjustment_set_value(vadj, target_v);
    }

    gtk_widget_queue_draw(app_state.drawing_area);
}

void reset_zoom_to_default() {
    if (!app_state.drawing_area) {
        return;
    }

    apply_zoom(1.0, app_state.canvas_width / 2.0, app_state.canvas_height / 2.0);

    if (app_state.scrolled_window) {
        GtkAdjustment* hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app_state.scrolled_window));
        GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app_state.scrolled_window));
        gtk_adjustment_set_value(hadj, gtk_adjustment_get_lower(hadj));
        gtk_adjustment_set_value(vadj, gtk_adjustment_get_lower(vadj));
    }
}

// Canvas queries

bool point_in_canvas(int x, int y) {
    return x >= 0 && x < app_state.canvas_width && y >= 0 && y < app_state.canvas_height;
}

// Surface management

void init_surface(GtkWidget* widget) {
    ensure_default_layers();
}

cairo_surface_t* create_blank_surface(int width, int height, bool fill_white) {
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(surface);
    configure_crisp_rendering(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    if (fill_white) {
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
    } else {
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
    }
    cairo_paint(cr);
    cairo_destroy(cr);
    return surface;
}

cairo_surface_t* clone_surface(cairo_surface_t* source, int width, int height) {
    if (!source || width <= 0 || height <= 0) {
        return nullptr;
    }

    cairo_surface_t* copy = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(copy);
    cairo_set_source_surface(cr, source, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    return copy;
}

// Color utilities

GdkRGBA get_active_color() {
    return app_state.is_right_button ? app_state.bg_color : app_state.fg_color;
}

bool is_transparent_color(const GdkRGBA& color) {
    return color.alpha <= 0.001;
}

GdkRGBA pixel_to_rgba(guint32 pixel) {
    GdkRGBA color;
    color.alpha = ((pixel >> 24) & 0xFF) / 255.0;
    color.red = ((pixel >> 16) & 0xFF) / 255.0;
    color.green = ((pixel >> 8) & 0xFF) / 255.0;
    color.blue = (pixel & 0xFF) / 255.0;
    return color;
}

guint32 rgba_to_pixel(const GdkRGBA& color) {
    guint8 r = static_cast<guint8>(std::round(clamp_color_channel(color.red) * 255.0));
    guint8 g = static_cast<guint8>(std::round(clamp_color_channel(color.green) * 255.0));
    guint8 b = static_cast<guint8>(std::round(clamp_color_channel(color.blue) * 255.0));
    guint8 a = static_cast<guint8>(std::round(clamp_color_channel(color.alpha) * 255.0));
    return (static_cast<guint32>(a) << 24) |
           (static_cast<guint32>(r) << 16) |
           (static_cast<guint32>(g) << 8) |
            static_cast<guint32>(b);
}

// Tool query functions

bool tool_needs_preview(Tool tool) {
    return tool == TOOL_LASSO_SELECT || tool == TOOL_RECT_SELECT ||
           tool == TOOL_LINE || tool == TOOL_CURVE ||
           tool == TOOL_RECTANGLE || tool == TOOL_POLYGON || tool == TOOL_CROP ||
           tool == TOOL_ELLIPSE || tool == TOOL_REGULAR_POLYGON || tool == TOOL_STAR ||
           tool == TOOL_ROUNDED_RECT || tool == TOOL_GRADIENT_FILL;
}

bool tool_is_selection_tool(Tool tool) {
    return tool == TOOL_LASSO_SELECT || tool == TOOL_RECT_SELECT ||
           tool == TOOL_SELECT_BY_COLOR || tool == TOOL_FUZZY_SELECT ||
           tool == TOOL_CROP;
}

int tool_to_index(Tool tool) {
    return static_cast<int>(tool);
}

bool tool_supports_line_thickness(Tool tool) {
    return tool == TOOL_PAINTBRUSH || tool == TOOL_AIRBRUSH || tool == TOOL_ERASER || tool == TOOL_SMUDGE ||
           tool == TOOL_LINE || tool == TOOL_CURVE || tool == TOOL_RECTANGLE ||
           tool == TOOL_POLYGON || tool == TOOL_ELLIPSE || tool == TOOL_REGULAR_POLYGON || tool == TOOL_STAR || tool == TOOL_ROUNDED_RECT;
}

bool tool_shows_brush_hover_outline(Tool tool) {
    return tool == TOOL_PAINTBRUSH || tool == TOOL_AIRBRUSH || tool == TOOL_ERASER || tool == TOOL_SMUDGE || tool == TOOL_ELLIPSE ||  tool == TOOL_LASSO_SELECT;
}

bool tool_shows_vertex_hover_markers(Tool tool) {
    return tool == TOOL_LINE || tool == TOOL_CURVE || tool == TOOL_POLYGON;
}

// Shape geometry helpers

void constrain_line(double start_x, double start_y, double& end_x, double& end_y) {
    double dx = end_x - start_x;
    double dy = end_y - start_y;

    if (fabs(dx) > fabs(dy)) {
        end_y = start_y;
    } else {
        end_x = start_x;
    }
}

void constrain_to_circle(double start_x, double start_y, double& end_x, double& end_y) {
    double dx = end_x - start_x;
    double dy = end_y - start_y;
    double radius = fmax(fabs(dx), fabs(dy));

    end_x = start_x + (dx >= 0 ? radius : -radius);
    end_y = start_y + (dy >= 0 ? radius : -radius);
}

void constrain_to_square(double start_x, double start_y, double& end_x, double& end_y) {
    double dx = end_x - start_x;
    double dy = end_y - start_y;
    double size = fmax(fabs(dx), fabs(dy));

    end_x = start_x + (dx >= 0 ? size : -size);
    end_y = start_y + (dy >= 0 ? size : -size);
}

void build_regular_polygon_points(double start_x, double start_y, double end_x, double end_y, bool from_center, bool uniform, int sides, std::vector<std::pair<double, double>>& points) {
    points.clear();

    if (sides < 3) {
        return;
    }

    double center_x = 0.0;
    double center_y = 0.0;
    double radius_x = 0.0;
    double radius_y = 0.0;

    if (from_center) {
        center_x = start_x;
        center_y = start_y;
        radius_x = fabs(end_x - start_x);
        radius_y = fabs(end_y - start_y);
    } else {
        center_x = (start_x + end_x) / 2.0;
        center_y = (start_y + end_y) / 2.0;
        radius_x = fabs(end_x - start_x) / 2.0;
        radius_y = fabs(end_y - start_y) / 2.0;
    }

    if (uniform) {
        double radius = fmax(radius_x, radius_y);
        radius_x = radius;
        radius_y = radius;
    }

    if (radius_x < 0.1 || radius_y < 0.1) {
        return;
    }

    const double start_angle = -M_PI / 2.0;
    const double step = (2.0 * M_PI) / static_cast<double>(sides);

    for (int i = 0; i < sides; ++i) {
        double angle = start_angle + step * static_cast<double>(i);
        points.push_back({
            center_x + cos(angle) * radius_x,
            center_y + sin(angle) * radius_y
        });
    }
}

void build_star_points(double start_x, double start_y, double end_x, double end_y, bool from_center, bool uniform, int points_count, std::vector<std::pair<double, double>>& points) {
    points.clear();

    if (points_count < 3) {
        return;
    }

    double center_x = 0.0;
    double center_y = 0.0;
    double radius_x = 0.0;
    double radius_y = 0.0;

    if (from_center) {
        center_x = start_x;
        center_y = start_y;
        radius_x = fabs(end_x - start_x);
        radius_y = fabs(end_y - start_y);
    } else {
        center_x = (start_x + end_x) / 2.0;
        center_y = (start_y + end_y) / 2.0;
        radius_x = fabs(end_x - start_x) / 2.0;
        radius_y = fabs(end_y - start_y) / 2.0;
    }

    if (uniform) {
        double radius = fmax(radius_x, radius_y);
        radius_x = radius;
        radius_y = radius;
    }

    if (radius_x < 0.1 || radius_y < 0.1) {
        return;
    }

    const double inner_scale = 0.5;
    const double start_angle = -M_PI / 2.0;
    const int vertex_count = points_count * 2;
    const double step = (2.0 * M_PI) / static_cast<double>(vertex_count);

    for (int i = 0; i < vertex_count; ++i) {
        const bool is_outer = (i % 2 == 0);
        const double scale = is_outer ? 1.0 : inner_scale;
        double angle = start_angle + step * static_cast<double>(i);
        points.push_back({
            center_x + cos(angle) * radius_x * scale,
            center_y + sin(angle) * radius_y * scale
        });
    }
}
