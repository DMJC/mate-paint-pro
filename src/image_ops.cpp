#include "image_ops.h"
#include "utils.h"
#include "layers.h"
#include "undo.h"
#include "selection.h"
#include "text_tool.h"
#include <cmath>
#include <algorithm>

void on_image_scale(GtkMenuItem* item, gpointer data) {
    if (!app_state.surface) return;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Scale Image"),
        GTK_WINDOW(app_state.window),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Scale"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(row), 10);
    gtk_container_add(GTK_CONTAINER(content), row);

    GtkWidget* percent_label = gtk_label_new(_("Scale (%):"));
    GtkWidget* percent_spin = gtk_spin_button_new_with_range(1, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(percent_spin), 100);

    gtk_box_pack_start(GTK_BOX(row), percent_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), percent_spin, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    double scale = gtk_spin_button_get_value(GTK_SPIN_BUTTON(percent_spin)) / 100.0;
    gtk_widget_destroy(dialog);

    int new_width = std::max(1, (int)std::lround(app_state.canvas_width * scale));
    int new_height = std::max(1, (int)std::lround(app_state.canvas_height * scale));

    push_undo_state();

    cairo_surface_t* old_surface = app_state.surface;
    cairo_surface_t* scaled_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);

    cairo_t* cr = cairo_create(scaled_surface);
    configure_crisp_rendering(cr);
    cairo_scale(cr, (double)new_width / app_state.canvas_width, (double)new_height / app_state.canvas_height);
    cairo_set_source_surface(cr, old_surface, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_destroy(cr);

    app_state.surface = scaled_surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    app_state.canvas_width = new_width;
    app_state.canvas_height = new_height;
    cairo_surface_destroy(old_surface);

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }

    gtk_widget_set_size_request(app_state.drawing_area, new_width, new_height);
    gtk_widget_queue_draw(app_state.drawing_area);
}

void on_image_resize_canvas(GtkMenuItem* item, gpointer data) {
    if (!app_state.surface) return;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Resize Image"),
        GTK_WINDOW(app_state.window),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Resize"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(container), 10);
    gtk_container_add(GTK_CONTAINER(content), container);

    GtkWidget* width_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* width_label = gtk_label_new(_("Width:"));
    GtkWidget* width_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(width_spin), app_state.canvas_width);
    gtk_box_pack_start(GTK_BOX(width_row), width_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(width_row), width_spin, FALSE, FALSE, 0);

    GtkWidget* height_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* height_label = gtk_label_new(_("Height:"));
    GtkWidget* height_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(height_spin), app_state.canvas_height);
    gtk_box_pack_start(GTK_BOX(height_row), height_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(height_row), height_spin, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(container), width_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), height_row, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    int new_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(width_spin));
    int new_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(height_spin));
    gtk_widget_destroy(dialog);

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

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }

    gtk_widget_set_size_request(app_state.drawing_area, new_width, new_height);
    gtk_widget_queue_draw(app_state.drawing_area);
}

void on_image_rotate_clockwise(GtkMenuItem* item, gpointer data) {
    if (!app_state.surface) return;

    if (app_state.has_selection) {
        if (!app_state.floating_selection_active) {
            start_selection_drag();
        }
        if (!app_state.floating_selection_active || !app_state.floating_surface) {
            return;
        }

        push_undo_state();

        SelectionPixelBounds bounds = get_selection_pixel_bounds();
        const int new_width = bounds.height;
        const int new_height = bounds.width;

        cairo_surface_t* old_floating_surface = app_state.floating_surface;
        cairo_surface_t* rotated_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);
        cairo_t* cr = cairo_create(rotated_surface);
        configure_crisp_rendering(cr);

        cairo_translate(cr, new_width, 0);
        cairo_rotate(cr, M_PI / 2.0);
        cairo_set_source_surface(cr, old_floating_surface, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);

        app_state.floating_surface = rotated_surface;
        cairo_surface_destroy(old_floating_surface);

        app_state.selection_is_rect = true;
        app_state.selection_path.clear();
        app_state.selection_has_mask = false;
        app_state.selection_mask.clear();
        app_state.selection_x1 = bounds.x;
        app_state.selection_y1 = bounds.y;
        app_state.selection_x2 = bounds.x + new_width;
        app_state.selection_y2 = bounds.y + new_height;

        gtk_widget_queue_draw(app_state.drawing_area);
        return;
    }

    const int old_width = app_state.canvas_width;
    const int old_height = app_state.canvas_height;
    const int new_width = old_height;
    const int new_height = old_width;

    push_undo_state();

    cairo_surface_t* old_surface = app_state.surface;
    cairo_surface_t* rotated_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);
    cairo_t* cr = cairo_create(rotated_surface);
    configure_crisp_rendering(cr);

    cairo_translate(cr, new_width, 0);
    cairo_rotate(cr, M_PI / 2.0);
    cairo_set_source_surface(cr, old_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    app_state.surface = rotated_surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    app_state.canvas_width = new_width;
    app_state.canvas_height = new_height;
    cairo_surface_destroy(old_surface);

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }

    gtk_widget_set_size_request(app_state.drawing_area, new_width, new_height);
    gtk_widget_queue_draw(app_state.drawing_area);
}

