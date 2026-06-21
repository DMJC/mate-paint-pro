#include "ui_widgets.h"
#include "utils.h"
#include "text_tool.h"
#include "selection.h"

bool ask_regular_polygon_sides(GtkWidget* parent) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Polygon Button"),
        GTK_WINDOW(parent),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_OK"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* label = gtk_label_new(_("Choose number of sides (3-50):"));
    GtkWidget* spin = gtk_spin_button_new_with_range(3, 50, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), app_state.regular_polygon_sides);

    gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(content), spin, FALSE, FALSE, 6);
    gtk_widget_show_all(dialog);

    bool accepted = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK);
    if (accepted) {
        app_state.regular_polygon_sides = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
    }

    gtk_widget_destroy(dialog);
    return accepted;
}

bool ask_star_points(GtkWidget* parent) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Star Tool"),
        GTK_WINDOW(parent),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_OK"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* label = gtk_label_new(_("Choose number of star points (3-50):"));
    GtkWidget* spin = gtk_spin_button_new_with_range(3, 50, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), app_state.star_points);

    gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(content), spin, FALSE, FALSE, 6);
    gtk_widget_show_all(dialog);

    bool accepted = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK);
    if (accepted) {
        app_state.star_points = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
    }

    gtk_widget_destroy(dialog);
    return accepted;
}

// Tool button callback
void on_tool_clicked(GtkButton* button, gpointer data) {
    Tool new_tool = (Tool)GPOINTER_TO_INT(data);

    if (new_tool == TOOL_REGULAR_POLYGON && !ask_regular_polygon_sides(app_state.window)) {
        return;
    }
    if (new_tool == TOOL_STAR && !ask_star_points(app_state.window)) {
        return;
    }

    // Cancel text if switching away from text tool (don't finalize empty text)
    if (app_state.text_active && new_tool != TOOL_TEXT) {
        cancel_text();
    }

    // Clear or commit selection when switching tools
    if (new_tool != app_state.current_tool) {
        if (!tool_is_selection_tool(new_tool)) {
            if (app_state.floating_selection_active) {
                commit_floating_selection();
            } else {
                clear_selection();
            }
            if (!app_state.text_active) {
                stop_ant_animation();
            }
        }
    }

    app_state.current_tool = new_tool;
    if (tool_supports_line_thickness(new_tool)) {
        app_state.active_line_thickness_index = app_state.tool_line_thickness_indices[tool_to_index(new_tool)];
        app_state.line_width = line_thickness_options[app_state.active_line_thickness_index];
        update_line_thickness_buttons();
    }
    if (new_tool == TOOL_ZOOM) {
        update_zoom_buttons();
    }
    app_state.polygon_points.clear();
    app_state.polygon_finished = false;
    app_state.lasso_points.clear();
    app_state.lasso_polygon_mode = false;
    app_state.ellipse_center_mode = false;
    app_state.gradient_fill_first_point_set = false;
    app_state.gradient_fill_circular = false;
    app_state.curve_active = false;
    app_state.curve_has_end = false;
    app_state.curve_has_control = false;
    update_line_thickness_visibility();
    update_zoom_visibility();
}

gboolean on_line_thickness_button_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    if (index < 0 || index >= (int)(sizeof(line_thickness_options) / sizeof(line_thickness_options[0]))) {
        return FALSE;
    }

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, line_thickness_options[index]);
    cairo_move_to(cr, 4, allocation.height / 2.0);
    cairo_line_to(cr, allocation.width - 4, allocation.height / 2.0);
    cairo_stroke(cr);

    return FALSE;
}

void update_line_thickness_buttons() {
    for (size_t i = 0; i < app_state.line_thickness_buttons.size(); ++i) {
        GtkWidget* button = app_state.line_thickness_buttons[i];
        bool is_active = ((int)i == app_state.active_line_thickness_index);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), is_active);
    }
}

