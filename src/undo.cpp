#include "undo.h"
#include "utils.h"
#include "layers.h"
#include "selection.h"
#include "text_tool.h"

void push_undo_state() {
    if (!app_state.surface) {
        return;
    }

    cairo_surface_t* snapshot = clone_surface(app_state.surface, app_state.canvas_width, app_state.canvas_height);
    if (!snapshot) {
        return;
    }

    UndoSnapshot undo_snapshot;
    undo_snapshot.surface = snapshot;
    undo_snapshot.width = app_state.canvas_width;
    undo_snapshot.height = app_state.canvas_height;
    app_state.undo_stack.push_back(undo_snapshot);
    if (app_state.undo_stack.size() > AppState::max_undo_steps) {
        cairo_surface_destroy(app_state.undo_stack.front().surface);
        app_state.undo_stack.erase(app_state.undo_stack.begin());
    }
    for (UndoSnapshot& redo_snapshot : app_state.redo_stack) {
        cairo_surface_destroy(redo_snapshot.surface);
    }
    app_state.redo_stack.clear();
}

void undo_last_operation() {
    if (app_state.undo_stack.empty()) {
        return;
    }

    cairo_surface_t* redo_surface = clone_surface(app_state.surface, app_state.canvas_width, app_state.canvas_height);
    if (redo_surface) {
        UndoSnapshot redo_snapshot;
        redo_snapshot.surface = redo_surface;
        redo_snapshot.width = app_state.canvas_width;
        redo_snapshot.height = app_state.canvas_height;
        app_state.redo_stack.push_back(redo_snapshot);
        if (app_state.redo_stack.size() > AppState::max_undo_steps) {
            cairo_surface_destroy(app_state.redo_stack.front().surface);
            app_state.redo_stack.erase(app_state.redo_stack.begin());
        }
    }

    UndoSnapshot snapshot = app_state.undo_stack.back();
    app_state.undo_stack.pop_back();

    if (app_state.surface) {
        cairo_surface_destroy(app_state.surface);
    }

    app_state.surface = snapshot.surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    app_state.canvas_width = snapshot.width;
    app_state.canvas_height = snapshot.height;

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }
    app_state.drag_undo_snapshot_taken = false;

    if (app_state.drawing_area) {
        gtk_widget_set_size_request(
            app_state.drawing_area,
            static_cast<int>(app_state.canvas_width * app_state.zoom_factor),
            static_cast<int>(app_state.canvas_height * app_state.zoom_factor)
        );
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void redo_last_operation() {
    if (app_state.redo_stack.empty()) {
        return;
    }

    cairo_surface_t* undo_surface = clone_surface(app_state.surface, app_state.canvas_width, app_state.canvas_height);
    if (undo_surface) {
        UndoSnapshot undo_snapshot;
        undo_snapshot.surface = undo_surface;
        undo_snapshot.width = app_state.canvas_width;
        undo_snapshot.height = app_state.canvas_height;
        app_state.undo_stack.push_back(undo_snapshot);
        if (app_state.undo_stack.size() > AppState::max_undo_steps) {
            cairo_surface_destroy(app_state.undo_stack.front().surface);
            app_state.undo_stack.erase(app_state.undo_stack.begin());
        }
    }

    UndoSnapshot snapshot = app_state.redo_stack.back();
    app_state.redo_stack.pop_back();

    if (app_state.surface) {
        cairo_surface_destroy(app_state.surface);
    }

    app_state.surface = snapshot.surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    app_state.canvas_width = snapshot.width;
    app_state.canvas_height = snapshot.height;

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }
    app_state.drag_undo_snapshot_taken = false;

    if (app_state.drawing_area) {
        gtk_widget_set_size_request(
            app_state.drawing_area,
            static_cast<int>(app_state.canvas_width * app_state.zoom_factor),
            static_cast<int>(app_state.canvas_height * app_state.zoom_factor)
        );
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}
