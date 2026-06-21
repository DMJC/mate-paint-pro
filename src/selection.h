#pragma once

#include "app_state.h"

// Selection query
bool point_in_selection(double x, double y);

// Selection management
void clear_selection();
void commit_floating_selection(bool record_undo = true);

// Selection path/lasso
void append_selection_path(cairo_t* cr);
void finalize_lasso_selection();

// Selection pixel operations
SelectionPixelBounds get_selection_pixel_bounds();
bool selection_mask_selected_at(int x, int y);
void copy_selection_masked_pixels(cairo_surface_t* destination, int dest_x, int dest_y,
                                  cairo_surface_t* source, const SelectionPixelBounds& bounds);
void clear_selection_masked_pixels(cairo_surface_t* destination, const SelectionPixelBounds& bounds, guint32 fill_pixel);

// Selection drag/clipboard
void start_selection_drag();
void copy_selection();
void cut_selection();
void paste_selection();

// System clipboard
void copy_surface_to_system_clipboard(cairo_surface_t* surface);
cairo_surface_t* get_surface_from_system_clipboard(int& width, int& height);

// Canvas resize for paste
bool should_expand_canvas_for_paste(int pasted_width, int pasted_height);
void resize_canvas_for_paste(int new_width, int new_height);

// Ant path (marching ants) animation
gboolean ant_path_timer(gpointer data);
void start_ant_animation();
void stop_ant_animation();
void draw_ant_path(cairo_t* cr);
