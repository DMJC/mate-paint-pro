#pragma once

#include "app_state.h"

// Dialog helpers
bool ask_regular_polygon_sides(GtkWidget* parent);
bool ask_star_points(GtkWidget* parent);

// Tool button callback
void on_tool_clicked(GtkButton* button, gpointer data);

// Line thickness widgets
gboolean on_line_thickness_button_draw(GtkWidget* widget, cairo_t* cr, gpointer data);
void update_line_thickness_buttons();
void on_line_thickness_toggled(GtkToggleButton* button, gpointer data);
GtkWidget* create_line_thickness_button(int index);
void update_line_thickness_visibility();

// Zoom widgets
void update_zoom_buttons();
void on_zoom_toggled(GtkToggleButton* button, gpointer data);
GtkWidget* create_zoom_button(int index);
void update_zoom_visibility();

// Tool icon and button creation
const char* get_tool_icon_filename(Tool tool);
GtkWidget* create_tool_icon(Tool tool);
GtkWidget* create_tool_button(Tool tool, const char* tooltip);