void on_image_rotate_counter_clockwise(GtkMenuItem* item, gpointer data) {
    if (!app_state.surface) return;

    if (app_state.has_selection) {
        if (!app_state.floating_selection_active) {
            start_selection_drag();
        }
        if (!app_state.floating_selection_active || !app_state.floating_surface) {
            return;
        }

        push_undo_state();

        SelectionPixelBounds bounds = get_selection_pixel_bounds();
        const int new_width = bounds.height;
        const int new_height = bounds.width;

        cairo_surface_t* old_floating_surface = app_state.floating_surface;
        cairo_surface_t* rotated_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);
        cairo_t* cr = cairo_create(rotated_surface);
        configure_crisp_rendering(cr);

        cairo_translate(cr, 0, new_height);
        cairo_rotate(cr, -M_PI / 2.0);
        cairo_set_source_surface(cr, old_floating_surface, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);

        app_state.floating_surface = rotated_surface;
        cairo_surface_destroy(old_floating_surface);

        app_state.selection_is_rect = true;
        app_state.selection_path.clear();
        app_state.selection_has_mask = false;
        app_state.selection_mask.clear();
        app_state.selection_x1 = bounds.x;
        app_state.selection_y1 = bounds.y;
        app_state.selection_x2 = bounds.x + new_width;
        app_state.selection_y2 = bounds.y + new_height;

        gtk_widget_queue_draw(app_state.drawing_area);
        return;
    }

    const int old_width = app_state.canvas_width;
    const int old_height = app_state.canvas_height;
    const int new_width = old_height;
    const int new_height = old_width;

    push_undo_state();

    cairo_surface_t* old_surface = app_state.surface;
    cairo_surface_t* rotated_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);
    cairo_t* cr = cairo_create(rotated_surface);
    configure_crisp_rendering(cr);

    cairo_translate(cr, 0, new_height);
    cairo_rotate(cr, -M_PI / 2.0);
    cairo_set_source_surface(cr, old_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    app_state.surface = rotated_surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    app_state.canvas_width = new_width;
    app_state.canvas_height = new_height;
    cairo_surface_destroy(old_surface);

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }

    gtk_widget_set_size_request(app_state.drawing_area, new_width, new_height);
    gtk_widget_queue_draw(app_state.drawing_area);
}

void on_image_flip_horizontal(GtkMenuItem* item, gpointer data) {
    if (!app_state.surface) return;

    if (app_state.has_selection) {
        if (!app_state.floating_selection_active) {
            start_selection_drag();
        }
        if (!app_state.floating_selection_active || !app_state.floating_surface) {
            return;
        }

        push_undo_state();

        SelectionPixelBounds bounds = get_selection_pixel_bounds();

        cairo_surface_t* old_floating_surface = app_state.floating_surface;
        cairo_surface_t* flipped_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bounds.width, bounds.height);
        cairo_t* cr = cairo_create(flipped_surface);
        configure_crisp_rendering(cr);

        cairo_translate(cr, bounds.width, 0);
        cairo_scale(cr, -1, 1);
        cairo_set_source_surface(cr, old_floating_surface, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);

        app_state.floating_surface = flipped_surface;
        cairo_surface_destroy(old_floating_surface);

        app_state.selection_is_rect = true;
        app_state.selection_path.clear();
        app_state.selection_has_mask = false;
        app_state.selection_mask.clear();

        gtk_widget_queue_draw(app_state.drawing_area);
        return;
    }

    const int width = app_state.canvas_width;
    const int height = app_state.canvas_height;

    push_undo_state();

    cairo_surface_t* old_surface = app_state.surface;
    cairo_surface_t* flipped_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(flipped_surface);
    configure_crisp_rendering(cr);

    cairo_translate(cr, width, 0);
    cairo_scale(cr, -1, 1);
    cairo_set_source_surface(cr, old_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    app_state.surface = flipped_surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    cairo_surface_destroy(old_surface);

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }

    gtk_widget_queue_draw(app_state.drawing_area);
}

