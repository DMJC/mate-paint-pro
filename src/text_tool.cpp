#include "text_tool.h"
#include "utils.h"
#include "undo.h"
#include "selection.h"

// Check if point is inside text box
bool point_in_text_box(double x, double y) {
    if (!app_state.text_active) return false;
    return x >= app_state.text_x && x <= app_state.text_x + app_state.text_box_width &&
           y >= app_state.text_y && y <= app_state.text_y + app_state.text_box_height;
}

// Calculate required text box size based on content and font
void update_text_box_size() {
    if (!app_state.text_active) return;

    // Create a temporary cairo surface for measurements
    cairo_surface_t* temp_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t* cr = cairo_create(temp_surface);
    configure_crisp_rendering(cr);

    // Set font
    cairo_select_font_face(cr, app_state.text_font_family.c_str(),
                          CAIRO_FONT_SLANT_NORMAL,
                          CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, app_state.text_font_size);

    // Calculate size needed for text
    const double min_width = 200.0;
    const double width_padding = 20.0;
    const double wrap_padding = 10.0;
    const double max_canvas_width = fmax(20.0, app_state.canvas_width - app_state.text_x);
    double target_width = fmin(min_width, max_canvas_width);
    double total_height = app_state.text_font_size + 10;

    if (!app_state.text_content.empty()) {
        const std::string& text = app_state.text_content;
        const bool has_manual_line_break = text.find('\n') != std::string::npos;
        cairo_text_extents_t extents;


        // Grow width with the current line while typing until we hit the canvas edge.
        if (!has_manual_line_break) {
            cairo_text_extents(cr, text.c_str(), &extents);
            double content_width = extents.width + width_padding;
            target_width = fmin(fmax(min_width, content_width), max_canvas_width);
        } else {
            // Once Enter is used, keep current width and wrap without expanding further right.
            target_width = fmin(fmax(app_state.text_box_width, min_width), max_canvas_width);
        }

        // Measure wrapped line count based on the chosen width.
        std::string word;
        std::string line;
        int line_count = 1;

        for (size_t i = 0; i <= text.length(); i++) {
            if (i == text.length() || text[i] == ' ' || text[i] == '\n') {
                if (!word.empty()) {
                    std::string test_line = line.empty() ? word : line + " " + word;
                    cairo_text_extents(cr, test_line.c_str(), &extents);

                    if (extents.width > target_width - wrap_padding && !line.empty()) {
                    } else {
                        line = test_line;
                    }
                    word.clear();
                }

                if (i < text.length() && text[i] == '\n') {
                    line.clear();
                    line_count++;
                }
            } else {
                word += text[i];
            }
        }
        total_height = line_count * (app_state.text_font_size + 2) + 15;
    } else {
        // Empty text, use minimum size based on font
        total_height = app_state.text_font_size * 3 + 20;
    }
    cairo_destroy(cr);
    cairo_surface_destroy(temp_surface);

    // Update text box dimensions
    app_state.text_box_width = target_width;
    app_state.text_box_height = fmax(total_height, app_state.text_font_size * 2 + 20);

    // Make sure box doesn't go off canvas
    if (app_state.text_x + app_state.text_box_width > app_state.canvas_width) {
        app_state.text_box_width = app_state.canvas_width - app_state.text_x;
    }
    if (app_state.text_y + app_state.text_box_height > app_state.canvas_height) {
        app_state.text_box_height = app_state.canvas_height - app_state.text_y;
    }
}

