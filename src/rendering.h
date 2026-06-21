#pragma once

#include "app_state.h"

#include <cairo.h>
#include <gtk/gtk.h>

// Text overlay
void draw_text_overlay(cairo_t* cr);

// Selection overlay
void draw_selection_overlay(cairo_t* cr);

// Circle outline helper
void draw_black_outline_circle(cairo_t* cr, double x, double y, double radius);

// Hover indicator for brush/vertex tools
void draw_hover_indicator(cairo_t* cr);

// Shape/tool preview overlays (ant paths)
void draw_preview(cairo_t* cr);

// Guide color detection
bool is_close_to_guide_blue(guint8 r, guint8 g, guint8 b);

// Composited pixel color query
void get_visible_composited_rgb(int x, int y, guint8& out_r, guint8& out_g, guint8& out_b);

// Guide drawing
void draw_vertical_guide(cairo_t* cr, double x);
void draw_horizontal_guide(cairo_t* cr, double y);

// Canvas grid (checkerboard) background
void draw_canvas_grid_background(cairo_t* cr, double width, double height);

// Line pattern preview overlay (defined in filters.cpp)
void draw_line_pattern_preview_overlay(cairo_t* cr);

// Main canvas draw callback
gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer data);