void on_image_flip_vertical(GtkMenuItem* item, gpointer data) {
    if (!app_state.surface) return;

    if (app_state.has_selection) {
        if (!app_state.floating_selection_active) {
            start_selection_drag();
        }
        if (!app_state.floating_selection_active || !app_state.floating_surface) {
            return;
        }

        push_undo_state();

        SelectionPixelBounds bounds = get_selection_pixel_bounds();

        cairo_surface_t* old_floating_surface = app_state.floating_surface;
        cairo_surface_t* flipped_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bounds.width, bounds.height);
        cairo_t* cr = cairo_create(flipped_surface);
        configure_crisp_rendering(cr);

        cairo_translate(cr, 0, bounds.height);
        cairo_scale(cr, 1, -1);
        cairo_set_source_surface(cr, old_floating_surface, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);

        app_state.floating_surface = flipped_surface;
        cairo_surface_destroy(old_floating_surface);

        app_state.selection_is_rect = true;
        app_state.selection_path.clear();
        app_state.selection_has_mask = false;
        app_state.selection_mask.clear();

        gtk_widget_queue_draw(app_state.drawing_area);
        return;
    }

    const int width = app_state.canvas_width;
    const int height = app_state.canvas_height;

    push_undo_state();

    cairo_surface_t* old_surface = app_state.surface;
    cairo_surface_t* flipped_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(flipped_surface);
    configure_crisp_rendering(cr);

    cairo_translate(cr, 0, height);
    cairo_scale(cr, 1, -1);
    cairo_set_source_surface(cr, old_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    app_state.surface = flipped_surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    cairo_surface_destroy(old_surface);

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }

    gtk_widget_queue_draw(app_state.drawing_area);
}

void crop_to_rectangle(double x1, double y1, double x2, double y2) {
    if (!app_state.surface) return;

    int left = static_cast<int>(std::floor(fmin(x1, x2)));
    int top = static_cast<int>(std::floor(fmin(y1, y2)));
    int right = static_cast<int>(std::ceil(fmax(x1, x2)));
    int bottom = static_cast<int>(std::ceil(fmax(y1, y2)));

    left = std::max(0, std::min(left, app_state.canvas_width));
    top = std::max(0, std::min(top, app_state.canvas_height));
    right = std::max(0, std::min(right, app_state.canvas_width));
    bottom = std::max(0, std::min(bottom, app_state.canvas_height));

    int new_width = right - left;
    int new_height = bottom - top;
    if (new_width <= 0 || new_height <= 0) {
        clear_selection();
        return;
    }

    push_undo_state();

    cairo_surface_t* old_surface = app_state.surface;
    cairo_surface_t* cropped_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);

    cairo_t* cr = cairo_create(cropped_surface);
    configure_crisp_rendering(cr);
    cairo_set_source_rgba(
        cr,
        app_state.bg_color.red,
        app_state.bg_color.green,
        app_state.bg_color.blue,
        app_state.bg_color.alpha
    );
    cairo_paint(cr);
    cairo_set_source_surface(cr, old_surface, -left, -top);
    cairo_paint(cr);
    cairo_destroy(cr);

    app_state.surface = cropped_surface;
    if (!app_state.layers.empty()) app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    app_state.canvas_width = new_width;
    app_state.canvas_height = new_height;
    cairo_surface_destroy(old_surface);

    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }

    update_canvas_dimensions_label();
    gtk_widget_set_size_request(
        app_state.drawing_area,
        static_cast<int>(app_state.canvas_width * app_state.zoom_factor),
        static_cast<int>(app_state.canvas_height * app_state.zoom_factor)
    );
    gtk_widget_queue_draw(app_state.drawing_area);
}
