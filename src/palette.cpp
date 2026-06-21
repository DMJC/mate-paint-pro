#include "palette.h"
#include "utils.h"

std::string get_config_file_path() {
    gchar* path = g_build_filename(g_get_user_config_dir(), "mate", "mate-paint-pro", "mate-paint.cfg", NULL);
    std::string config_path(path);
    g_free(path);
    return config_path;
}

int get_custom_palette_start_index() {
    return static_cast<int>((sizeof(palette_colors) / sizeof(palette_colors[0])) + (sizeof(additional_palette_colors) / sizeof(additional_palette_colors[0])) - custom_palette_slot_count);
}

void load_custom_palette_colors() {
    GKeyFile* key_file = g_key_file_new();
    std::string config_path = get_config_file_path();
    GError* error = NULL;
    if (!g_key_file_load_from_file(key_file, config_path.c_str(), G_KEY_FILE_NONE, &error)) {
        if (error) {
            g_error_free(error);
        }
        g_key_file_unref(key_file);
        return;
    }

    const int custom_start_index = get_custom_palette_start_index();
    for (int i = 0; i < custom_palette_slot_count; ++i) {
        gchar* key = g_strdup_printf("custom_color_%d", i + 1);
        gchar* color_string = g_key_file_get_string(key_file, "palette", key, NULL);
        g_free(key);

        if (!color_string) {
            continue;
        }

        GdkRGBA color;
        if (gdk_rgba_parse(&color, color_string)) {
            app_state.palette_button_colors[custom_start_index + i] = color;
        }

        g_free(color_string);
    }

    g_key_file_unref(key_file);
}

void save_custom_palette_colors() {
    GKeyFile* key_file = g_key_file_new();
    std::string config_path = get_config_file_path();

    const int custom_start_index = get_custom_palette_start_index();
    for (int i = 0; i < custom_palette_slot_count; ++i) {
        gchar* key = g_strdup_printf("custom_color_%d", i + 1);
        gchar* color_string = gdk_rgba_to_string(&app_state.palette_button_colors[custom_start_index + i]);
        g_key_file_set_string(key_file, "palette", key, color_string);
        g_free(color_string);
        g_free(key);
    }

    gsize data_len = 0;
    gchar* data = g_key_file_to_data(key_file, &data_len, NULL);
    if (data) {
        gchar* config_dir = g_path_get_dirname(config_path.c_str());
        g_mkdir_with_parents(config_dir, 0755);
        g_file_set_contents(config_path.c_str(), data, data_len, NULL);
        g_free(config_dir);
        g_free(data);
    }

    g_key_file_unref(key_file);
}

// Custom draw function for color indicator buttons
gboolean on_color_button_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    bool is_foreground = GPOINTER_TO_INT(data);
    GdkRGBA* color = is_foreground ? &app_state.fg_color : &app_state.bg_color;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    cairo_set_source_rgba(cr, color->red, color->green, color->blue, color->alpha);
    cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, 1, 1, alloc.width - 2, alloc.height - 2);
    cairo_stroke(cr);

    if (is_transparent_color(*color)) {
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, fmax(10.0, alloc.height * 0.7));

        cairo_text_extents_t extents;
        cairo_text_extents(cr, "T", &extents);

        double text_x = (alloc.width - extents.width) / 2.0 - extents.x_bearing;
        double text_y = (alloc.height - extents.height) / 2.0 - extents.y_bearing;

        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_move_to(cr, text_x, text_y);
        cairo_show_text(cr, "T");
    }

    return TRUE;
}

// Update the foreground/background color indicator buttons
void update_color_indicators() {
    if (app_state.fg_button) {
        gtk_widget_queue_draw(app_state.fg_button);
    }

    if (app_state.bg_button) {
        gtk_widget_queue_draw(app_state.bg_button);
    }
}

void apply_color_button_style(GtkWidget* button, const GdkRGBA& color, bool is_custom_slot) {
    double brightness = (color.red * 0.299) + (color.green * 0.587) + (color.blue * 0.114);
    const char* text_color = brightness > 0.5 ? "#111" : "#fff";

    gchar* css = g_strdup_printf(
        "button { "
        "background-color: rgb(%d,%d,%d); "
        "color: %s; "
        "background-image: none; "
        "border: 1px solid #555; "
        "min-width: 18px; "
        "min-height: 18px; "
        "font-weight: bold; "
        "padding: 0; "
        "margin: 0; "
        "}"
        "button:hover { "
        "border: 1px solid #000; "
        "}",
        (int)(color.red * 255),
        (int)(color.green * 255),
        (int)(color.blue * 255),
        (is_custom_slot || is_transparent_color(color)) ? text_color : "transparent"
    );

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(button),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );

    g_free(css);
    g_object_unref(provider);
}

void show_custom_color_dialog(int index) {
    if (index < 0 || index >= (int)app_state.palette_button_colors.size()) {
        return;
    }

    GtkWidget* dialog = gtk_color_chooser_dialog_new(_("Custom color"), GTK_WINDOW(app_state.window));
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog), &app_state.palette_button_colors[index]);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        GdkRGBA selected_color;
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dialog), &selected_color);
        app_state.palette_button_colors[index] = selected_color;

        if (index < (int)app_state.palette_buttons.size() && app_state.palette_buttons[index]) {
            apply_color_button_style(app_state.palette_buttons[index], selected_color, true);
        }

        save_custom_palette_colors();
    }

    gtk_widget_destroy(dialog);
}

// Color button callback
gboolean on_color_button_press(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    if (index < 0 || index >= (int)app_state.palette_button_colors.size()) {
        return TRUE;
    }

    bool is_custom_slot = index < (int)app_state.custom_palette_slots.size() && app_state.custom_palette_slots[index];
    if (is_custom_slot && event->type == GDK_2BUTTON_PRESS) {
        show_custom_color_dialog(index);
        return TRUE;
    }

    if (event->button == 1) {
        app_state.fg_color = app_state.palette_button_colors[index];
        update_color_indicators();
    } else if (event->button == 3) {
        app_state.bg_color = app_state.palette_button_colors[index];
        update_color_indicators();
    }
    return TRUE;
}

static const char* get_palette_button_label(int index, bool is_custom_slot) {
    if (index == 0) {
        return "T";
    }

    if (is_custom_slot) {
        return "c";
    }

    return "";
}

// Create color button
GtkWidget* create_color_button(GdkRGBA color, int index, bool is_custom_slot) {
    GtkWidget* button = gtk_button_new_with_label(get_palette_button_label(index, is_custom_slot));
    gtk_widget_set_size_request(button, 18, 18);

    apply_color_button_style(button, color, is_custom_slot);

    gtk_widget_add_events(button, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(button, "button-press-event", G_CALLBACK(on_color_button_press), GINT_TO_POINTER(index));

    return button;
}
