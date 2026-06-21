#pragma once

#include "app_state.h"

// Edit menu callbacks
void on_edit_copy(GtkMenuItem* item, gpointer data);
void on_edit_cut(GtkMenuItem* item, gpointer data);
void on_edit_paste(GtkMenuItem* item, gpointer data);
void on_edit_undo(GtkMenuItem* item, gpointer data);
void on_edit_redo(GtkMenuItem* item, gpointer data);

// Layer menu callbacks
void on_layer_add(GtkMenuItem* item, gpointer data);
void on_layer_delete(GtkMenuItem* item, gpointer data);

// View menu callbacks
void on_view_vertical_center_toggled(GtkCheckMenuItem* item, gpointer data);
void on_view_horizontal_center_toggled(GtkCheckMenuItem* item, gpointer data);
bool prompt_guides(bool vertical);
void on_view_vertical_guides_toggled(GtkCheckMenuItem* item, gpointer data);
void on_view_horizontal_guides_toggled(GtkCheckMenuItem* item, gpointer data);

// Help menu callbacks
void on_help_manual(GtkMenuItem* item, gpointer data);
void on_help_about(GtkMenuItem* item, gpointer data);

// Menu bar construction
GtkWidget* build_menubar();
