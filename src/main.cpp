#include "app_state.h"
#include "utils.h"
#include "palette.h"
#include "ui_widgets.h"
#include "layers.h"
#include "selection.h"
#include "events.h"
#include "rendering.h"
#include "menus.h"

AppState app_state;

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    gtk_init(&argc, &argv);

    app_state.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app_state.window), _("Mate-Paint Pro"));
    gtk_window_set_default_size(GTK_WINDOW(app_state.window), 900, 700);
    g_signal_connect(app_state.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app_state.window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(app_state.window, "key-release-event", G_CALLBACK(on_key_release), NULL);

    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app_state.window), main_box);

    GtkWidget* menubar = build_menubar();
    gtk_box_pack_start(GTK_BOX(main_box), menubar, FALSE, FALSE, 0);

    GtkWidget* content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(main_box), content_box, TRUE, TRUE, 0);

    GtkWidget* tool_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(tool_column, 5);
    gtk_widget_set_margin_end(tool_column, 5);
    gtk_widget_set_margin_top(tool_column, 5);

    GtkWidget* toolbox = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(toolbox), 2);
    gtk_grid_set_row_spacing(GTK_GRID(toolbox), 2);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_LASSO_SELECT,
            _("Lasso Select - Draw freehand selection")),
        0, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_RECT_SELECT,
            _("Rectangle Select - Select rectangular regions (Ctrl+C to copy, Ctrl+X to cut)")),
        1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_SELECT_BY_COLOR,
            _("Select by Colour - Select all matching pixels in the image")),
        0, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_FUZZY_SELECT,
            _("Fuzzy Select - Select connected matching pixels around the click point")),
        1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_FILL,
            _("Fill Tool - Fill areas with color")),
        0, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_GRADIENT_FILL,
            _("Gradient Fill - Click start and end points (hold Ctrl for circular gradient)")),
        0, 3, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_SMUDGE,
            _("Smudge Tool - Drag to smear existing pixels")),
        1, 3, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_EYEDROPPER,
            _("Eyedropper - Pick color from canvas")),
        1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_ERASER,
            _("Eraser - Erase to transparency")),
        0, 4, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_ZOOM,
            _("Zoom Tool - Zoom in/out")),
        1, 6, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_PENCIL,
            _("Pencil - Draw thin lines")),
        0, 5, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_PAINTBRUSH,
            _("Paintbrush - Draw with brush strokes")),
        1, 4, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_AIRBRUSH,
            _("Airbrush - Spray paint effect")),
        0, 6, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_TEXT,
            _("Text Tool - Add text (Left-click outside to finalize, Right-click outside to cancel)")),
        1, 5, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_LINE,
            _("Line Tool - Draw straight lines (hold Shift for horizontal/vertical)")),
        0, 7, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_CURVE,
            _("Curve Tool - Draw curved lines")),
        1, 7, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_RECTANGLE,
            _("Rectangle - Draw rectangles")),
        0, 8, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_POLYGON,
            _("Polygon - Draw multi-sided shapes")),
        1, 8, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_ELLIPSE,
            _("Ellipse/Circle - Draw ellipses (hold Shift for circles)")),
        0, 9, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_ROUNDED_RECT,
            _("Rounded Rectangle - Draw rectangles with rounded corners")),
        1, 9, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_REGULAR_POLYGON,
            _("Polygon Button - Draw regular polygons (asks for 3-50 sides, hold Ctrl for center, Shift for uniform)")),
        0, 10, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_STAR,
            _("Star - Draw stars (asks for 3-50 points, hold Ctrl for center, Shift for uniform)")),
        1, 10, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_CROP,
            _("Crop - Draw a crop rectangle (hold Shift for square, click inside to crop, outside to deselect)")),
        1, 11, 1, 1);
    gtk_box_pack_start(GTK_BOX(tool_column), toolbox, FALSE, FALSE, 0);

    app_state.line_thickness_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_bottom(app_state.line_thickness_box, 5);
    for (int i = 0; i < (int)(sizeof(line_thickness_options) / sizeof(line_thickness_options[0])); ++i) {
        GtkWidget* thickness_button = create_line_thickness_button(i);
        app_state.line_thickness_buttons.push_back(thickness_button);
        gtk_box_pack_start(GTK_BOX(app_state.line_thickness_box), thickness_button, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(tool_column), app_state.line_thickness_box, FALSE, FALSE, 0);
    app_state.active_line_thickness_index = app_state.tool_line_thickness_indices[tool_to_index(app_state.current_tool)];
    app_state.line_width = line_thickness_options[app_state.active_line_thickness_index];
    update_line_thickness_buttons();
    update_line_thickness_visibility();
    app_state.zoom_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_bottom(app_state.zoom_box, 5);
    for (int i = 0; i < (int)(sizeof(zoom_options) / sizeof(zoom_options[0])); ++i) {
        GtkWidget* zoom_button = create_zoom_button(i);
        app_state.zoom_buttons.push_back(zoom_button);
        gtk_box_pack_start(GTK_BOX(app_state.zoom_box), zoom_button, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(tool_column), app_state.zoom_box, FALSE, FALSE, 0);

    update_zoom_buttons();
    update_zoom_visibility();

    gtk_box_pack_start(GTK_BOX(content_box), tool_column, FALSE, FALSE, 0);

    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    app_state.scrolled_window = scrolled;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    app_state.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(app_state.drawing_area,
        static_cast<int>(app_state.canvas_width * app_state.zoom_factor),
        static_cast<int>(app_state.canvas_height * app_state.zoom_factor));

    g_signal_connect(app_state.drawing_area, "draw", G_CALLBACK(on_draw), NULL);
    g_signal_connect(app_state.drawing_area, "button-press-event", G_CALLBACK(on_button_press), NULL);
    g_signal_connect(app_state.drawing_area, "motion-notify-event", G_CALLBACK(on_motion_notify), NULL);
    g_signal_connect(app_state.drawing_area, "leave-notify-event", G_CALLBACK(on_leave_notify), NULL);
    g_signal_connect(app_state.drawing_area, "button-release-event", G_CALLBACK(on_button_release), NULL);

    gtk_widget_set_events(app_state.drawing_area,
        GDK_BUTTON_PRESS_MASK |
        GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK |
        GDK_LEAVE_NOTIFY_MASK
    );

    gtk_container_add(GTK_CONTAINER(scrolled), app_state.drawing_area);
    gtk_box_pack_start(GTK_BOX(content_box), scrolled, TRUE, TRUE, 0);

    app_state.layer_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(app_state.layer_panel, 6);
    gtk_widget_set_margin_end(app_state.layer_panel, 6);
    gtk_widget_set_margin_top(app_state.layer_panel, 6);
    GtkWidget* layer_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* layers_label = gtk_label_new(_("Layers"));
    app_state.layer_move_up_button = gtk_button_new_with_label("+");
    app_state.layer_move_down_button = gtk_button_new_with_label("-");
    g_signal_connect(app_state.layer_move_up_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
        move_layer_up(app_state.active_layer_index);
    }), NULL);
    g_signal_connect(app_state.layer_move_down_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
        move_layer_down(app_state.active_layer_index);
    }), NULL);
    gtk_box_pack_start(GTK_BOX(layer_header), layers_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(layer_header), app_state.layer_move_up_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(layer_header), app_state.layer_move_down_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app_state.layer_panel), layer_header, FALSE, FALSE, 0);

    app_state.layer_opacity_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(app_state.layer_opacity_scale), FALSE);
    gtk_range_set_value(GTK_RANGE(app_state.layer_opacity_scale), 1.0);
    gtk_widget_set_tooltip_text(app_state.layer_opacity_scale, _("Layer opacity"));
    g_signal_connect(app_state.layer_opacity_scale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer data) {
        if (app_state.layers.empty()) return;
        app_state.layers[app_state.active_layer_index].opacity = gtk_range_get_value(range);
        if (app_state.drawing_area) gtk_widget_queue_draw(app_state.drawing_area);
    }), NULL);
    gtk_box_pack_start(GTK_BOX(app_state.layer_panel), app_state.layer_opacity_scale, FALSE, FALSE, 0);

    app_state.layer_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(app_state.layer_panel), app_state.layer_list_box, FALSE, FALSE, 0);
    GtkWidget* layer_action_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    app_state.add_layer_button = gtk_button_new_with_label(_("+ Add a Layer"));
    g_signal_connect(app_state.add_layer_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
        add_new_layer();
    }), NULL);
    app_state.duplicate_layer_button = gtk_button_new_with_label(_("Duplicate Layer"));
    g_signal_connect(app_state.duplicate_layer_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
        duplicate_active_layer();
    }), NULL);
    app_state.merge_layer_button = gtk_button_new_with_label(_("Merge Down"));
    gtk_widget_set_tooltip_text(app_state.merge_layer_button, _("Merge the selected layer into the layer below"));
    g_signal_connect(app_state.merge_layer_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
        merge_layer_down(app_state.active_layer_index);
    }), NULL);
    gtk_box_pack_start(GTK_BOX(layer_action_row), app_state.add_layer_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(layer_action_row), app_state.duplicate_layer_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(layer_action_row), app_state.merge_layer_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(app_state.layer_panel), layer_action_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box), app_state.layer_panel, FALSE, FALSE, 0);

    GtkWidget* bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(bottom_box, 5);
    gtk_widget_set_margin_end(bottom_box, 5);
    gtk_widget_set_margin_bottom(bottom_box, 5);

    app_state.fg_button = gtk_drawing_area_new();
    gtk_widget_set_size_request(app_state.fg_button, 36, 36);
    gtk_widget_set_tooltip_text(app_state.fg_button, _("Foreground color (left-click palette / left-click canvas)"));
    g_signal_connect(app_state.fg_button, "draw", G_CALLBACK(on_color_button_draw), GINT_TO_POINTER(1));

    app_state.bg_button = gtk_drawing_area_new();
    gtk_widget_set_size_request(app_state.bg_button, 36, 36);
    gtk_widget_set_tooltip_text(app_state.bg_button, _("Background color (right-click palette / right-click canvas)"));
    g_signal_connect(app_state.bg_button, "draw", G_CALLBACK(on_color_button_draw), GINT_TO_POINTER(0));

    gtk_box_pack_start(GTK_BOX(bottom_box), app_state.fg_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bottom_box), app_state.bg_button, FALSE, FALSE, 0);

    GtkWidget* palette_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(palette_grid), 2);
    gtk_grid_set_row_spacing(GTK_GRID(palette_grid), 2);

    app_state.palette_button_colors.assign(std::begin(palette_colors), std::end(palette_colors));
    app_state.palette_button_colors.insert(
        app_state.palette_button_colors.end(),
        std::begin(additional_palette_colors),
        std::end(additional_palette_colors)
    );

    app_state.custom_palette_slots.assign(app_state.palette_button_colors.size(), false);
    const int custom_start_index = (int)app_state.palette_button_colors.size() - custom_palette_slot_count;
    for (int i = custom_start_index; i < (int)app_state.custom_palette_slots.size(); i++) {
        app_state.custom_palette_slots[i] = true;
    }

    load_custom_palette_colors();

    app_state.palette_buttons.clear();
    app_state.palette_buttons.reserve(app_state.palette_button_colors.size());

    int colors_per_row = 14;
    for (size_t i = 0; i < app_state.palette_button_colors.size(); i++) {
        bool is_custom_slot = app_state.custom_palette_slots[i];
        GtkWidget* color_btn = create_color_button(app_state.palette_button_colors[i], i, is_custom_slot);
        if (is_custom_slot) {
            gtk_widget_set_tooltip_text(color_btn, _("Double-click to choose a custom colour"));
        }

        app_state.palette_buttons.push_back(color_btn);

        int row = i / colors_per_row;
        int col = i % colors_per_row;
        gtk_grid_attach(GTK_GRID(palette_grid), color_btn, col, row, 1, 1);
    }

    gtk_box_pack_start(GTK_BOX(bottom_box), palette_grid, FALSE, FALSE, 10);

    GtkWidget* status_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_halign(status_box, GTK_ALIGN_END);

    app_state.canvas_dimensions_label = gtk_label_new("800x600");
    gtk_widget_set_halign(app_state.canvas_dimensions_label, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(status_box), app_state.canvas_dimensions_label, FALSE, FALSE, 0);

    app_state.cursor_position_label = gtk_label_new("-");
    gtk_widget_set_halign(app_state.cursor_position_label, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(status_box), app_state.cursor_position_label, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(bottom_box), status_box, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(main_box), bottom_box, FALSE, FALSE, 0);

    init_surface(app_state.drawing_area);

    rebuild_layer_panel();

    start_ant_animation();

    gtk_widget_show_all(app_state.window);
    update_line_thickness_visibility();
    update_zoom_visibility();
    gtk_main();

    stop_ant_animation();
    clear_layers();
    if (app_state.clipboard_surface) {
        cairo_surface_destroy(app_state.clipboard_surface);
    }
    if (app_state.floating_surface) {
        cairo_surface_destroy(app_state.floating_surface);
    }

    save_custom_palette_colors();

    return 0;
}
