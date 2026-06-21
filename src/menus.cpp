#include "menus.h"
#include "utils.h"
#include "selection.h"
#include "undo.h"
#include "layers.h"
#include "file_io.h"
#include "filters.h"
#include "image_ops.h"

void on_edit_copy(GtkMenuItem* item, gpointer data) {
    copy_selection();
}

void on_edit_cut(GtkMenuItem* item, gpointer data) {
    cut_selection();
}

void on_edit_paste(GtkMenuItem* item, gpointer data) {
    paste_selection();
}

void on_edit_undo(GtkMenuItem* item, gpointer data) {
    undo_last_operation();
}

void on_edit_redo(GtkMenuItem* item, gpointer data) {
    redo_last_operation();
}

void on_layer_add(GtkMenuItem* item, gpointer data) {
    add_new_layer();
}

void on_layer_delete(GtkMenuItem* item, gpointer data) {
    delete_layer(app_state.active_layer_index);
}

void on_view_vertical_center_toggled(GtkCheckMenuItem* item, gpointer data) {
    app_state.show_vertical_center_guide = gtk_check_menu_item_get_active(item);
    if (app_state.drawing_area) gtk_widget_queue_draw(app_state.drawing_area);
}

void on_view_horizontal_center_toggled(GtkCheckMenuItem* item, gpointer data) {
    app_state.show_horizontal_center_guide = gtk_check_menu_item_get_active(item);
    if (app_state.drawing_area) gtk_widget_queue_draw(app_state.drawing_area);
}

bool prompt_guides(bool vertical) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        vertical ? _("Vertical Guides") : _("Horizontal Guides"),
        GTK_WINDOW(app_state.window),
        GTK_DIALOG_MODAL,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_OK"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    GtkWidget* count_label = gtk_label_new(_("Number of lines:"));
    GtkWidget* spacing_label = gtk_label_new(_("Spacing (pixels):"));
    GtkWidget* offset_label = gtk_label_new(_("Offset (pixels):"));
    GtkWidget* count_spin = gtk_spin_button_new_with_range(1, 100, 1);
    GtkWidget* spacing_spin = gtk_spin_button_new_with_range(1, 1000, 1);
    GtkWidget* offset_spin = gtk_spin_button_new_with_range(0, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), 3);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spacing_spin), 50);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(offset_spin), 0);
    gtk_grid_attach(GTK_GRID(grid), count_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), count_spin, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spacing_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spacing_spin, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), offset_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), offset_spin, 1, 2, 1, 1);
    gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    bool applied = false;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        int count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin));
        int spacing = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spacing_spin));
        int offset = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(offset_spin));
        std::vector<double>& guides = vertical ? app_state.vertical_guides : app_state.horizontal_guides;
        guides.clear();
        for (int i = 0; i < count; ++i) {
            guides.push_back(offset + ((i + 1) * spacing));
        }
        if (app_state.drawing_area) gtk_widget_queue_draw(app_state.drawing_area);
        applied = true;
    }
    gtk_widget_destroy(dialog);
    return applied;
}

void on_view_vertical_guides_toggled(GtkCheckMenuItem* item, gpointer data) {
    std::vector<double>& guides = app_state.vertical_guides;
    if (gtk_check_menu_item_get_active(item)) {
        if (!prompt_guides(true)) {
            g_signal_handlers_block_by_func(item, (gpointer)on_view_vertical_guides_toggled, data);
            gtk_check_menu_item_set_active(item, FALSE);
            g_signal_handlers_unblock_by_func(item, (gpointer)on_view_vertical_guides_toggled, data);
        }
        return;
    }
    guides.clear();
    if (app_state.drawing_area) gtk_widget_queue_draw(app_state.drawing_area);
}

void on_view_horizontal_guides_toggled(GtkCheckMenuItem* item, gpointer data) {
    std::vector<double>& guides = app_state.horizontal_guides;
    if (gtk_check_menu_item_get_active(item)) {
        if (!prompt_guides(false)) {
            g_signal_handlers_block_by_func(item, (gpointer)on_view_horizontal_guides_toggled, data);
            gtk_check_menu_item_set_active(item, FALSE);
            g_signal_handlers_unblock_by_func(item, (gpointer)on_view_horizontal_guides_toggled, data);
        }
        return;
    }
    guides.clear();
    if (app_state.drawing_area) gtk_widget_queue_draw(app_state.drawing_area);
}

void on_help_manual(GtkMenuItem* item, gpointer data) {
    // Use help URI instead of filesystem path
    const char* uri = "help:mate-paint-pro/contents";
    gchar* command = g_strdup_printf("yelp %s &", uri);
    std::system(command);
    g_free(command);
}

void on_help_about(GtkMenuItem* item, gpointer data) {
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(app_state.window),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        _("Mate-Paint-Pro\nVersion 1.0\nCopyright © 2006 James Carthew")
    );
    gtk_window_set_title(GTK_WINDOW(dialog), _("About"));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

