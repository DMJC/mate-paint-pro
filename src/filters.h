#pragma once

#include "app_state.h"

// Pixel sampling and composition
guint32 sample_surface_pixel(cairo_surface_t* surface, int x, int y);
guint32 compose_premultiplied_pixel(double red, double green, double blue, double alpha);

// Convolution and image filters
void apply_convolution_to_active_layer(const ConvolutionKernel& kernel);
void apply_cartoonify_to_active_layer();

// Filter menu callbacks
void on_filters_noise(GtkMenuItem* item, gpointer data);
void on_filters_gaussian_blur(GtkMenuItem* item, gpointer data);
void on_filters_blur(GtkMenuItem* item, gpointer data);
void on_filters_sharpen(GtkMenuItem* item, gpointer data);
void on_filters_cartoonify(GtkMenuItem* item, gpointer data);

// Color and brightness adjustments
void apply_color_balance_from_original(ColorBalanceWindowState* state);
void apply_brightness_contrast_from_original(BrightnessContrastWindowState* state);
void on_layer_color_balance(GtkMenuItem* item, gpointer data);
void on_layer_brightness_contrast(GtkMenuItem* item, gpointer data);

// Line pattern drawing
void on_layer_draw_horizontal_lines(GtkMenuItem* item, gpointer data);
void on_layer_draw_vertical_lines(GtkMenuItem* item, gpointer data);
void on_layer_draw_grid(GtkMenuItem* item, gpointer data);
void draw_line_pattern_preview_overlay(cairo_t* cr);
