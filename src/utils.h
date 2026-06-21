#pragma once

#include "app_state.h"

#include <cairo.h>
#include <gdk/gdk.h>
#include <vector>
#include <utility>

// Coordinate and math helpers
double to_canvas_coordinate(double coordinate);
double clamp_double(double value, double min_value, double max_value);
double clamp_color_channel(double channel);

// Canvas status display
void update_canvas_dimensions_label();
void update_cursor_position_label(double canvas_x, double canvas_y, bool cursor_in_canvas);

// Rendering helpers
void configure_crisp_rendering(cairo_t* cr);

// Zoom
void apply_zoom(double zoom_factor, double focus_x, double focus_y);
void reset_zoom_to_default();

// Canvas queries
bool point_in_canvas(int x, int y);

// Surface management
void init_surface(GtkWidget* widget);
cairo_surface_t* create_blank_surface(int width, int height, bool fill_white);
cairo_surface_t* clone_surface(cairo_surface_t* source, int width, int height);

// Color utilities
GdkRGBA get_active_color();
bool is_transparent_color(const GdkRGBA& color);
GdkRGBA pixel_to_rgba(guint32 pixel);
guint32 rgba_to_pixel(const GdkRGBA& color);

// Tool query functions
bool tool_needs_preview(Tool tool);
bool tool_is_selection_tool(Tool tool);
int tool_to_index(Tool tool);
bool tool_supports_line_thickness(Tool tool);
bool tool_shows_brush_hover_outline(Tool tool);
bool tool_shows_vertex_hover_markers(Tool tool);

// Shape geometry helpers
void constrain_line(double start_x, double start_y, double& end_x, double& end_y);
void constrain_to_circle(double start_x, double start_y, double& end_x, double& end_y);
void constrain_to_square(double start_x, double start_y, double& end_x, double& end_y);
void build_regular_polygon_points(double start_x, double start_y, double end_x, double end_y, bool from_center, bool uniform, int sides, std::vector<std::pair<double, double>>& points);
void build_star_points(double start_x, double start_y, double end_x, double end_y, bool from_center, bool uniform, int points_count, std::vector<std::pair<double, double>>& points);
