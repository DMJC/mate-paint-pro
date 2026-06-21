#pragma once

#include "app_state.h"

void on_image_scale(GtkMenuItem* item, gpointer data);
void on_image_resize_canvas(GtkMenuItem* item, gpointer data);
void on_image_rotate_clockwise(GtkMenuItem* item, gpointer data);
void on_image_rotate_counter_clockwise(GtkMenuItem* item, gpointer data);
void on_image_flip_horizontal(GtkMenuItem* item, gpointer data);
void on_image_flip_vertical(GtkMenuItem* item, gpointer data);
void crop_to_rectangle(double x1, double y1, double x2, double y2);
