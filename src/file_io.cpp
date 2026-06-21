#include "file_io.h"
#include "utils.h"
#include "layers.h"
#include "undo.h"
#include "selection.h"
#include "text_tool.h"

// File operations
void save_image_dialog(GtkWidget* parent) {
    if (app_state.floating_selection_active) {
        commit_floating_selection();
    }

    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        _("Save Image"),
        GTK_WINDOW(parent),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Save"), GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    GtkFileFilter* filter_png = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_png, _("PNG Images"));
    gtk_file_filter_add_pattern(filter_png, "*.png");
    gtk_file_filter_add_pattern(filter_png, "*.PNG");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_png);

    GtkFileFilter* filter_jpg = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_jpg, _("JPEG Images"));
    gtk_file_filter_add_pattern(filter_jpg, "*.jpg");
    gtk_file_filter_add_pattern(filter_jpg, "*.jpeg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_jpg);

    GtkFileFilter* filter_xpm = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_xpm, _("XPM Images"));
    gtk_file_filter_add_pattern(filter_xpm, "*.xpm");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_xpm);

    if (!app_state.current_filename.empty()) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), app_state.current_filename.c_str());
    } else {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), _("untitled.png"));
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        if (filename) {
            std::string fname(filename);
            size_t dot_pos = fname.find_last_of('.');
            std::string extension = (dot_pos == std::string::npos) ? "" : fname.substr(dot_pos + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            if (extension != "png") {
                fname += ".png";
            }

            app_state.current_filename = fname;
            cairo_surface_t* composed = compose_visible_layers_surface();
            cairo_surface_write_to_png(composed, fname.c_str());
            cairo_surface_destroy(composed);

            g_free(filename);
        }
    }

    gtk_widget_destroy(dialog);
}

static std::string get_file_extension_lowercase(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos + 1 >= filename.size()) {
        return "";
    }

    std::string extension = filename.substr(dot_pos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

bool save_surface_to_file(cairo_surface_t* surface, const std::string& filename) {
    if (!surface || filename.empty()) {
        return false;
    }

    std::string extension = get_file_extension_lowercase(filename);
    if (extension == "jpg" || extension == "jpeg" || extension == "xpm") {
        cairo_surface_t* rgb_surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32,
            app_state.canvas_width,
            app_state.canvas_height
        );
        cairo_t* cr = cairo_create(rgb_surface);
        configure_crisp_rendering(cr);
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);

        std::string temp_png = filename + ".temp.png";
        cairo_surface_write_to_png(rgb_surface, temp_png.c_str());
        cairo_surface_destroy(rgb_surface);

        GError* error = NULL;
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(temp_png.c_str(), &error);
        bool save_success = false;
        if (pixbuf) {
            if (extension == "xpm") {
                save_success = gdk_pixbuf_save(pixbuf, filename.c_str(), "xpm", &error, NULL);
            } else {
                save_success = gdk_pixbuf_save(pixbuf, filename.c_str(), "jpeg", &error, "quality", "95", NULL);
            }
            g_object_unref(pixbuf);
        }
        remove(temp_png.c_str());
        return save_success;
    }

    return cairo_surface_write_to_png(surface, filename.c_str()) == CAIRO_STATUS_SUCCESS;
}

void open_image_dialog(GtkWidget* parent) {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        _("Open Image"),
        GTK_WINDOW(parent),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Open"), GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkFileFilter* filter_images = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_images, _("PNG Images"));
    gtk_file_filter_add_pattern(filter_images, "*.png");
    gtk_file_filter_add_pattern(filter_images, "*.PNG");
    gtk_file_filter_add_pattern(filter_images, "*.xpm");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_images);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        if (filename) {
            app_state.current_filename = filename;

            cairo_surface_t* loaded_surface = cairo_image_surface_create_from_png(filename);
            if (cairo_surface_status(loaded_surface) == CAIRO_STATUS_SUCCESS) {
                int width = cairo_image_surface_get_width(loaded_surface);
                int height = cairo_image_surface_get_height(loaded_surface);

                push_undo_state();

                app_state.canvas_width = width;
                app_state.canvas_height = height;

                clear_layers();
                Layer layer;
                layer.name = "Layer 1";
                layer.visible = true;
                layer.surface = loaded_surface;
                app_state.layers.push_back(layer);
                set_active_layer(0);
                rebuild_layer_panel();

                gtk_widget_set_size_request(app_state.drawing_area,
                    static_cast<int>(width * app_state.zoom_factor),
                    static_cast<int>(height * app_state.zoom_factor));
                gtk_widget_queue_draw(app_state.drawing_area);
            } else {
                cairo_surface_destroy(loaded_surface);
            }

            g_free(filename);
        }
    }

    gtk_widget_destroy(dialog);
}


