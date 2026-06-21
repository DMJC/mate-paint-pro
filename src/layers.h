#pragma once

#include "app_state.h"

#include <cairo.h>

// Layer control synchronization
void sync_layer_controls();

// Layer selection
void set_active_layer(int index);

// Layer lifecycle
void clear_layers();
void ensure_default_layers();
void add_new_layer();
void duplicate_active_layer();

// Layer ordering
void move_layer_up(int index);
void move_layer_down(int index);

// Layer operations
void merge_layer_down(int index);
void delete_layer(int index);

// Layer panel UI
void rebuild_layer_panel();

// Compositing
cairo_surface_t* compose_visible_layers_surface();
