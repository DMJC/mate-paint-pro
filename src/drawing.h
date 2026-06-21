#pragma once

#include "app_state.h"

#include <cairo.h>
#include <vector>
#include <utility>

// Pixel reading helper
guint32 read_pixel(int x, int y);

// Color picking
void pick_color_at(int x, int y, bool set_background);

// Fill operations
void flood_fill_at(int start_x, int start_y);
void gradient_fill_at(int start_x, int start_y, int end_x, int end_y, bool circular_gradient);

// Color selection
bool pixel_matches_with_tolerance(guint32 pixel, guint32 target, int tolerance);
bool select_pixels_by_color(int start_x, int start_y, bool contiguous_only, int tolerance);

// Shape drawing
void draw_line(cairo_t* cr, double x1, double y1, double x2, double y2);
void draw_rectangle(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled);
void draw_ellipse(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled);
void draw_rounded_rectangle(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled);
void draw_polygon(cairo_t* cr, const std::vector<std::pair<double, double>>& points);
void draw_regular_polygon(cairo_t* cr, const std::vector<std::pair<double, double>>& points);
void draw_curve(cairo_t* cr, double start_x, double start_y, double control_x, double control_y, double end_x, double end_y);

// Freehand drawing tools
void draw_pencil(cairo_t* cr, double x, double y);
void draw_paintbrush(cairo_t* cr, double x, double y);
void draw_airbrush(cairo_t* cr, double x, double y);
void draw_eraser(cairo_t* cr, double x, double y);
void draw_smudge(double x, double y);