// Menu callbacks
void on_file_new(GtkMenuItem* item, gpointer data) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("New Image"),
        GTK_WINDOW(app_state.window),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Create"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(container), 10);
    gtk_container_add(GTK_CONTAINER(content), container);

    GtkWidget* resolution_label = gtk_label_new(_("Resolution:"));
    gtk_widget_set_halign(resolution_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(container), resolution_label, FALSE, FALSE, 0);

    GtkWidget* resolution_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(resolution_combo), "256x256");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(resolution_combo), "512x512");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(resolution_combo), "1024x1024");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(resolution_combo), "640x480");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(resolution_combo), "800x600");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(resolution_combo), _("Custom"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(resolution_combo), 4);
    gtk_box_pack_start(GTK_BOX(container), resolution_combo, FALSE, FALSE, 0);

    GtkWidget* custom_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* x_label = gtk_label_new(_("X:"));
    GtkWidget* x_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    GtkWidget* separator_label = gtk_label_new("x");
    GtkWidget* y_label = gtk_label_new(_("Y:"));
    GtkWidget* y_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    GtkWidget* pixels_label = gtk_label_new(_("pixels"));

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(x_spin), 800);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(y_spin), 600);

    gtk_box_pack_start(GTK_BOX(custom_row), x_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(custom_row), x_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(custom_row), separator_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(custom_row), y_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(custom_row), y_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(custom_row), pixels_label, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(custom_row, FALSE);
    gtk_box_pack_start(GTK_BOX(container), custom_row, FALSE, FALSE, 0);

    g_signal_connect(resolution_combo, "changed", G_CALLBACK(+[] (GtkComboBox* combo, gpointer user_data) {
        GtkWidget* row = GTK_WIDGET(user_data);
        GtkWidget* x_input = GTK_WIDGET(g_object_get_data(G_OBJECT(row), "x-spin"));
        GtkWidget* y_input = GTK_WIDGET(g_object_get_data(G_OBJECT(row), "y-spin"));

        gchar* selected = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
        bool is_custom = selected && g_strcmp0(selected, _("Custom")) == 0;
        gtk_widget_set_sensitive(row, is_custom);

        if (!is_custom && selected) {
            int width = 0;
            int height = 0;
            if (sscanf(selected, "%dx%d", &width, &height) == 2) {
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(x_input), width);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(y_input), height);
            }
        }

        g_free(selected);
    }), custom_row);

    g_object_set_data(G_OBJECT(custom_row), "x-spin", x_spin);
    g_object_set_data(G_OBJECT(custom_row), "y-spin", y_spin);

    gtk_widget_show_all(dialog);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    int new_width = 800;
    int new_height = 600;
    gchar* selected = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(resolution_combo));
    if (selected && g_strcmp0(selected, _("Custom")) == 0) {
        new_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(x_spin));
        new_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(y_spin));
    } else if (selected) {
        if (sscanf(selected, "%dx%d", &new_width, &new_height) != 2) {
            new_width = 800;
            new_height = 600;
        }
    }
    g_free(selected);
    gtk_widget_destroy(dialog);

    if (app_state.surface) {
        push_undo_state();
    }

    app_state.canvas_width = new_width;
    app_state.canvas_height = new_height;
    gtk_widget_set_size_request(app_state.drawing_area, new_width, new_height);
    init_surface(app_state.drawing_area);
	rebuild_layer_panel();
    app_state.current_filename.clear();
    clear_selection();
    if (app_state.text_active) {
        cancel_text();
    }
    gtk_widget_queue_draw(app_state.drawing_area);
}

void on_file_open(GtkMenuItem* item, gpointer data) {
    open_image_dialog(app_state.window);
}

void on_file_save(GtkMenuItem* item, gpointer data) {
    if (!app_state.current_filename.empty()) {
        std::string filename = app_state.current_filename;
        size_t dot_pos = filename.find_last_of('.');
        std::string extension = (dot_pos == std::string::npos) ? "" : filename.substr(dot_pos + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        if (extension != "png") {
            filename += ".png";
            app_state.current_filename = filename;
        }
        cairo_surface_t* composed = compose_visible_layers_surface();
        save_surface_to_file(composed, app_state.current_filename);
        cairo_surface_destroy(composed);
    } else {
        save_image_dialog(app_state.window);
    }
}

void on_file_save_as(GtkMenuItem* item, gpointer data) {
    save_image_dialog(app_state.window);
}

void on_file_quit(GtkMenuItem* item, gpointer data) {
    gtk_main_quit();
}
