#pragma once

#include "app_state.h"

// Undo/Redo operations
void push_undo_state();
void undo_last_operation();
void redo_last_operation();