// Cancel text without rendering
void cancel_text() {
    app_state.text_active = false;
    app_state.text_content.clear();

    // Destroy text window if it exists
    if (app_state.text_window) {
        gtk_widget_destroy(app_state.text_window);
        app_state.text_window = nullptr;
        app_state.text_entry = nullptr;
    }

    // Stop animation if no selection is active
    if (!app_state.has_selection) {
        stop_ant_animation();
    }

    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

// Finalize text onto canvas
void finalize_text() {
    if (!app_state.text_active || app_state.text_content.empty() || !app_state.surface) {
        // If text is active but empty, just cancel it
        if (app_state.text_active) {
            cancel_text();
        }
        return;
    }

    push_undo_state();

    cairo_t* cr = cairo_create(app_state.surface);
    configure_crisp_rendering(cr);

    // Set font
    cairo_select_font_face(cr, app_state.text_font_family.c_str(),
                          CAIRO_FONT_SLANT_NORMAL,
                          CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, app_state.text_font_size);

    // Set color
    cairo_set_source_rgba(cr,
        app_state.fg_color.red,
        app_state.fg_color.green,
        app_state.fg_color.blue,
        app_state.fg_color.alpha
    );

    // Draw text with word wrapping
    std::string text = app_state.text_content;
    double y = app_state.text_y + app_state.text_font_size + 5;
    double x = app_state.text_x + 5;

    cairo_text_extents_t extents;
    std::string word;
    std::string line;

    for (size_t i = 0; i <= text.length(); i++) {
        if (i == text.length() || text[i] == ' ' || text[i] == '\n') {
            if (!word.empty()) {
                std::string test_line = line.empty() ? word : line + " " + word;
                cairo_text_extents(cr, test_line.c_str(), &extents);

                if (extents.width > app_state.text_box_width - 10) {
                    if (!line.empty()) {
                        cairo_move_to(cr, x, y);
                        cairo_show_text(cr, line.c_str());
                        y += app_state.text_font_size + 2;
                        line = word;
                    } else {
                        cairo_move_to(cr, x, y);
                        cairo_show_text(cr, word.c_str());
                        y += app_state.text_font_size + 2;
                        line.clear();
                    }
                } else {
                    line = test_line;
                }
                word.clear();
            }

            if (i < text.length() && text[i] == '\n') {
                if (!line.empty()) {
                    cairo_move_to(cr, x, y);
                    cairo_show_text(cr, line.c_str());
                    y += app_state.text_font_size + 2;
                    line.clear();
                }
            }
        } else {
            word += text[i];
        }
    }

    if (!line.empty()) {
        cairo_move_to(cr, x, y);
        cairo_show_text(cr, line.c_str());
    }

    cairo_destroy(cr);

    // Clear text state
    app_state.text_active = false;
    app_state.text_content.clear();

    // Destroy text window if it exists
    if (app_state.text_window) {
        gtk_widget_destroy(app_state.text_window);
        app_state.text_window = nullptr;
        app_state.text_entry = nullptr;
    }

    // Stop animation if no selection is active
    if (!app_state.has_selection) {
        stop_ant_animation();
    }

    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

// Text entry changed callback
static void on_text_entry_changed(GtkTextBuffer* buffer, gpointer data) {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    app_state.text_content = text ? text : "";
    g_free(text);

    // Update text box size when content changes
    update_text_box_size();

    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

// Font selection callback
static void on_font_selected(GtkFontButton* button, gpointer data) {
    const gchar* font_name = gtk_font_button_get_font_name(button);
    PangoFontDescription* desc = pango_font_description_from_string(font_name);

    app_state.text_font_family = pango_font_description_get_family(desc);
    app_state.text_font_size = pango_font_description_get_size(desc) / PANGO_SCALE;

    pango_font_description_free(desc);

    // Update text box size when font changes
    update_text_box_size();

    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

// Create text input window
void create_text_window(double x, double y) {
    if (app_state.text_window) {
        gtk_widget_destroy(app_state.text_window);
    }

    app_state.text_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app_state.text_window), _("Text Tool"));
    gtk_window_set_default_size(GTK_WINDOW(app_state.text_window), 300, 200);
    gtk_window_set_transient_for(GTK_WINDOW(app_state.text_window), GTK_WINDOW(app_state.window));

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(app_state.text_window), vbox);

    // Font selector
    GtkWidget* font_button = gtk_font_button_new();
    gchar* font_str = g_strdup_printf("%s %d", app_state.text_font_family.c_str(), app_state.text_font_size);
    gtk_font_button_set_font_name(GTK_FONT_BUTTON(font_button), font_str);
    g_free(font_str);
    g_signal_connect(font_button, "font-set", G_CALLBACK(on_font_selected), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), font_button, FALSE, FALSE, 0);

    // Text view
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    GtkWidget* text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    app_state.text_entry = text_view;

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    if (!app_state.text_content.empty()) {
        gtk_text_buffer_set_text(buffer, app_state.text_content.c_str(), -1);
    }
    g_signal_connect(buffer, "changed", G_CALLBACK(on_text_entry_changed), NULL);

    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    gtk_widget_show_all(app_state.text_window);
    gtk_widget_grab_focus(text_view);
}
