#include "layers.h"
#include "utils.h"

void sync_layer_controls() {
    if (app_state.layers.empty()) {
        return;
    }

    if (app_state.layer_move_up_button) {
        gtk_widget_set_sensitive(app_state.layer_move_up_button, app_state.active_layer_index < (int)app_state.layers.size() - 1);
    }
    if (app_state.layer_move_down_button) {
        gtk_widget_set_sensitive(app_state.layer_move_down_button, app_state.active_layer_index > 0);
    }
    if (app_state.merge_layer_button) {
        gtk_widget_set_sensitive(app_state.merge_layer_button, app_state.active_layer_index > 0 && app_state.layers.size() > 1);
    }
    if (app_state.layer_opacity_scale) {
        gtk_range_set_value(GTK_RANGE(app_state.layer_opacity_scale), app_state.layers[app_state.active_layer_index].opacity);
    }
}

void set_active_layer(int index) {
    if (index < 0 || index >= (int)app_state.layers.size()) {
        return;
    }
    app_state.active_layer_index = index;
    app_state.surface = app_state.layers[index].surface;
    rebuild_layer_panel();
}

void clear_layers() {
    for (Layer& layer : app_state.layers) {
        if (layer.surface) {
            cairo_surface_destroy(layer.surface);
            layer.surface = nullptr;
        }
    }
    app_state.layers.clear();
    app_state.active_layer_index = 0;
    app_state.surface = nullptr;
}

void ensure_default_layers() {
    clear_layers();
    Layer layer;
    layer.name = "Layer 1";
    layer.visible = true;
    layer.surface = create_blank_surface(app_state.canvas_width, app_state.canvas_height, true);
    app_state.layers.push_back(layer);
    set_active_layer(0);
}

