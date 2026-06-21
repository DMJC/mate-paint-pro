#pragma once

#include "app_state.h"

#include <cairo.h>
#include <string>

// File save/load dialogs
void save_image_dialog(GtkWidget* parent);
bool save_surface_to_file(cairo_surface_t* surface, const std::string& filename);
void open_image_dialog(GtkWidget* parent);

// File menu callbacks
void on_file_new(GtkMenuItem* item, gpointer data);
void on_file_open(GtkMenuItem* item, gpointer data);
void on_file_save(GtkMenuItem* item, gpointer data);
void on_file_save_as(GtkMenuItem* item, gpointer data);
void on_file_quit(GtkMenuItem* item, gpointer data);
