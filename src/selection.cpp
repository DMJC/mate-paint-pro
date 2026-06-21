#include "selection.h"
#include "utils.h"
#include "undo.h"

// Check if point is inside selection
bool point_in_selection(double x, double y) {
    if (!app_state.has_selection) return false;

    if (app_state.selection_has_mask && !app_state.selection_mask.empty()) {
        int ix = static_cast<int>(std::floor(x));
        int iy = static_cast<int>(std::floor(y));
        if (!point_in_canvas(ix, iy)) return false;
        return app_state.selection_mask[iy * app_state.canvas_width + ix];
    }

    if (app_state.selection_is_rect) {
        double x1 = fmin(app_state.selection_x1, app_state.selection_x2);
        double y1 = fmin(app_state.selection_y1, app_state.selection_y2);
        double x2 = fmax(app_state.selection_x1, app_state.selection_x2);
        double y2 = fmax(app_state.selection_y1, app_state.selection_y2);
        return x >= x1 && x <= x2 && y >= y1 && y <= y2;
    }

    if (app_state.selection_path.size() < 3) return false;

    bool inside = false;
    size_t j = app_state.selection_path.size() - 1;
    for (size_t i = 0; i < app_state.selection_path.size(); i++) {
        double xi = app_state.selection_path[i].first;
        double yi = app_state.selection_path[i].second;
        double xj = app_state.selection_path[j].first;
        double yj = app_state.selection_path[j].second;

        bool intersects = ((yi > y) != (yj > y)) &&
            (x < ((xj - xi) * (y - yi) / (yj - yi) + xi));
        if (intersects) {
            inside = !inside;
        }

        j = i;
    }

    return inside;
}

// Stop ant path animation
void stop_ant_animation() {
    if (app_state.ant_timer_id != 0) {
        g_source_remove(app_state.ant_timer_id);
        app_state.ant_timer_id = 0;
    }
}