void add_new_layer() {
    Layer layer;
    layer.name = "Layer " + std::to_string(app_state.layers.size() + 1);
    layer.visible = true;
    layer.surface = create_blank_surface(app_state.canvas_width, app_state.canvas_height, false);
    app_state.layers.push_back(layer);
    set_active_layer((int)app_state.layers.size() - 1);
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void duplicate_active_layer() {
    if (app_state.active_layer_index < 0 || app_state.active_layer_index >= (int)app_state.layers.size()) {
        return;
    }

    Layer& source_layer = app_state.layers[app_state.active_layer_index];
    Layer duplicated_layer;
    duplicated_layer.name = source_layer.name + " Copy";
    duplicated_layer.visible = source_layer.visible;
    duplicated_layer.opacity = source_layer.opacity;
    duplicated_layer.surface = clone_surface(source_layer.surface, app_state.canvas_width, app_state.canvas_height);

    if (!duplicated_layer.surface) {
        return;
    }

    app_state.layers.insert(app_state.layers.begin() + app_state.active_layer_index + 1, duplicated_layer);
    set_active_layer(app_state.active_layer_index + 1);
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void move_layer_up(int index) {
    if (index < 0 || index >= (int)app_state.layers.size() - 1) {
        return;
    }
    std::swap(app_state.layers[index], app_state.layers[index + 1]);
    app_state.active_layer_index = index + 1;
    app_state.surface = app_state.layers[app_state.active_layer_index].surface;
    rebuild_layer_panel();
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void move_layer_down(int index) {
    if (index <= 0 || index >= (int)app_state.layers.size()) {
        return;
    }
    std::swap(app_state.layers[index], app_state.layers[index - 1]);
    app_state.active_layer_index = index - 1;
    app_state.surface = app_state.layers[app_state.active_layer_index].surface;
    rebuild_layer_panel();
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void merge_layer_down(int index) {
    if (index <= 0 || index >= (int)app_state.layers.size()) {
        return;
    }

    Layer& source_layer = app_state.layers[index];
    Layer& target_layer = app_state.layers[index - 1];
    if (!source_layer.surface || !target_layer.surface) {
        return;
    }

    cairo_t* cr = cairo_create(target_layer.surface);
    cairo_set_source_surface(cr, source_layer.surface, 0, 0);
    cairo_paint_with_alpha(cr, source_layer.opacity);
    cairo_destroy(cr);

    cairo_surface_destroy(source_layer.surface);
    source_layer.surface = nullptr;
    app_state.layers[index - 1].opacity = 1.0;
    app_state.layers.erase(app_state.layers.begin() + index);

    app_state.active_layer_index = index - 1;
    app_state.surface = app_state.layers[app_state.active_layer_index].surface;

    rebuild_layer_panel();
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void delete_layer(int index) {
    if (index < 0 || index >= (int)app_state.layers.size()) {
        return;
    }
    if (app_state.layers.size() <= 1) {
        return;
    }

    if (app_state.layers[index].surface) {
        cairo_surface_destroy(app_state.layers[index].surface);
        app_state.layers[index].surface = nullptr;
    }

    app_state.layers.erase(app_state.layers.begin() + index);
    if (app_state.active_layer_index >= (int)app_state.layers.size()) {
        app_state.active_layer_index = (int)app_state.layers.size() - 1;
    } else if (app_state.active_layer_index > index) {
        app_state.active_layer_index--;
    } else if (app_state.active_layer_index == index) {
        app_state.active_layer_index = std::max(0, index - 1);
    }

    set_active_layer(app_state.active_layer_index);
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void rebuild_layer_panel() {
    if (!app_state.layer_list_box) return;
    GList* children = gtk_container_get_children(GTK_CONTAINER(app_state.layer_list_box));
    for (GList* l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    GSList* group = NULL;
    for (int i = (int)app_state.layers.size() - 1; i >= 0; --i) {
        Layer& layer = app_state.layers[i];
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        layer.visible_check = gtk_check_button_new();
       gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(layer.visible_check), layer.visible);
        g_signal_connect(layer.visible_check, "toggled", G_CALLBACK(+[](GtkToggleButton* btn, gpointer data) {
            int idx = GPOINTER_TO_INT(data);
            app_state.layers[idx].visible = gtk_toggle_button_get_active(btn);
            if (app_state.drawing_area) gtk_widget_queue_draw(app_state.drawing_area);
        }), GINT_TO_POINTER(i));

        layer.select_button = gtk_radio_button_new(group);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(layer.select_button));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(layer.select_button), i == app_state.active_layer_index);
        g_signal_connect(layer.select_button, "toggled", G_CALLBACK(+[](GtkToggleButton* btn, gpointer data) {
           if (!gtk_toggle_button_get_active(btn)) return;
            set_active_layer(GPOINTER_TO_INT(data));
        }), GINT_TO_POINTER(i));

        layer.name_entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(layer.name_entry), layer.name.c_str());
        g_signal_connect(layer.name_entry, "changed", G_CALLBACK(+[](GtkEditable* editable, gpointer data) {
            int idx = GPOINTER_TO_INT(data);
            app_state.layers[idx].name = gtk_entry_get_text(GTK_ENTRY(editable));
        }), GINT_TO_POINTER(i));

        GtkWidget* delete_button = gtk_button_new_with_label("-");
        gtk_widget_set_sensitive(delete_button, app_state.layers.size() > 1);
        g_signal_connect(delete_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
            delete_layer(GPOINTER_TO_INT(data));
        }), GINT_TO_POINTER(i));

        gtk_box_pack_start(GTK_BOX(row), layer.visible_check, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), layer.select_button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), layer.name_entry, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), delete_button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(app_state.layer_list_box), row, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(app_state.layer_list_box);
    sync_layer_controls();
}

cairo_surface_t* compose_visible_layers_surface() {
    cairo_surface_t* composed = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, app_state.canvas_width, app_state.canvas_height);
    cairo_t* cr = cairo_create(composed);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    for (const Layer& layer : app_state.layers) {
        if (!layer.visible || !layer.surface) continue;
        cairo_set_source_surface(cr, layer.surface, 0, 0);
        cairo_paint_with_alpha(cr, layer.opacity);
    }
    cairo_destroy(cr);
    return composed;
}