void on_line_thickness_toggled(GtkToggleButton* button, gpointer data) {
    int index = GPOINTER_TO_INT(data);

    if (!gtk_toggle_button_get_active(button)) {
        if (app_state.active_line_thickness_index == index) {
            gtk_toggle_button_set_active(button, TRUE);
        }
        return;
    }

    app_state.active_line_thickness_index = index;
    app_state.line_width = line_thickness_options[index];
    if (tool_supports_line_thickness(app_state.current_tool)) {
        app_state.tool_line_thickness_indices[tool_to_index(app_state.current_tool)] = index;
    }

    for (size_t i = 0; i < app_state.line_thickness_buttons.size(); ++i) {
        if ((int)i != index) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app_state.line_thickness_buttons[i]), FALSE);
        }
    }
}

GtkWidget* create_line_thickness_button(int index) {
    GtkWidget* button = gtk_toggle_button_new();
    gtk_widget_set_size_request(button, 66, 20);
    gtk_widget_set_tooltip_text(button, _("Line thickness"));
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "togglebutton {"
        "background: #ffffff;"
        "background-image: none;"
        "border: 1px solid #888;"
        "border-radius: 0;"
        "padding: 0;"
        "box-shadow: none;"
        "}"
        "togglebutton:checked {"
        "background: #ffffff;"
        "background-image: none;"
        "border: 1px solid #333;"
        "}"
        "togglebutton:hover {"
        "background: #ffffff;"
        "background-image: none;"
        "}",
        -1,
        NULL
    );
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(button),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    g_object_unref(provider);

    GtkWidget* preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(preview, 58, 16);
    GtkCssProvider* preview_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(preview_provider,
        "drawingarea {"
        "background: #ffffff;"
        "background-image: none;"
        "}",
        -1,
        NULL
    );
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(preview),
        GTK_STYLE_PROVIDER(preview_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    g_object_unref(preview_provider);

    g_signal_connect(preview, "draw", G_CALLBACK(on_line_thickness_button_draw), GINT_TO_POINTER(index));
    gtk_container_add(GTK_CONTAINER(button), preview);

    g_signal_connect(button, "toggled", G_CALLBACK(on_line_thickness_toggled), GINT_TO_POINTER(index));

    return button;
}

void update_line_thickness_visibility() {
    if (!app_state.line_thickness_box) {
        return;
    }

    if (tool_supports_line_thickness(app_state.current_tool)) {
        gtk_widget_show_all(app_state.line_thickness_box);
    } else {
        gtk_widget_hide(app_state.line_thickness_box);
    }
}

void update_zoom_buttons() {
    for (size_t i = 0; i < app_state.zoom_buttons.size(); ++i) {
        GtkWidget* button = app_state.zoom_buttons[i];
        bool is_active = ((int)i == app_state.active_zoom_index);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), is_active);
    }
}

void on_zoom_toggled(GtkToggleButton* button, gpointer data) {
    int index = GPOINTER_TO_INT(data);

    if (!gtk_toggle_button_get_active(button)) {
        if (app_state.active_zoom_index == index) {
            gtk_toggle_button_set_active(button, TRUE);
        }
        return;
    }

    app_state.active_zoom_index = index;
    for (size_t i = 0; i < app_state.zoom_buttons.size(); ++i) {
        if ((int)i != index) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app_state.zoom_buttons[i]), FALSE);
        }
    }
}

GtkWidget* create_zoom_button(int index) {
    gchar* zoom_label = g_strdup_printf("%dx", (int)zoom_options[index]);
    GtkWidget* button = gtk_toggle_button_new_with_label(zoom_label);
    g_free(zoom_label);
    gtk_widget_set_size_request(button, 66, 20);
    gtk_widget_set_tooltip_text(button, _("Zoom level"));
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "togglebutton {"
        "background: #ffffff;"
        "background-image: none;"
        "border: 1px solid #888;"
        "border-radius: 0;"
        "padding: 0;"
        "box-shadow: none;"
        "}"
        "togglebutton:checked {"
        "background: #ffffff;"
        "background-image: none;"
        "border: 1px solid #333;"
        "}"
        "togglebutton:hover {"
        "background: #ffffff;"
        "background-image: none;"
        "}",
        -1,
        NULL
    );
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(button),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    g_object_unref(provider);

    g_signal_connect(button, "toggled", G_CALLBACK(on_zoom_toggled), GINT_TO_POINTER(index));
    return button;
}