GtkWidget* build_menubar() {
    GtkWidget* menubar = gtk_menu_bar_new();

    // File menu
    GtkWidget* file_menu = gtk_menu_new();
    GtkWidget* file_menu_item = gtk_menu_item_new_with_label(_("File"));

    GtkWidget* file_new = gtk_menu_item_new_with_label(_("New"));
    GtkWidget* file_open = gtk_menu_item_new_with_label(_("Open..."));
    GtkWidget* file_save = gtk_menu_item_new_with_label(_("Save"));
    GtkWidget* file_save_as = gtk_menu_item_new_with_label(_("Save As..."));
    GtkWidget* file_quit = gtk_menu_item_new_with_label(_("Quit"));

    g_signal_connect(file_new, "activate", G_CALLBACK(on_file_new), NULL);
    g_signal_connect(file_open, "activate", G_CALLBACK(on_file_open), NULL);
    g_signal_connect(file_save, "activate", G_CALLBACK(on_file_save), NULL);
    g_signal_connect(file_save_as, "activate", G_CALLBACK(on_file_save_as), NULL);
    g_signal_connect(file_quit, "activate", G_CALLBACK(on_file_quit), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), file_new);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), file_open);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), file_save);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), file_save_as);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), file_quit);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_menu_item);

    // Edit menu
    GtkWidget* edit_menu = gtk_menu_new();
    GtkWidget* edit_menu_item = gtk_menu_item_new_with_label(_("Edit"));
    GtkWidget* edit_undo = gtk_menu_item_new_with_label(_("Undo"));
    GtkWidget* edit_redo = gtk_menu_item_new_with_label(_("Redo"));
    GtkWidget* edit_cut = gtk_menu_item_new_with_label(_("Cut"));
    GtkWidget* edit_copy = gtk_menu_item_new_with_label(_("Copy"));
    GtkWidget* edit_paste = gtk_menu_item_new_with_label(_("Paste"));

    g_signal_connect(edit_undo, "activate", G_CALLBACK(on_edit_undo), NULL);
    g_signal_connect(edit_redo, "activate", G_CALLBACK(on_edit_redo), NULL);
    g_signal_connect(edit_cut, "activate", G_CALLBACK(on_edit_cut), NULL);
    g_signal_connect(edit_copy, "activate", G_CALLBACK(on_edit_copy), NULL);
    g_signal_connect(edit_paste, "activate", G_CALLBACK(on_edit_paste), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), edit_undo);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), edit_redo);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), edit_cut);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), edit_copy);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), edit_paste);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_menu_item), edit_menu);

    // Layer menu
    GtkWidget* layer_menu = gtk_menu_new();
    GtkWidget* layer_menu_item = gtk_menu_item_new_with_label(_("Layer"));
    GtkWidget* layer_add = gtk_menu_item_new_with_label(_("New Layer"));
    GtkWidget* layer_delete = gtk_menu_item_new_with_label(_("Delete Layer"));
    GtkWidget* layer_color_balance = gtk_menu_item_new_with_label(_("Color Balance"));
    GtkWidget* layer_brightness_contrast = gtk_menu_item_new_with_label(_("Brightness/Contrast"));

    g_signal_connect(layer_add, "activate", G_CALLBACK(on_layer_add), NULL);
    g_signal_connect(layer_delete, "activate", G_CALLBACK(on_layer_delete), NULL);
    g_signal_connect(layer_color_balance, "activate", G_CALLBACK(on_layer_color_balance), NULL);
    g_signal_connect(layer_brightness_contrast, "activate", G_CALLBACK(on_layer_brightness_contrast), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), layer_add);
    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), layer_delete);
    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), layer_color_balance);
    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), layer_brightness_contrast);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(layer_menu_item), layer_menu);

    // Draw menu
    GtkWidget* draw_menu = gtk_menu_new();
    GtkWidget* draw_menu_item = gtk_menu_item_new_with_label(_("Draw"));
    GtkWidget* draw_horizontal_lines = gtk_menu_item_new_with_label(_("Draw Horizontal Lines"));
    GtkWidget* draw_vertical_lines = gtk_menu_item_new_with_label(_("Draw Vertical Lines"));
    GtkWidget* draw_grid = gtk_menu_item_new_with_label(_("Draw Grid"));

    g_signal_connect(draw_horizontal_lines, "activate", G_CALLBACK(on_layer_draw_horizontal_lines), NULL);
    g_signal_connect(draw_vertical_lines, "activate", G_CALLBACK(on_layer_draw_vertical_lines), NULL);
    g_signal_connect(draw_grid, "activate", G_CALLBACK(on_layer_draw_grid), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(draw_menu), draw_horizontal_lines);
    gtk_menu_shell_append(GTK_MENU_SHELL(draw_menu), draw_vertical_lines);
    gtk_menu_shell_append(GTK_MENU_SHELL(draw_menu), draw_grid);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(draw_menu_item), draw_menu);

    // View menu
    GtkWidget* view_menu = gtk_menu_new();
    GtkWidget* view_menu_item = gtk_menu_item_new_with_label(_("View"));
    GtkWidget* view_vertical_center = gtk_check_menu_item_new_with_label(_("View Vertical Center Guide"));
    GtkWidget* view_horizontal_center = gtk_check_menu_item_new_with_label(_("View Horizontal Center Guide"));
    GtkWidget* view_vertical_guides = gtk_check_menu_item_new_with_label(_("View Vertical Guides"));
    GtkWidget* view_horizontal_guides = gtk_check_menu_item_new_with_label(_("View Horizontal Guides"));

    g_signal_connect(view_vertical_center, "toggled", G_CALLBACK(on_view_vertical_center_toggled), NULL);
    g_signal_connect(view_horizontal_center, "toggled", G_CALLBACK(on_view_horizontal_center_toggled), NULL);
    g_signal_connect(view_vertical_guides, "toggled", G_CALLBACK(on_view_vertical_guides_toggled), NULL);
    g_signal_connect(view_horizontal_guides, "toggled", G_CALLBACK(on_view_horizontal_guides_toggled), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), view_vertical_center);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), view_horizontal_center);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), view_vertical_guides);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), view_horizontal_guides);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_menu_item), view_menu);

    // Image menu
    GtkWidget* image_menu = gtk_menu_new();
    GtkWidget* image_menu_item = gtk_menu_item_new_with_label(_("Image"));
    GtkWidget* image_scale = gtk_menu_item_new_with_label(_("Scale Image..."));
    GtkWidget* image_resize = gtk_menu_item_new_with_label(_("Resize Image..."));
    GtkWidget* image_rotate_clockwise = gtk_menu_item_new_with_label(_("Rotate Clockwise"));
    GtkWidget* image_rotate_counter_clockwise = gtk_menu_item_new_with_label(_("Rotate Counter-Clockwise"));
    GtkWidget* image_flip_vertical = gtk_menu_item_new_with_label(_("Flip Vertical"));
    GtkWidget* image_flip_horizontal = gtk_menu_item_new_with_label(_("Flip Horizontal"));

    g_signal_connect(image_scale, "activate", G_CALLBACK(on_image_scale), NULL);
    g_signal_connect(image_resize, "activate", G_CALLBACK(on_image_resize_canvas), NULL);
    g_signal_connect(image_rotate_clockwise, "activate", G_CALLBACK(on_image_rotate_clockwise), NULL);
    g_signal_connect(image_rotate_counter_clockwise, "activate", G_CALLBACK(on_image_rotate_counter_clockwise), NULL);
    g_signal_connect(image_flip_vertical, "activate", G_CALLBACK(on_image_flip_vertical), NULL);
    g_signal_connect(image_flip_horizontal, "activate", G_CALLBACK(on_image_flip_horizontal), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), image_scale);
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), image_resize);
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), image_rotate_clockwise);
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), image_rotate_counter_clockwise);
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), image_flip_vertical);
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), image_flip_horizontal);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(image_menu_item), image_menu);

    // Filters menu
    GtkWidget* filters_menu = gtk_menu_new();
    GtkWidget* filters_menu_item = gtk_menu_item_new_with_label(_("Filters"));
    GtkWidget* filters_noise = gtk_menu_item_new_with_label(_("Noise"));
    GtkWidget* filters_gaussian_blur = gtk_menu_item_new_with_label(_("Gaussian Blur"));
    GtkWidget* filters_blur = gtk_menu_item_new_with_label(_("Blur"));
    GtkWidget* filters_cartoonify = gtk_menu_item_new_with_label(_("Cartoonify"));
    GtkWidget* filters_sharpen = gtk_menu_item_new_with_label(_("Sharpen"));

    g_signal_connect(filters_noise, "activate", G_CALLBACK(on_filters_noise), NULL);
    g_signal_connect(filters_gaussian_blur, "activate", G_CALLBACK(on_filters_gaussian_blur), NULL);
    g_signal_connect(filters_blur, "activate", G_CALLBACK(on_filters_blur), NULL);
    g_signal_connect(filters_cartoonify, "activate", G_CALLBACK(on_filters_cartoonify), NULL);
    g_signal_connect(filters_sharpen, "activate", G_CALLBACK(on_filters_sharpen), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(filters_menu), filters_noise);
    gtk_menu_shell_append(GTK_MENU_SHELL(filters_menu), filters_gaussian_blur);
    gtk_menu_shell_append(GTK_MENU_SHELL(filters_menu), filters_blur);
    gtk_menu_shell_append(GTK_MENU_SHELL(filters_menu), filters_cartoonify);
    gtk_menu_shell_append(GTK_MENU_SHELL(filters_menu), filters_sharpen);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filters_menu_item), filters_menu);

    // Help menu
    GtkWidget* help_menu = gtk_menu_new();
    GtkWidget* help_menu_item = gtk_menu_item_new_with_label(_("Help"));
    GtkWidget* help_manual = gtk_menu_item_new_with_label(_("Contents"));
    GtkWidget* help_about = gtk_menu_item_new_with_label(_("About"));

    g_signal_connect(help_manual, "activate", G_CALLBACK(on_help_manual), NULL);
    g_signal_connect(help_about, "activate", G_CALLBACK(on_help_about), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), help_manual);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), help_about);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_menu_item), help_menu);

    // Append all top-level menu items to the menubar
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), layer_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), draw_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), image_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), filters_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_menu_item);

    return menubar;
}