// Clear selection
void clear_selection() {
    if (app_state.floating_surface) {
        cairo_surface_destroy(app_state.floating_surface);
        app_state.floating_surface = nullptr;
    }
    app_state.floating_selection_active = false;
    app_state.dragging_selection = false;
    app_state.floating_drag_completed = false;
    app_state.has_selection = false;
    app_state.selection_path.clear();
    app_state.selection_mask.clear();
    app_state.selection_has_mask = false;
    app_state.drag_undo_snapshot_taken = false;
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void commit_floating_selection(bool record_undo) {
    if (!app_state.floating_selection_active || !app_state.floating_surface || !app_state.surface) {
        return;
    }

    double x = std::round(fmin(app_state.selection_x1, app_state.selection_x2));
    double y = std::round(fmin(app_state.selection_y1, app_state.selection_y2));

    if (record_undo) {
        push_undo_state();
    }

    cairo_t* cr = cairo_create(app_state.surface);
    configure_crisp_rendering(cr);
    cairo_set_source_surface(cr, app_state.floating_surface, x, y);
    cairo_paint(cr);
    cairo_destroy(cr);

    clear_selection();
    app_state.drag_undo_snapshot_taken = false;
}

void append_selection_path(cairo_t* cr) {
    if (app_state.selection_path.size() < 3) {
        return;
    }

    cairo_move_to(cr, app_state.selection_path[0].first, app_state.selection_path[0].second);
    for (size_t i = 1; i < app_state.selection_path.size(); i++) {
        cairo_line_to(cr, app_state.selection_path[i].first, app_state.selection_path[i].second);
    }
    cairo_close_path(cr);
}

void finalize_lasso_selection() {
    if (app_state.lasso_points.size() < 3) {
        app_state.lasso_points.clear();
        app_state.lasso_polygon_mode = false;
        app_state.is_drawing = false;
        stop_ant_animation();
        return;
    }

    app_state.has_selection = true;
    app_state.selection_is_rect = false;
    app_state.floating_selection_active = false;
    app_state.selection_has_mask = false;
    app_state.selection_mask.clear();
    app_state.selection_path = app_state.lasso_points;
    app_state.lasso_points.clear();

    if (!app_state.selection_path.empty()) {
        app_state.selection_x1 = app_state.selection_x2 = app_state.selection_path[0].first;
        app_state.selection_y1 = app_state.selection_y2 = app_state.selection_path[0].second;
        for (const auto& point : app_state.selection_path) {
            app_state.selection_x1 = fmin(app_state.selection_x1, point.first);
            app_state.selection_y1 = fmin(app_state.selection_y1, point.second);
            app_state.selection_x2 = fmax(app_state.selection_x2, point.first);
            app_state.selection_y2 = fmax(app_state.selection_y2, point.second);
        }
    }

    app_state.lasso_polygon_mode = false;
    app_state.is_drawing = false;
}

SelectionPixelBounds get_selection_pixel_bounds() {
    double x1 = fmin(app_state.selection_x1, app_state.selection_x2);
    double y1 = fmin(app_state.selection_y1, app_state.selection_y2);
    double x2 = fmax(app_state.selection_x1, app_state.selection_x2);
    double y2 = fmax(app_state.selection_y1, app_state.selection_y2);

    int pixel_x = static_cast<int>(std::floor(x1));
    int pixel_y = static_cast<int>(std::floor(y1));
    int pixel_x2 = static_cast<int>(std::ceil(x2));
    int pixel_y2 = static_cast<int>(std::ceil(y2));

    return {pixel_x, pixel_y, pixel_x2 - pixel_x, pixel_y2 - pixel_y};
}

bool selection_mask_selected_at(int x, int y) {
    if (!app_state.selection_has_mask || app_state.selection_mask.empty()) return false;
    if (!point_in_canvas(x, y)) return false;
    return app_state.selection_mask[y * app_state.canvas_width + x];
}

void copy_selection_masked_pixels(cairo_surface_t* destination, int dest_x, int dest_y,
                                  cairo_surface_t* source, const SelectionPixelBounds& bounds) {
    if (!destination || !source || !app_state.selection_has_mask || app_state.selection_mask.empty()) return;

    cairo_surface_flush(source);
    cairo_surface_flush(destination);

    unsigned char* src_data = cairo_image_surface_get_data(source);
    unsigned char* dst_data = cairo_image_surface_get_data(destination);
    int src_stride = cairo_image_surface_get_stride(source);
    int dst_stride = cairo_image_surface_get_stride(destination);
    int src_width = cairo_image_surface_get_width(source);
    int src_height = cairo_image_surface_get_height(source);
    int dst_width = cairo_image_surface_get_width(destination);
    int dst_height = cairo_image_surface_get_height(destination);

    for (int y = 0; y < bounds.height; y++) {
        int source_y = bounds.y + y;
        int target_y = dest_y + y;
        if (source_y < 0 || source_y >= src_height) continue;
        if (target_y < 0 || target_y >= dst_height) continue;

        guint32* src_row = reinterpret_cast<guint32*>(src_data + source_y * src_stride);
        guint32* dst_row = reinterpret_cast<guint32*>(dst_data + target_y * dst_stride);

        for (int x = 0; x < bounds.width; x++) {
            int source_x = bounds.x + x;
            int target_x = dest_x + x;
            if (source_x < 0 || source_x >= src_width) continue;
            if (target_x < 0 || target_x >= dst_width) continue;
            if (!selection_mask_selected_at(source_x, source_y)) continue;
            dst_row[target_x] = src_row[source_x];
        }
    }

    cairo_surface_mark_dirty(destination);
}

void clear_selection_masked_pixels(cairo_surface_t* destination, const SelectionPixelBounds& bounds, guint32 fill_pixel) {
    if (!destination || !app_state.selection_has_mask || app_state.selection_mask.empty()) return;

    cairo_surface_flush(destination);
    unsigned char* dst_data = cairo_image_surface_get_data(destination);
    int dst_stride = cairo_image_surface_get_stride(destination);

    for (int y = 0; y < bounds.height; y++) {
        int target_y = bounds.y + y;
        if (target_y < 0 || target_y >= app_state.canvas_height) continue;
        guint32* dst_row = reinterpret_cast<guint32*>(dst_data + target_y * dst_stride);

        for (int x = 0; x < bounds.width; x++) {
            int target_x = bounds.x + x;
            if (target_x < 0 || target_x >= app_state.canvas_width) continue;
            if (!selection_mask_selected_at(target_x, target_y)) continue;
            dst_row[target_x] = fill_pixel;
        }
    }

    cairo_surface_mark_dirty(destination);
}

void start_selection_drag() {
    if (!app_state.has_selection || !app_state.surface) return;

    if (app_state.floating_selection_active) return;

    if (!app_state.drag_undo_snapshot_taken) {
        push_undo_state();
        app_state.drag_undo_snapshot_taken = true;
    }

    SelectionPixelBounds bounds = get_selection_pixel_bounds();
    int w = bounds.width;
    int h = bounds.height;
    if (w <= 0 || h <= 0) return;

    app_state.floating_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t* float_cr = cairo_create(app_state.floating_surface);
    configure_crisp_rendering(float_cr);

    cairo_set_operator(float_cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(float_cr, 0, 0, 0, 0);
    cairo_paint(float_cr);
    cairo_set_operator(float_cr, CAIRO_OPERATOR_OVER);

    if (app_state.selection_has_mask && !app_state.selection_mask.empty()) {
        copy_selection_masked_pixels(app_state.floating_surface, 0, 0, app_state.surface, bounds);
    } else if (app_state.selection_is_rect) {
        cairo_set_source_surface(float_cr, app_state.surface, -bounds.x, -bounds.y);
        cairo_paint(float_cr);
    } else if (app_state.selection_path.size() > 2) {
        cairo_save(float_cr);
        cairo_translate(float_cr, -bounds.x, -bounds.y);
	append_selection_path(float_cr);
        cairo_clip(float_cr);
        cairo_set_source_surface(float_cr, app_state.surface, 0, 0);
        cairo_paint(float_cr);
        cairo_restore(float_cr);
    }
    cairo_destroy(float_cr);

    cairo_t* cr = cairo_create(app_state.surface);
    configure_crisp_rendering(cr);
    cairo_set_source_rgba(cr,
        app_state.bg_color.red,
        app_state.bg_color.green,
        app_state.bg_color.blue,
        app_state.bg_color.alpha
    );

    if (app_state.selection_has_mask && !app_state.selection_mask.empty()) {
        clear_selection_masked_pixels(app_state.surface, bounds, rgba_to_pixel(app_state.bg_color));
    } else if (app_state.selection_is_rect) {
        cairo_rectangle(cr, bounds.x, bounds.y, w, h);
        cairo_fill(cr);
    } else if (app_state.selection_path.size() > 2) {
        append_selection_path(cr);
        cairo_fill(cr);
    }
    cairo_destroy(cr);

    app_state.selection_x1 = bounds.x;
    app_state.selection_y1 = bounds.y;
    app_state.selection_x2 = bounds.x + w;
    app_state.selection_y2 = bounds.y + h;

    app_state.floating_selection_active = true;
    app_state.floating_drag_completed = false;
}

// Copy selection to clipboard
void copy_selection() {
    if (!app_state.has_selection || !app_state.surface) return;

    SelectionPixelBounds bounds = get_selection_pixel_bounds();
    int w = bounds.width;
    int h = bounds.height;

    if (w <= 0 || h <= 0) return;

    if (app_state.clipboard_surface) {
        cairo_surface_destroy(app_state.clipboard_surface);
    }

    app_state.clipboard_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    app_state.clipboard_width = w;
    app_state.clipboard_height = h;

    cairo_t* cr = cairo_create(app_state.clipboard_surface);
    configure_crisp_rendering(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (app_state.floating_selection_active && app_state.floating_surface) {
        cairo_set_source_surface(cr, app_state.floating_surface, 0, 0);
        cairo_paint(cr);
    } else if (app_state.selection_has_mask && !app_state.selection_mask.empty()) {
        copy_selection_masked_pixels(app_state.clipboard_surface, 0, 0, app_state.surface, bounds);
    } else if (app_state.selection_is_rect) {
        cairo_set_source_surface(cr, app_state.surface, -bounds.x, -bounds.y);
        cairo_paint(cr);
    } else if (app_state.selection_path.size() > 2) {
        cairo_save(cr);
        cairo_translate(cr, -bounds.x, -bounds.y);
        append_selection_path(cr);
        cairo_clip(cr);
        cairo_set_source_surface(cr, app_state.surface, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    }

    cairo_destroy(cr);
    copy_surface_to_system_clipboard(app_state.clipboard_surface);
}

// Cut selection to clipboard
void cut_selection() {
    if (!app_state.has_selection || !app_state.surface) return;

    copy_selection();

    if (app_state.floating_selection_active) {
        clear_selection();
        return;
    }

    cairo_t* cr = cairo_create(app_state.surface);
    configure_crisp_rendering(cr);
    cairo_set_source_rgba(cr,
        app_state.bg_color.red,
        app_state.bg_color.green,
        app_state.bg_color.blue,
        app_state.bg_color.alpha
    );

    bool did_clear = false;

    if (app_state.selection_has_mask && !app_state.selection_mask.empty()) {
        SelectionPixelBounds bounds = get_selection_pixel_bounds();

        push_undo_state();

        clear_selection_masked_pixels(app_state.surface, bounds, rgba_to_pixel(app_state.bg_color));
        did_clear = true;
    }

    if (!did_clear && app_state.selection_is_rect) {
        SelectionPixelBounds bounds = get_selection_pixel_bounds();

        push_undo_state();

        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
        cairo_fill(cr);
    } else if (!did_clear && app_state.selection_path.size() > 2) {
        append_selection_path(cr);
        cairo_fill(cr);
    }
    cairo_destroy(cr);

    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }

    clear_selection();
	app_state.drag_undo_snapshot_taken = false;
}

// Paste from clipboard
void paste_selection() {
    if (!app_state.surface) return;

    int clipboard_width = 0;
    int clipboard_height = 0;
    cairo_surface_t* system_surface = get_surface_from_system_clipboard(clipboard_width, clipboard_height);

    if (system_surface) {
        if (app_state.clipboard_surface) {
            cairo_surface_destroy(app_state.clipboard_surface);
        }
        app_state.clipboard_surface = system_surface;
        app_state.clipboard_width = clipboard_width;
        app_state.clipboard_height = clipboard_height;
    }

    if (!app_state.clipboard_surface) return;

    bool exceeds_canvas = app_state.clipboard_width > app_state.canvas_width ||
        app_state.clipboard_height > app_state.canvas_height;
    if (exceeds_canvas &&
        should_expand_canvas_for_paste(app_state.clipboard_width, app_state.clipboard_height)) {
        resize_canvas_for_paste(
            std::max(app_state.canvas_width, app_state.clipboard_width),
            std::max(app_state.canvas_height, app_state.clipboard_height)
        );
    }

    clear_selection();

    double paste_x = 20;
    double paste_y = 20;

    app_state.floating_surface = cairo_surface_reference(app_state.clipboard_surface);
    app_state.floating_selection_active = true;
    app_state.floating_drag_completed = false;
    app_state.dragging_selection = false;

    app_state.has_selection = true;
    app_state.selection_is_rect = true;
    app_state.selection_path.clear();
    app_state.selection_has_mask = false;
    app_state.selection_mask.clear();
    app_state.selection_x1 = paste_x;
    app_state.selection_y1 = paste_y;
    app_state.selection_x2 = paste_x + app_state.clipboard_width;
    app_state.selection_y2 = paste_y + app_state.clipboard_height;

    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void copy_surface_to_system_clipboard(cairo_surface_t* surface) {
    if (!surface || !app_state.window) return;

    GtkClipboard* clipboard = gtk_widget_get_clipboard(app_state.window, GDK_SELECTION_CLIPBOARD);
    if (!clipboard) return;

    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    if (width <= 0 || height <= 0) return;

    GdkPixbuf* pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
    if (!pixbuf) return;

    gtk_clipboard_set_image(clipboard, pixbuf);
    g_object_unref(pixbuf);
}

cairo_surface_t* get_surface_from_system_clipboard(int& width, int& height) {
    width = 0;
    height = 0;

    if (!app_state.window) return nullptr;

    GtkClipboard* clipboard = gtk_widget_get_clipboard(app_state.window, GDK_SELECTION_CLIPBOARD);
    if (!clipboard || !gtk_clipboard_wait_is_image_available(clipboard)) return nullptr;

    GdkPixbuf* pixbuf = gtk_clipboard_wait_for_image(clipboard);
    if (!pixbuf) return nullptr;

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    cairo_surface_t* surface = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, nullptr);
    g_object_unref(pixbuf);

    return surface;
}

bool should_expand_canvas_for_paste(int pasted_width, int pasted_height) {
    if (!app_state.window) return false;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Pasted Image Is Larger Than Canvas"),
        GTK_WINDOW(app_state.window),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Keep Canvas Size"), GTK_RESPONSE_CANCEL,
        _("_Expand Canvas"), GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* label = gtk_label_new(
        _("The pasted image is larger than the current canvas.\nWould you like to keep the current canvas size or expand it to fit the pasted image?")
    );
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);
    gtk_container_add(GTK_CONTAINER(content), label);

    char details[128];
    std::snprintf(
        details,
        sizeof(details),
        _("Canvas: %d x %d    Pasted image: %d x %d"),
        app_state.canvas_width,
        app_state.canvas_height,
        pasted_width,
        pasted_height
    );
    GtkWidget* detail_label = gtk_label_new(details);
    gtk_label_set_xalign(GTK_LABEL(detail_label), 0.0);
    gtk_container_add(GTK_CONTAINER(content), detail_label);

    gtk_widget_show_all(dialog);
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return response == GTK_RESPONSE_ACCEPT;
}

void resize_canvas_for_paste(int new_width, int new_height) {
    if (!app_state.surface) return;
    if (new_width <= app_state.canvas_width && new_height <= app_state.canvas_height) return;

    push_undo_state();

    cairo_surface_t* old_surface = app_state.surface;
    cairo_surface_t* resized_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);

    cairo_t* cr = cairo_create(resized_surface);
    configure_crisp_rendering(cr);
    cairo_set_source_rgba(
        cr,
        app_state.bg_color.red,
        app_state.bg_color.green,
        app_state.bg_color.blue,
        app_state.bg_color.alpha
    );
    cairo_paint(cr);
    cairo_set_source_surface(cr, old_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    app_state.surface = resized_surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    app_state.canvas_width = new_width;
    app_state.canvas_height = new_height;
    cairo_surface_destroy(old_surface);

    if (app_state.drawing_area) {
        gtk_widget_set_size_request(app_state.drawing_area, new_width, new_height);
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

// Ant path timer callback
gboolean ant_path_timer(gpointer data) {
    app_state.ant_offset += 1.0;
    if (app_state.ant_offset >= 8.0) {
        app_state.ant_offset = 0.0;
    }
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
    return TRUE;
}

// Start ant path animation
void start_ant_animation() {
    if (app_state.ant_timer_id == 0) {
        app_state.ant_timer_id = g_timeout_add(50, ant_path_timer, NULL);
    }
}

// Draw ant path (marching ants)
void draw_ant_path(cairo_t* cr) {
    double dashes[] = {4.0, 4.0};
    cairo_set_dash(cr, dashes, 2, app_state.ant_offset);
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgb(cr, 0, 0, 0);
}