void update_zoom_visibility() {
    if (!app_state.zoom_box) {
        return;
    }

    if (app_state.current_tool == TOOL_ZOOM) {
        gtk_widget_show_all(app_state.zoom_box);
    } else {
        gtk_widget_hide(app_state.zoom_box);
    }
}

const char* get_tool_icon_filename(Tool tool) {
    switch (tool) {
        case TOOL_LASSO_SELECT: return "stock-tool-free-select.png";
        case TOOL_RECT_SELECT: return "stock-tool-rect-select.png";
        case TOOL_SELECT_BY_COLOR: return "stock-tool-color-select.png";
        case TOOL_FUZZY_SELECT: return "stock-tool-fuzzy-select.png";
        case TOOL_ERASER: return "stock-tool-eraser.png";
        case TOOL_FILL: return "stock-tool-bucket-fill.png";
        case TOOL_GRADIENT_FILL: return "stock-tool-gradient-fill.png";
        case TOOL_SMUDGE: return "stock-tool-smudge.png";
        case TOOL_EYEDROPPER: return "stock-tool-color-picker.png";
        case TOOL_ZOOM: return "stock-tool-zoom.png";
        case TOOL_PENCIL: return "stock-tool-pencil.png";
        case TOOL_PAINTBRUSH: return "stock-tool-paintbrush.png";
        case TOOL_AIRBRUSH: return "stock-tool-airbrush.png";
        case TOOL_TEXT: return "stock-tool-text.png";
        case TOOL_LINE: return "stock_draw-line.png";
        case TOOL_CURVE: return "stock_draw-curve.png";
        case TOOL_RECTANGLE: return "stock_draw-rectangle.png";
        case TOOL_POLYGON: return "stock_draw-fill_polygon.png";
        case TOOL_CROP: return "stock-tool-crop.png";
        case TOOL_ELLIPSE: return "stock_draw-ellipse.png";
        case TOOL_REGULAR_POLYGON: return "stock_draw-pentagon.png";
        case TOOL_STAR: return "stock_draw-star.png";
        case TOOL_ROUNDED_RECT: return "stock_draw-rounded-rectangle.png";
        default: return NULL;
    }
}

GtkWidget* create_tool_icon(Tool tool) {
    const char* icon_file = get_tool_icon_filename(tool);
    if (!icon_file) {
        return gtk_image_new();
    }

    const char* icon_roots[] = {
        "/usr/share/mate-paint-pro",
        "."
    };

    for (gsize i = 0; i < G_N_ELEMENTS(icon_roots); i++) {
        gchar* icon_path = g_build_filename(icon_roots[i], "data", "icons", "16x16", "actions", icon_file, NULL);
        if (g_file_test(icon_path, G_FILE_TEST_EXISTS)) {
            GtkWidget* icon = gtk_image_new_from_file(icon_path);
            g_free(icon_path);
            return icon;
        }
        g_free(icon_path);
    }

    return gtk_image_new();
}

// Create tool button with tooltip
GtkWidget* create_tool_button(Tool tool, const char* tooltip) {
    GtkWidget* button = gtk_button_new();
    gtk_widget_set_size_request(button, 28, 28);

    GtkWidget* icon = create_tool_icon(tool);
    gtk_button_set_image(GTK_BUTTON(button), icon);

    // Set tooltip
    gtk_widget_set_tooltip_text(button, tooltip);

    g_signal_connect(button, "clicked", G_CALLBACK(on_tool_clicked), GINT_TO_POINTER(tool));

    return button;
}
