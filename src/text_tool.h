#pragma once

#include "app_state.h"

// Text box hit testing
bool point_in_text_box(double x, double y);

// Text box sizing
void update_text_box_size();

// Text lifecycle
void cancel_text();
void finalize_text();

// Text input window
void create_text_window(double x, double y);
