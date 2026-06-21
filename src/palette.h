#pragma once

#include "app_state.h"

#include <string>

// Config file path
std::string get_config_file_path();

// Custom palette index helpers
int get_custom_palette_start_index();

// Custom palette persistence
void load_custom_palette_colors();
void save_custom_palette_colors();

// Color indicator drawing and updates
gboolean on_color_button_draw(GtkWidget* widget, cairo_t* cr, gpointer data);
void update_color_indicators();

// Palette button styling and interaction
void apply_color_button_style(GtkWidget* button, const GdkRGBA& color, bool is_custom_slot);
void show_custom_color_dialog(int index);
gboolean on_color_button_press(GtkWidget* widget, GdkEventButton* event, gpointer data);
GtkWidget* create_color_button(GdkRGBA color, int index, bool is_custom_slot);
