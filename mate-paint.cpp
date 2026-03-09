#include <gtk/gtk.h>
#include <cairo.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <memory>
#include <iterator>
#include <string>
#include <cctype>
#include <queue>
#include <cstdio>

struct Layer {
    cairo_surface_t* surface = nullptr;
    std::string name;
    bool visible = true;
    double opacity = 1.0;
    GtkWidget* visible_check = nullptr;
    GtkWidget* name_entry = nullptr;
    GtkWidget* select_button = nullptr;
};

const double line_thickness_options[] = {1.0, 2.0, 4.0, 6.0, 8.0};
const double zoom_options[] = {1.0, 2.0, 4.0, 6.0, 8.0};
// Tool types
enum Tool {
    TOOL_LASSO_SELECT,
    TOOL_RECT_SELECT,
    TOOL_ERASER,
    TOOL_FILL,
    TOOL_GRADIENT_FILL,
    TOOL_SMUDGE,
    TOOL_EYEDROPPER,
    TOOL_ZOOM,
    TOOL_PENCIL,
    TOOL_PAINTBRUSH,
    TOOL_AIRBRUSH,
    TOOL_TEXT,
    TOOL_LINE,
    TOOL_CURVE,
    TOOL_RECTANGLE,
    TOOL_POLYGON,
    TOOL_ELLIPSE,
    TOOL_REGULAR_POLYGON,
    TOOL_ROUNDED_RECT,
    TOOL_COUNT
};

// Forward declarations
void update_color_indicators();
void save_image_dialog(GtkWidget* parent);
void open_image_dialog(GtkWidget* parent);
void clear_selection();
void copy_selection();
void cut_selection();
void paste_selection();
void copy_surface_to_system_clipboard(cairo_surface_t* surface);
cairo_surface_t* get_surface_from_system_clipboard(int& width, int& height);
bool should_expand_canvas_for_paste(int pasted_width, int pasted_height);
void resize_canvas_for_paste(int new_width, int new_height);
void commit_floating_selection(bool record_undo = true);
void finalize_text();
void cancel_text();
void update_text_box_size();
void update_line_thickness_visibility();
void update_line_thickness_buttons();
int tool_to_index(Tool tool);
void update_zoom_buttons();
void update_zoom_visibility();
void update_canvas_dimensions_label();
void update_cursor_position_label(double canvas_x, double canvas_y, bool cursor_in_canvas);
void push_undo_state();
void undo_last_operation();
void redo_last_operation();
void draw_canvas_grid_background(cairo_t* cr, double width, double height);
static void draw_line_pattern_preview_overlay(cairo_t* cr);
bool is_transparent_color(const GdkRGBA& color);
bool save_surface_to_file(cairo_surface_t* surface, const std::string& filename);
void load_custom_palette_colors();
void save_custom_palette_colors();
void rebuild_layer_panel();
void set_active_layer(int index);
void add_new_layer();
void delete_layer(int index);
void move_layer_up(int index);
void move_layer_down(int index);
void merge_layer_down(int index);
void sync_layer_controls();
bool ask_regular_polygon_sides(GtkWidget* parent);
void build_regular_polygon_points(double start_x, double start_y, double end_x, double end_y, bool from_center, bool uniform, int sides, std::vector<std::pair<double, double>>& points);
void draw_regular_polygon(cairo_t* cr, const std::vector<std::pair<double, double>>& points);

struct UndoSnapshot {
    cairo_surface_t* surface = nullptr;
    int width = 0;
    int height = 0;
};

// Application state
struct AppState {
    Tool current_tool = TOOL_PENCIL;
    GdkRGBA fg_color = {0.0, 0.5, 0.0, 1.0}; // Green
    GdkRGBA bg_color = {1.0, 1.0, 1.0, 1.0}; // White
    cairo_surface_t* surface = nullptr;
    std::vector<Layer> layers;
    int active_layer_index = 0;
    int canvas_width = 800;
    int canvas_height = 600;
    double last_x = 0;
    double last_y = 0;
    bool is_drawing = false;
    bool is_right_button = false;
    bool shift_pressed = false;
    bool ctrl_pressed = false;
    double line_width = 2.0;
    
    // For shape tools and preview
    double start_x = 0;
    double start_y = 0;
    double current_x = 0;
    double current_y = 0;
    bool hover_in_canvas = false;
    double hover_x = 0;
    double hover_y = 0;
    std::vector<std::pair<double, double>> polygon_points;
    bool polygon_finished = false;
    std::vector<std::pair<double, double>> lasso_points;
    bool lasso_polygon_mode = false;
    bool ellipse_center_mode = false;
    bool gradient_fill_first_point_set = false;
    bool gradient_fill_circular = false;
    int regular_polygon_sides = 5;
    
    // Curve tool state
    bool curve_active = false;
    bool curve_has_end = false;
    bool curve_has_control = false;
    bool curve_primary_right_button = false;
    double curve_start_x = 0;
    double curve_start_y = 0;
    double curve_end_x = 0;
    double curve_end_y = 0;
    double curve_control_x = 0;
    double curve_control_y = 0;

    // Selection state
    bool has_selection = false;
    bool selection_is_rect = false;
    double selection_x1 = 0;
    double selection_y1 = 0;
    double selection_x2 = 0;
    double selection_y2 = 0;
    std::vector<std::pair<double, double>> selection_path;
    cairo_surface_t* floating_surface = nullptr;
    bool floating_selection_active = false;
    bool dragging_selection = false;
    bool floating_drag_completed = false;
    double selection_drag_offset_x = 0;
    double selection_drag_offset_y = 0;
    
    // Text tool state
    bool text_active = false;
    double text_x = 0;
    double text_y = 0;
    double text_box_width = 200;
    double text_box_height = 100;
    std::string text_content;
    std::string text_font_family = "Sans";
    int text_font_size = 14;
    GtkWidget* text_window = nullptr;
    GtkWidget* text_entry = nullptr;
    
    // Clipboard
    cairo_surface_t* clipboard_surface = nullptr;
    int clipboard_width = 0;
    int clipboard_height = 0;
    
    // Ant path animation
    double ant_offset = 0;
    guint ant_timer_id = 0;
    
    // UI elements
    GtkWidget* fg_button = nullptr;
    GtkWidget* bg_button = nullptr;
    GtkWidget* drawing_area = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* line_thickness_box = nullptr;
    std::vector<GtkWidget*> line_thickness_buttons;
    int active_line_thickness_index = 1;
    std::vector<int> tool_line_thickness_indices = std::vector<int>(TOOL_COUNT, 1);
    GtkWidget* zoom_box = nullptr;
    std::vector<GtkWidget*> zoom_buttons;
    int active_zoom_index = 0;
    double zoom_factor = 1.0;
    GtkWidget* scrolled_window = nullptr;
    GtkWidget* canvas_dimensions_label = nullptr;
    GtkWidget* cursor_position_label = nullptr;
    std::vector<GdkRGBA> palette_button_colors;
    std::vector<bool> custom_palette_slots;
    std::vector<GtkWidget*> palette_buttons;
    GtkWidget* layer_list_box = nullptr;
    GtkWidget* layer_panel = nullptr;
    GtkWidget* add_layer_button = nullptr;
    GtkWidget* merge_layer_button = nullptr;
    GtkWidget* layer_move_up_button = nullptr;
    GtkWidget* layer_move_down_button = nullptr;
    GtkWidget* layer_opacity_scale = nullptr;

    bool show_vertical_center_guide = false;
    bool show_horizontal_center_guide = false;
    std::vector<double> vertical_guides;
    std::vector<double> horizontal_guides;

    bool show_line_pattern_preview = false;
    bool preview_show_horizontal_lines = false;
    bool preview_show_vertical_lines = false;
    int preview_line_count = 0;
    int preview_horizontal_offset = 0;
    int preview_vertical_offset = 0;
    int preview_horizontal_spacing = 0;
    int preview_vertical_spacing = 0;

    std::string current_filename;

    std::vector<UndoSnapshot> undo_stack;
    std::vector<UndoSnapshot> redo_stack;
    static constexpr size_t max_undo_steps = 50;
    bool drag_undo_snapshot_taken = false;
};

AppState app_state;

// Color palette
const GdkRGBA palette_colors[] = {
    {0.0, 0.0, 0.0, 0.0},   // Transparency
    {0.0, 0.0, 0.0, 1.0},   // Black
    {0.2, 0.2, 0.2, 1.0},   // Dark gray
    {0.5, 0.5, 0.5, 1.0},   // Gray
    {0.5, 0.0, 0.0, 1.0},   // Dark red
    {0.8, 0.0, 0.0, 1.0},   // Red
    {1.0, 0.4, 0.0, 1.0},   // Orange
    {1.0, 0.8, 0.0, 1.0},   // Yellow-orange
    {1.0, 1.0, 0.0, 1.0},   // Yellow
    {0.8, 1.0, 0.0, 1.0},   // Yellow-green
    {0.0, 1.0, 0.0, 1.0},   // Bright green
    {0.0, 1.0, 0.5, 1.0},   // Cyan-green
    {0.0, 1.0, 1.0, 1.0},   // Cyan
    {0.0, 0.5, 1.0, 1.0},   // Light blue
    {0.0, 0.0, 1.0, 1.0},   // Blue
    {0.5, 0.0, 1.0, 1.0},   // Purple
    {0.8, 0.0, 0.8, 1.0},   // Magenta
    {1.0, 1.0, 1.0, 1.0},   // White
    {0.7, 0.7, 0.7, 1.0},   // Light gray
    {0.4, 0.2, 0.0, 1.0},   // Brown
    {1.0, 0.7, 0.7, 1.0},   // Light pink
    {1.0, 0.9, 0.7, 1.0},   // Cream
    {1.0, 1.0, 0.8, 1.0},   // Light yellow
    {0.8, 1.0, 0.8, 1.0},   // Light green
    {0.7, 0.9, 1.0, 1.0},   // Light cyan
    {0.7, 0.7, 1.0, 1.0},   // Light blue
    {0.9, 0.7, 1.0, 1.0},   // Light purple
};

const GdkRGBA additional_palette_colors[] = {
    {0.2, 0.0, 0.4, 1.0},   // Deep purple
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 1 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 2 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 3 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 4 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 5 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 6 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 7 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 8 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 9 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 10 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 11 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 12 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 13 default
    {0.6, 0.6, 0.6, 1.0},   // Custom slot 14 default
};

constexpr int custom_palette_slot_count = 14;

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

// Check if tool needs preview
bool tool_needs_preview(Tool tool) {
    return tool == TOOL_LASSO_SELECT || tool == TOOL_RECT_SELECT ||
           tool == TOOL_LINE || tool == TOOL_CURVE ||
           tool == TOOL_RECTANGLE || tool == TOOL_POLYGON ||
           tool == TOOL_ELLIPSE || tool == TOOL_REGULAR_POLYGON ||
           tool == TOOL_ROUNDED_RECT || tool == TOOL_GRADIENT_FILL;
}

int tool_to_index(Tool tool) {
    return static_cast<int>(tool);
}

bool tool_supports_line_thickness(Tool tool) {
    return tool == TOOL_PAINTBRUSH || tool == TOOL_AIRBRUSH || tool == TOOL_ERASER || tool == TOOL_SMUDGE ||
           tool == TOOL_LINE || tool == TOOL_CURVE || tool == TOOL_RECTANGLE ||
           tool == TOOL_POLYGON || tool == TOOL_ELLIPSE || tool == TOOL_REGULAR_POLYGON || tool == TOOL_ROUNDED_RECT;
}

bool tool_shows_brush_hover_outline(Tool tool) {
    return tool == TOOL_PAINTBRUSH || tool == TOOL_AIRBRUSH || tool == TOOL_ERASER || tool == TOOL_SMUDGE || tool == TOOL_ELLIPSE ||  tool == TOOL_LASSO_SELECT;
}

bool tool_shows_vertex_hover_markers(Tool tool) {
    return tool == TOOL_LINE || tool == TOOL_CURVE || tool == TOOL_POLYGON;
}

double to_canvas_coordinate(double coordinate) {
    return coordinate / app_state.zoom_factor;
}

double clamp_double(double value, double min_value, double max_value) {
    return fmax(min_value, fmin(value, max_value));
}

void update_canvas_dimensions_label() {
    if (!app_state.canvas_dimensions_label) {
        return;
    }

    gchar* dimensions_text = g_strdup_printf("%dx%d", app_state.canvas_width, app_state.canvas_height);
    gtk_label_set_text(GTK_LABEL(app_state.canvas_dimensions_label), dimensions_text);
    g_free(dimensions_text);
}

void update_cursor_position_label(double canvas_x, double canvas_y, bool cursor_in_canvas) {
    if (!app_state.cursor_position_label) {
        return;
    }

    if (!cursor_in_canvas) {
        gtk_label_set_text(GTK_LABEL(app_state.cursor_position_label), "-");
        return;
    }

    int x = static_cast<int>(std::lround(clamp_double(canvas_x, 0.0, app_state.canvas_width * 1.0)));
    int y = static_cast<int>(std::lround(clamp_double(canvas_y, 0.0, app_state.canvas_height * 1.0)));

    gchar* position_text = g_strdup_printf("%dx%d", x, y);
    gtk_label_set_text(GTK_LABEL(app_state.cursor_position_label), position_text);
    g_free(position_text);
}

void configure_crisp_rendering(cairo_t* cr) {
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
}

void apply_zoom(double zoom_factor, double focus_x, double focus_y) {
    if (!app_state.drawing_area) {
        return;
    }

    app_state.zoom_factor = zoom_factor;
    gtk_widget_set_size_request(
        app_state.drawing_area,
        static_cast<int>(app_state.canvas_width * app_state.zoom_factor),
        static_cast<int>(app_state.canvas_height * app_state.zoom_factor)
    );

    if (app_state.scrolled_window) {
        GtkAdjustment* hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app_state.scrolled_window));
        GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app_state.scrolled_window));

        double h_page = gtk_adjustment_get_page_size(hadj);
        double v_page = gtk_adjustment_get_page_size(vadj);

        double target_h = focus_x * app_state.zoom_factor - h_page / 2.0;
        double target_v = focus_y * app_state.zoom_factor - v_page / 2.0;

        double h_max = fmax(gtk_adjustment_get_lower(hadj),
            gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj));
        double v_max = fmax(gtk_adjustment_get_lower(vadj),
            gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));

        target_h = clamp_double(target_h, gtk_adjustment_get_lower(hadj), h_max);
        target_v = clamp_double(target_v, gtk_adjustment_get_lower(vadj), v_max);

        gtk_adjustment_set_value(hadj, target_h);
        gtk_adjustment_set_value(vadj, target_v);
    }

    gtk_widget_queue_draw(app_state.drawing_area);
}

void reset_zoom_to_default() {
    if (!app_state.drawing_area) {
        return;
    }

    apply_zoom(1.0, app_state.canvas_width / 2.0, app_state.canvas_height / 2.0);

    if (app_state.scrolled_window) {
        GtkAdjustment* hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(app_state.scrolled_window));
        GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app_state.scrolled_window));
        gtk_adjustment_set_value(hadj, gtk_adjustment_get_lower(hadj));
        gtk_adjustment_set_value(vadj, gtk_adjustment_get_lower(vadj));
    }
}

// Check if point is inside selection
bool point_in_selection(double x, double y) {
    if (!app_state.has_selection) return false;
    
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

// Stop ant path animation
void stop_ant_animation() {
    if (app_state.ant_timer_id != 0) {
        g_source_remove(app_state.ant_timer_id);
        app_state.ant_timer_id = 0;
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

struct SelectionPixelBounds {
    int x;
    int y;
    int width;
    int height;
};

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

    if (app_state.selection_is_rect) {
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
    if (app_state.selection_is_rect) {
        cairo_rectangle(cr, bounds.x, bounds.y, w, h);
    } else if (app_state.selection_path.size() > 2) {
        append_selection_path(cr);
    }
    cairo_fill(cr);
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

    if (app_state.selection_is_rect) {
        SelectionPixelBounds bounds = get_selection_pixel_bounds();

        push_undo_state();

        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
    } else if (app_state.selection_path.size() > 2) {
        append_selection_path(cr);
    }
    cairo_fill(cr);
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

// Constrain line to horizontal or vertical when shift is pressed
void constrain_line(double start_x, double start_y, double& end_x, double& end_y) {
    double dx = end_x - start_x;
    double dy = end_y - start_y;
    
    if (fabs(dx) > fabs(dy)) {
        end_y = start_y;
    } else {
        end_x = start_x;
    }
}

// Constrain ellipse to circle when shift is pressed
void constrain_to_circle(double start_x, double start_y, double& end_x, double& end_y) {
    double dx = end_x - start_x;
    double dy = end_y - start_y;
    double radius = fmax(fabs(dx), fabs(dy));
    
    end_x = start_x + (dx >= 0 ? radius : -radius);
    end_y = start_y + (dy >= 0 ? radius : -radius);
}

// Constrain rectangle bounds to a square when shift is pressed
void constrain_to_square(double start_x, double start_y, double& end_x, double& end_y) {
    double dx = end_x - start_x;
    double dy = end_y - start_y;
    double size = fmax(fabs(dx), fabs(dy));

    end_x = start_x + (dx >= 0 ? size : -size);
    end_y = start_y + (dy >= 0 ? size : -size);
}


void build_regular_polygon_points(double start_x, double start_y, double end_x, double end_y, bool from_center, bool uniform, int sides, std::vector<std::pair<double, double>>& points) {
    points.clear();

    if (sides < 3) {
        return;
    }

    double center_x = 0.0;
    double center_y = 0.0;
    double radius_x = 0.0;
    double radius_y = 0.0;

    if (from_center) {
        center_x = start_x;
        center_y = start_y;
        radius_x = fabs(end_x - start_x);
        radius_y = fabs(end_y - start_y);
    } else {
        center_x = (start_x + end_x) / 2.0;
        center_y = (start_y + end_y) / 2.0;
        radius_x = fabs(end_x - start_x) / 2.0;
        radius_y = fabs(end_y - start_y) / 2.0;
    }

    if (uniform) {
        double radius = fmax(radius_x, radius_y);
        radius_x = radius;
        radius_y = radius;
    }

    if (radius_x < 0.1 || radius_y < 0.1) {
        return;
    }

    const double start_angle = -M_PI / 2.0;
    const double step = (2.0 * M_PI) / static_cast<double>(sides);

    for (int i = 0; i < sides; ++i) {
        double angle = start_angle + step * static_cast<double>(i);
        points.push_back({
            center_x + cos(angle) * radius_x,
            center_y + sin(angle) * radius_y
        });
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

// Text entry changed callback
void on_text_entry_changed(GtkTextBuffer* buffer, gpointer data) {
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
void on_font_selected(GtkFontButton* button, gpointer data) {
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

cairo_surface_t* create_blank_surface(int width, int height, bool fill_white) {
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(surface);
    configure_crisp_rendering(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    if (fill_white) {
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
    } else {
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
    }
    cairo_paint(cr);
    cairo_destroy(cr);
    return surface;
}

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

cairo_surface_t* clone_surface(cairo_surface_t* source, int width, int height) {
    if (!source || width <= 0 || height <= 0) {
        return nullptr;
    }

    cairo_surface_t* copy = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(copy);
    cairo_set_source_surface(cr, source, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    return copy;
}

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

// Initialize drawing surface
void init_surface(GtkWidget* widget) {
    ensure_default_layers();
}

// Get active color based on mouse button
GdkRGBA get_active_color() {
    return app_state.is_right_button ? app_state.bg_color : app_state.fg_color;
}

bool is_transparent_color(const GdkRGBA& color) {
    return color.alpha <= 0.001;
}

bool point_in_canvas(int x, int y) {
    return x >= 0 && x < app_state.canvas_width && y >= 0 && y < app_state.canvas_height;
}

double clamp_color_channel(double channel) {
    return fmax(0.0, fmin(1.0, channel));
}

guint32 rgba_to_pixel(const GdkRGBA& color) {
    guint8 r = static_cast<guint8>(std::round(clamp_color_channel(color.red) * 255.0));
    guint8 g = static_cast<guint8>(std::round(clamp_color_channel(color.green) * 255.0));
    guint8 b = static_cast<guint8>(std::round(clamp_color_channel(color.blue) * 255.0));
    guint8 a = static_cast<guint8>(std::round(clamp_color_channel(color.alpha) * 255.0));
    return (static_cast<guint32>(a) << 24) |
           (static_cast<guint32>(r) << 16) |
           (static_cast<guint32>(g) << 8) |
            static_cast<guint32>(b);
}

GdkRGBA pixel_to_rgba(guint32 pixel) {
    GdkRGBA color;
    color.alpha = ((pixel >> 24) & 0xFF) / 255.0;
    color.red = ((pixel >> 16) & 0xFF) / 255.0;
    color.green = ((pixel >> 8) & 0xFF) / 255.0;
    color.blue = (pixel & 0xFF) / 255.0;
    return color;
}

guint32 read_pixel(int x, int y) {
    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);
    guint32* row = reinterpret_cast<guint32*>(data + y * stride);
    return row[x];
}


void pick_color_at(int x, int y, bool set_background) {
    if (!point_in_canvas(x, y)) return;

    cairo_surface_flush(app_state.surface);
    GdkRGBA sampled = pixel_to_rgba(read_pixel(x, y));
    if (set_background) {
        app_state.bg_color = sampled;
    } else {
        app_state.fg_color = sampled;
    }
    update_color_indicators();
}

void flood_fill_at(int start_x, int start_y) {
    if (!point_in_canvas(start_x, start_y)) return;

    cairo_surface_flush(app_state.surface);
    guint32 target = read_pixel(start_x, start_y);
    guint32 replacement = rgba_to_pixel(get_active_color());
    if (target == replacement) return;

    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);

    std::queue<std::pair<int, int>> pixels;
    pixels.push({start_x, start_y});

    while (!pixels.empty()) {
        std::pair<int, int> current = pixels.front();
        pixels.pop();

        int x = current.first;
        int y = current.second;

        if (!point_in_canvas(x, y)) continue;

        guint32* row = reinterpret_cast<guint32*>(data + y * stride);
        if (row[x] != target) continue;

        row[x] = replacement;
        pixels.push({x - 1, y});
        pixels.push({x + 1, y});
        pixels.push({x, y - 1});
        pixels.push({x, y + 1});
    }

    cairo_surface_mark_dirty(app_state.surface);
}

void gradient_fill_at(int start_x, int start_y, int end_x, int end_y, bool circular_gradient) {
    if (!point_in_canvas(start_x, start_y) || !point_in_canvas(end_x, end_y)) return;

    cairo_surface_flush(app_state.surface);
    guint32 target = read_pixel(start_x, start_y);
    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);

    std::queue<std::pair<int, int>> pixels;
    std::vector<bool> visited(app_state.canvas_width * app_state.canvas_height, false);
    pixels.push({start_x, start_y});
    visited[start_y * app_state.canvas_width + start_x] = true;

    double dx = static_cast<double>(end_x - start_x);
    double dy = static_cast<double>(end_y - start_y);
    double length_sq = dx * dx + dy * dy;
    double radius = std::sqrt(length_sq);

    while (!pixels.empty()) {
        std::pair<int, int> current = pixels.front();
        pixels.pop();

        int x = current.first;
        int y = current.second;
        guint32* row = reinterpret_cast<guint32*>(data + y * stride);
        if (row[x] != target) continue;

        double t = 0.0;
        if (circular_gradient) {
            if (radius > 0.0) {
                t = std::hypot(static_cast<double>(x - start_x), static_cast<double>(y - start_y)) / radius;
            }
        } else if (length_sq > 0.0) {
            t = ((x - start_x) * dx + (y - start_y) * dy) / length_sq;
        }
        t = clamp_double(t, 0.0, 1.0);

        GdkRGBA color;
        color.red = app_state.fg_color.red + (app_state.bg_color.red - app_state.fg_color.red) * t;
        color.green = app_state.fg_color.green + (app_state.bg_color.green - app_state.fg_color.green) * t;
        color.blue = app_state.fg_color.blue + (app_state.bg_color.blue - app_state.fg_color.blue) * t;
        color.alpha = app_state.fg_color.alpha + (app_state.bg_color.alpha - app_state.fg_color.alpha) * t;
        row[x] = rgba_to_pixel(color);

        const int neighbors[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const auto& neighbor : neighbors) {
            int nx = x + neighbor[0];
            int ny = y + neighbor[1];
            if (!point_in_canvas(nx, ny)) continue;

            int index = ny * app_state.canvas_width + nx;
            if (visited[index]) continue;
            visited[index] = true;
            pixels.push({nx, ny});
        }
    }

    cairo_surface_mark_dirty(app_state.surface);
}

// Drawing functions for each tool
void draw_line(cairo_t* cr, double x1, double y1, double x2, double y2) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, app_state.line_width);
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    cairo_stroke(cr);
}

void draw_rectangle(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    
    double x = fmin(x1, x2);
    double y = fmin(y1, y2);
    double w = fabs(x2 - x1);
    double h = fabs(y2 - y1);
    
    cairo_rectangle(cr, x, y, w, h);
    
    if (filled) {
        cairo_fill(cr);
    } else {
        cairo_set_line_width(cr, app_state.line_width);
        cairo_stroke(cr);
    }
}

void draw_ellipse(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    
    double cx = (x1 + x2) / 2.0;
    double cy = (y1 + y2) / 2.0;
    double rx = fabs(x2 - x1) / 2.0;
    double ry = fabs(y2 - y1) / 2.0;
    
    if (rx < 0.1 || ry < 0.1) return;
    
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx, ry);
    cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
    cairo_restore(cr);
    
    if (filled) {
        cairo_fill(cr);
    } else {
        cairo_set_line_width(cr, app_state.line_width);
        cairo_stroke(cr);
    }
}

void draw_rounded_rectangle(cairo_t* cr, double x1, double y1, double x2, double y2, bool filled) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    
    double x = fmin(x1, x2);
    double y = fmin(y1, y2);
    double w = fabs(x2 - x1);
    double h = fabs(y2 - y1);
    double r = fmin(w, h) * 0.1;
    
    if (w < 1 || h < 1) return;
    
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_arc(cr, x + w - r, y + r, r, 3 * M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_close_path(cr);
    
    if (filled) {
        cairo_fill(cr);
    } else {
        cairo_set_line_width(cr, app_state.line_width);
        cairo_stroke(cr);
    }
}

void draw_polygon(cairo_t* cr, const std::vector<std::pair<double, double>>& points) {
    if (points.size() < 2) return;

    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, app_state.line_width);

    cairo_move_to(cr, points[0].first, points[0].second);
    for (size_t i = 1; i < points.size(); i++) {
        cairo_line_to(cr, points[i].first, points[i].second);
    }
    cairo_close_path(cr);
    cairo_stroke(cr);
}

void draw_regular_polygon(cairo_t* cr, const std::vector<std::pair<double, double>>& points) {
    draw_polygon(cr, points);
}

void draw_curve(cairo_t* cr, double start_x, double start_y, double control_x, double control_y, double end_x, double end_y) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, app_state.line_width);
    cairo_move_to(cr, start_x, start_y);
    cairo_curve_to(cr, control_x, control_y, control_x, control_y, end_x, end_y);
    cairo_stroke(cr);
}

void draw_pencil(cairo_t* cr, double x, double y) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    
    if (app_state.last_x != 0 && app_state.last_y != 0) {
        cairo_move_to(cr, app_state.last_x, app_state.last_y);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);
    }
}

void draw_paintbrush(cairo_t* cr, double x, double y) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_set_line_width(cr, app_state.line_width * 2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    
    if (app_state.last_x != 0 && app_state.last_y != 0) {
        cairo_move_to(cr, app_state.last_x, app_state.last_y);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);
    }
}

void draw_airbrush(cairo_t* cr, double x, double y) {
    GdkRGBA color = get_active_color();
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    double spray_radius = app_state.line_width * 5.0;
    
    for (int i = 0; i < 20; i++) {
        double angle = g_random_double() * 2 * M_PI;
        double radius = g_random_double() * spray_radius;
        int px = static_cast<int>(std::round(x + cos(angle) * radius));
        int py = static_cast<int>(std::round(y + sin(angle) * radius));
        cairo_rectangle(cr, px, py, 1, 1);
    }
    cairo_fill(cr);
}

void draw_eraser(cairo_t* cr, double x, double y) {
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_line_width(cr, app_state.line_width * 3);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (app_state.last_x != 0 && app_state.last_y != 0) {
        cairo_move_to(cr, app_state.last_x, app_state.last_y);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
}

void draw_smudge(double x, double y) {
    if (app_state.last_x == 0 && app_state.last_y == 0) {
        return;
    }

    int source_x = static_cast<int>(std::round(app_state.last_x));
    int source_y = static_cast<int>(std::round(app_state.last_y));
    int target_x = static_cast<int>(std::round(x));
    int target_y = static_cast<int>(std::round(y));

    cairo_surface_flush(app_state.surface);
    unsigned char* data = cairo_image_surface_get_data(app_state.surface);
    int stride = cairo_image_surface_get_stride(app_state.surface);

    const int radius = static_cast<int>(std::round(app_state.line_width * 2.0));
    const double strength = 0.35;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }

            int sx = source_x + dx;
            int sy = source_y + dy;
            int tx = target_x + dx;
            int ty = target_y + dy;

            if (!point_in_canvas(sx, sy) || !point_in_canvas(tx, ty)) {
                continue;
            }

            guint32* source_row = reinterpret_cast<guint32*>(data + sy * stride);
            guint32* target_row = reinterpret_cast<guint32*>(data + ty * stride);
            GdkRGBA source = pixel_to_rgba(source_row[sx]);
            GdkRGBA target = pixel_to_rgba(target_row[tx]);

            GdkRGBA mixed;
            mixed.red = target.red + (source.red - target.red) * strength;
            mixed.green = target.green + (source.green - target.green) * strength;
            mixed.blue = target.blue + (source.blue - target.blue) * strength;
            mixed.alpha = target.alpha + (source.alpha - target.alpha) * strength;

            target_row[tx] = rgba_to_pixel(mixed);
        }
    }

    cairo_surface_mark_dirty(app_state.surface);
}


// Draw text box overlay
void draw_text_overlay(cairo_t* cr) {
    if (!app_state.text_active) return;
    
    // Draw text box with ant path
    draw_ant_path(cr);
    cairo_rectangle(cr, app_state.text_x, app_state.text_y, 
                   app_state.text_box_width, app_state.text_box_height);
    cairo_stroke(cr);
    
    // Draw text preview
    if (!app_state.text_content.empty()) {
        cairo_select_font_face(cr, app_state.text_font_family.c_str(), 
                              CAIRO_FONT_SLANT_NORMAL, 
                              CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, app_state.text_font_size);
        cairo_set_source_rgba(cr, 
            app_state.fg_color.red,
            app_state.fg_color.green,
            app_state.fg_color.blue,
            app_state.fg_color.alpha
        );
        
        // Simple preview (first line)
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
                
                if (y > app_state.text_y + app_state.text_box_height) break;
            } else {
                word += text[i];
            }
        }
        
        if (!line.empty() && y <= app_state.text_y + app_state.text_box_height) {
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, line.c_str());
        }
    }
}

// Draw selection overlay
void draw_selection_overlay(cairo_t* cr) {
    if (!app_state.has_selection) return;
    
    draw_ant_path(cr);
    
    if (app_state.selection_is_rect) {
        double x1 = fmin(app_state.selection_x1, app_state.selection_x2);
        double y1 = fmin(app_state.selection_y1, app_state.selection_y2);
        double x2 = fmax(app_state.selection_x1, app_state.selection_x2);
        double y2 = fmax(app_state.selection_y1, app_state.selection_y2);
        
        cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
        cairo_stroke(cr);
    } else if (app_state.selection_path.size() > 1) {
        cairo_move_to(cr, app_state.selection_path[0].first, app_state.selection_path[0].second);
        for (size_t i = 1; i < app_state.selection_path.size(); i++) {
            cairo_line_to(cr, app_state.selection_path[i].first, app_state.selection_path[i].second);
        }
        cairo_close_path(cr);
        cairo_stroke(cr);
    }
}

void draw_black_outline_circle(cairo_t* cr, double x, double y, double radius) {
    cairo_save(cr);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_arc(cr, x, y, radius, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void draw_hover_indicator(cairo_t* cr) {
    if (!app_state.hover_in_canvas) {
        return;
    }

    if (tool_shows_brush_hover_outline(app_state.current_tool) && !app_state.is_drawing) {
        double radius = app_state.line_width;
        if (app_state.current_tool == TOOL_ERASER) {
            radius = app_state.line_width * 1.5;
        } else if (app_state.current_tool == TOOL_AIRBRUSH) {
            radius = app_state.line_width * 5.0;
        }
        draw_black_outline_circle(cr, app_state.hover_x, app_state.hover_y, radius);
        return;
    }

    if (tool_shows_vertex_hover_markers(app_state.current_tool) && !app_state.is_drawing) {
        draw_black_outline_circle(cr, app_state.hover_x, app_state.hover_y, 5.0);
    }
}

// Draw preview overlays with ant paths
void draw_preview(cairo_t* cr) {
    if (!app_state.is_drawing) return;
    
    // Dragging an existing floating selection should only show the active
    // selection outline, not a new preview marquee from the selection tool.
    if (app_state.dragging_selection) return;

    cairo_save(cr);
    
    double preview_x = app_state.current_x;
    double preview_y = app_state.current_y;
    
    if (app_state.shift_pressed && !app_state.ellipse_center_mode) {
        if (app_state.current_tool == TOOL_LINE) {
            constrain_line(app_state.start_x, app_state.start_y, preview_x, preview_y);
        } else if (app_state.current_tool == TOOL_ELLIPSE) {
            constrain_to_circle(app_state.start_x, app_state.start_y, preview_x, preview_y);
        } else if (app_state.current_tool == TOOL_RECTANGLE ||
                   app_state.current_tool == TOOL_ROUNDED_RECT ||
                   app_state.current_tool == TOOL_RECT_SELECT) {
            constrain_to_square(app_state.start_x, app_state.start_y, preview_x, preview_y);
        }
    }
    
    switch (app_state.current_tool) {
        case TOOL_CURVE: {
            if (app_state.curve_active) {
                draw_ant_path(cr);

                draw_black_outline_circle(cr, app_state.curve_start_x, app_state.curve_start_y, 5.0);
                if (app_state.curve_has_end) {
                    draw_black_outline_circle(cr, app_state.curve_end_x, app_state.curve_end_y, 5.0);
                }

                if (app_state.curve_has_end) {
                    if (app_state.curve_has_control) {
                        cairo_move_to(cr, app_state.curve_start_x, app_state.curve_start_y);
                        cairo_curve_to(
                            cr,
                            app_state.curve_control_x,
                            app_state.curve_control_y,
                            app_state.curve_control_x,
                            app_state.curve_control_y,
                            app_state.curve_end_x,
                            app_state.curve_end_y
                        );
                    } else {
                        cairo_move_to(cr, app_state.curve_start_x, app_state.curve_start_y);
                        cairo_line_to(cr, app_state.curve_end_x, app_state.curve_end_y);
                    }
                    cairo_stroke(cr);
                }
            }
            break;
        }

        case TOOL_RECT_SELECT: {
            double x = fmin(app_state.start_x, preview_x);
            double y = fmin(app_state.start_y, preview_y);
            double w = fabs(preview_x - app_state.start_x);
            double h = fabs(preview_y - app_state.start_y);
            
            draw_ant_path(cr);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);
            break;
        }
        
        case TOOL_LASSO_SELECT: {
            if (app_state.lasso_points.size() > 1) {
                draw_ant_path(cr);
                cairo_move_to(cr, app_state.lasso_points[0].first, app_state.lasso_points[0].second);
                for (size_t i = 1; i < app_state.lasso_points.size(); i++) {
                    cairo_line_to(cr, app_state.lasso_points[i].first, app_state.lasso_points[i].second);
                }
                if (app_state.lasso_polygon_mode) {
                    cairo_line_to(cr, preview_x, preview_y);
                }
                cairo_stroke(cr);
            }
            if (app_state.lasso_polygon_mode) {
                for (const auto& point : app_state.lasso_points) {
                    draw_black_outline_circle(cr, point.first, point.second, 5.0);
                }
            }
            break;
        }
        
        case TOOL_LINE: {
            draw_ant_path(cr);
            cairo_move_to(cr, app_state.start_x, app_state.start_y);
            cairo_line_to(cr, preview_x, preview_y);
            cairo_stroke(cr);
            draw_black_outline_circle(cr, app_state.start_x, app_state.start_y, 5.0);
            draw_black_outline_circle(cr, preview_x, preview_y, 5.0);
            break;
        }
        
        case TOOL_RECTANGLE: {
            double x = fmin(app_state.start_x, preview_x);
            double y = fmin(app_state.start_y, preview_y);
            double w = fabs(preview_x - app_state.start_x);
            double h = fabs(preview_y - app_state.start_y);
            
            draw_ant_path(cr);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);
            break;
        }
        
        case TOOL_ELLIPSE: {
            double cx;
            double cy;
            double rx;
            double ry;

            if (app_state.ellipse_center_mode) {
                double radius = std::hypot(preview_x - app_state.start_x, preview_y - app_state.start_y);
                cx = app_state.start_x;
                cy = app_state.start_y;
                rx = radius;
                ry = radius;
            } else {
                cx = (app_state.start_x + preview_x) / 2.0;
                cy = (app_state.start_y + preview_y) / 2.0;
                rx = fabs(preview_x - app_state.start_x) / 2.0;
                ry = fabs(preview_y - app_state.start_y) / 2.0;
            }
            
            if (rx > 0.1 && ry > 0.1) {
                draw_ant_path(cr);
                cairo_save(cr);
                cairo_translate(cr, cx, cy);
                cairo_scale(cr, rx, ry);
                cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
                cairo_restore(cr);
                cairo_stroke(cr);
            }
            break;
        }

        case TOOL_REGULAR_POLYGON: {
            std::vector<std::pair<double, double>> points;
            build_regular_polygon_points(
                app_state.start_x,
                app_state.start_y,
                preview_x,
                preview_y,
                app_state.ctrl_pressed,
                app_state.shift_pressed,
                app_state.regular_polygon_sides,
                points
            );

            if (points.size() >= 3) {
                draw_ant_path(cr);
                cairo_move_to(cr, points[0].first, points[0].second);
                for (size_t i = 1; i < points.size(); ++i) {
                    cairo_line_to(cr, points[i].first, points[i].second);
                }
                cairo_close_path(cr);
                cairo_stroke(cr);
            }
            break;
        }
        
        case TOOL_ROUNDED_RECT: {
            double x = fmin(app_state.start_x, preview_x);
            double y = fmin(app_state.start_y, preview_y);
            double w = fabs(preview_x - app_state.start_x);
            double h = fabs(preview_y - app_state.start_y);
            double r = fmin(w, h) * 0.1;
            
            if (w > 1 && h > 1) {
                draw_ant_path(cr);
                cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
                cairo_arc(cr, x + w - r, y + r, r, 3 * M_PI / 2, 0);
                cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
                cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
                cairo_close_path(cr);
                cairo_stroke(cr);
            }
            break;
        }

        case TOOL_GRADIENT_FILL: {
            draw_ant_path(cr);
            draw_black_outline_circle(cr, app_state.start_x, app_state.start_y, 5.0);
            draw_black_outline_circle(cr, preview_x, preview_y, 5.0);

            if (app_state.gradient_fill_circular) {
                double radius = std::hypot(preview_x - app_state.start_x, preview_y - app_state.start_y);
                cairo_arc(cr, app_state.start_x, app_state.start_y, radius, 0, 2 * M_PI);
            } else {
                cairo_move_to(cr, app_state.start_x, app_state.start_y);
                cairo_line_to(cr, preview_x, preview_y);
            }
            cairo_stroke(cr);
            break;
        }
        
        case TOOL_POLYGON: {
            if (app_state.polygon_points.size() > 0) {
                draw_ant_path(cr);
                cairo_move_to(cr, app_state.polygon_points[0].first, app_state.polygon_points[0].second);
                for (size_t i = 1; i < app_state.polygon_points.size(); i++) {
                    cairo_line_to(cr, app_state.polygon_points[i].first, app_state.polygon_points[i].second);
                }

                if (app_state.polygon_finished) {
                    cairo_close_path(cr);
                } else {
                    cairo_line_to(cr, preview_x, preview_y);
                }

                cairo_stroke(cr);
                
                for (const auto& point : app_state.polygon_points) {
                    draw_black_outline_circle(cr, point.first, point.second, 5.0);
                }
            }
            break;
        }
    }
    
    cairo_restore(cr);
}

bool is_close_to_guide_blue(guint8 r, guint8 g, guint8 b) {
    const int target_r = 0;
    const int target_g = 0;
    const int target_b = 255;
    return std::abs((int)r - target_r) <= 10 && std::abs((int)g - target_g) <= 10 && std::abs((int)b - target_b) <= 10;
}

void get_visible_composited_rgb(int x, int y, guint8& out_r, guint8& out_g, guint8& out_b) {
    double r = 1.0, g = 1.0, b = 1.0, a = 1.0;
    for (int i = 0; i < (int)app_state.layers.size(); ++i) {
        const Layer& layer = app_state.layers[i];
        if (!layer.visible || !layer.surface) continue;
        int lw = cairo_image_surface_get_width(layer.surface);
        int lh = cairo_image_surface_get_height(layer.surface);
        if (x < 0 || y < 0 || x >= lw || y >= lh) continue;
        cairo_surface_flush(layer.surface);
        unsigned char* data = cairo_image_surface_get_data(layer.surface);
        int stride = cairo_image_surface_get_stride(layer.surface);
        guint32 pixel = *reinterpret_cast<guint32*>(data + y * stride + x * 4);
        double sa = (((pixel >> 24) & 0xFF) / 255.0) * layer.opacity;
        double sr = ((pixel >> 16) & 0xFF) / 255.0;
        double sg = ((pixel >> 8) & 0xFF) / 255.0;
        double sb = (pixel & 0xFF) / 255.0;
        r = sr * sa + r * (1.0 - sa);
        g = sg * sa + g * (1.0 - sa);
        b = sb * sa + b * (1.0 - sa);
    }
    out_r = (guint8)std::round(std::max(0.0, std::min(1.0, r)) * 255.0);
    out_g = (guint8)std::round(std::max(0.0, std::min(1.0, g)) * 255.0);
    out_b = (guint8)std::round(std::max(0.0, std::min(1.0, b)) * 255.0);
}

void draw_vertical_guide(cairo_t* cr, double x) {
    int xi = (int)std::round(x);
    if (xi < 0 || xi >= app_state.canvas_width) return;
    for (int y = 0; y < app_state.canvas_height; ++y) {
        guint8 r, g, b;
        get_visible_composited_rgb(xi, y, r, g, b);
        if (is_close_to_guide_blue(r, g, b)) {
            cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
        } else {
            cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
        }
        cairo_rectangle(cr, xi, y, 1, 1);
        cairo_fill(cr);
    }
}

void draw_horizontal_guide(cairo_t* cr, double y) {
    int yi = (int)std::round(y);
    if (yi < 0 || yi >= app_state.canvas_height) return;
    for (int x = 0; x < app_state.canvas_width; ++x) {
        guint8 r, g, b;
        get_visible_composited_rgb(x, yi, r, g, b);
        if (is_close_to_guide_blue(r, g, b)) {
            cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
        } else {
            cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
        }
        cairo_rectangle(cr, x, yi, 1, 1);
        cairo_fill(cr);
    }
}

void draw_canvas_grid_background(cairo_t* cr, double width, double height) {
    static cairo_pattern_t* checker_pattern = nullptr;

    if (!checker_pattern) {
        const int checker_size = 1;
        const int pattern_size = checker_size * 2;

        cairo_surface_t* pattern_surface = cairo_image_surface_create(
            CAIRO_FORMAT_RGB24,
            pattern_size,
            pattern_size
        );
        cairo_t* pattern_cr = cairo_create(pattern_surface);
        configure_crisp_rendering(pattern_cr);

        cairo_set_source_rgb(pattern_cr, 1.0, 1.0, 1.0);
        cairo_paint(pattern_cr);

        cairo_set_source_rgb(pattern_cr, 0.0, 0.0, 0.0);
        cairo_rectangle(pattern_cr, 0, 0, checker_size, checker_size);
        cairo_rectangle(pattern_cr, checker_size, checker_size, checker_size, checker_size);
        cairo_fill(pattern_cr);

        cairo_destroy(pattern_cr);

        checker_pattern = cairo_pattern_create_for_surface(pattern_surface);
        cairo_pattern_set_extend(checker_pattern, CAIRO_EXTEND_REPEAT);
        cairo_pattern_set_filter(checker_pattern, CAIRO_FILTER_NEAREST);
        cairo_surface_destroy(pattern_surface);
    }

    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_set_source(cr, checker_pattern);
    cairo_fill(cr);
    cairo_restore(cr);
}

// Canvas draw callback
gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    if (app_state.layers.empty()) {
        return FALSE;
    }

    configure_crisp_rendering(cr);
    draw_canvas_grid_background(
        cr,
        app_state.canvas_width * app_state.zoom_factor,
        app_state.canvas_height * app_state.zoom_factor
    );
    cairo_save(cr);
    cairo_scale(cr, app_state.zoom_factor, app_state.zoom_factor);

    for (const Layer& layer : app_state.layers) {
        if (!layer.visible || !layer.surface) continue;
        cairo_set_source_surface(cr, layer.surface, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_paint_with_alpha(cr, layer.opacity);
    }
 
    if (app_state.floating_selection_active && app_state.floating_surface) {
        double x = std::round(fmin(app_state.selection_x1, app_state.selection_x2));
        double y = std::round(fmin(app_state.selection_y1, app_state.selection_y2));
        cairo_set_source_surface(cr, app_state.floating_surface, x, y);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_paint(cr);
    }
 
    if (app_state.show_vertical_center_guide) {
        draw_vertical_guide(cr, app_state.canvas_width / 2.0);
    }
    if (app_state.show_horizontal_center_guide) {
        draw_horizontal_guide(cr, app_state.canvas_height / 2.0);
    }
    for (double x : app_state.vertical_guides) draw_vertical_guide(cr, x);
    for (double y : app_state.horizontal_guides) draw_horizontal_guide(cr, y);

    draw_line_pattern_preview_overlay(cr);

    if (app_state.has_selection) {
       draw_selection_overlay(cr);
    }
    if (app_state.text_active) {
        draw_text_overlay(cr);
    }
    if (tool_needs_preview(app_state.current_tool)) {
        draw_preview(cr);
    }
    draw_hover_indicator(cr);
    cairo_restore(cr);
    return FALSE;
}

// Key press event
gboolean on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer data) {
    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R) {
        app_state.shift_pressed = true;
        if (app_state.is_drawing && app_state.drawing_area) {
            gtk_widget_queue_draw(app_state.drawing_area);
        }
    } else if (event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
        app_state.ctrl_pressed = true;
        if (app_state.is_drawing && app_state.drawing_area) {
            gtk_widget_queue_draw(app_state.drawing_area);
        }
    } else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_c) {
        copy_selection();
    } else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_x) {
        cut_selection();
    } else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_v) {
        paste_selection();
    } else if ((event->state & GDK_CONTROL_MASK) && (event->state & GDK_SHIFT_MASK) && event->keyval == GDK_KEY_Z) {
        redo_last_operation();
    } else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_y) {
        redo_last_operation();
    } else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_z) {
        undo_last_operation();
    }
    return FALSE;
}

// Key release event
gboolean on_key_release(GtkWidget* widget, GdkEventKey* event, gpointer data) {
    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R) {
        app_state.shift_pressed = false;
        if (app_state.is_drawing && app_state.drawing_area) {
            gtk_widget_queue_draw(app_state.drawing_area);
        }
    } else if (event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
        app_state.ctrl_pressed = false;
        if (app_state.is_drawing && app_state.drawing_area) {
            gtk_widget_queue_draw(app_state.drawing_area);
        }
    }
    return FALSE;
}

// Mouse button press
gboolean on_button_press(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    if ((event->button == 1 || event->button == 3) && app_state.surface) {
        double canvas_x = to_canvas_coordinate(event->x);
        double canvas_y = to_canvas_coordinate(event->y);

        if (app_state.floating_selection_active) {
            if (app_state.floating_drag_completed || !point_in_selection(canvas_x, canvas_y)) {
                commit_floating_selection();
                return TRUE;
            }

            start_selection_drag();
            app_state.dragging_selection = true;
            app_state.selection_drag_offset_x = canvas_x - fmin(app_state.selection_x1, app_state.selection_x2);
            app_state.selection_drag_offset_y = canvas_y - fmin(app_state.selection_y1, app_state.selection_y2);
            app_state.is_drawing = true;
            return TRUE;
        }

        // Handle zoom tool
        if (app_state.current_tool == TOOL_ZOOM && event->button == 1) {
            double selected_zoom = zoom_options[app_state.active_zoom_index];
            if (selected_zoom == 1.0) {
                reset_zoom_to_default();
            } else {
                apply_zoom(selected_zoom, canvas_x, canvas_y);
            }
            return TRUE;
        }
        // Handle text tool
        if (app_state.current_tool == TOOL_TEXT) {
            if (app_state.text_active && !point_in_text_box(canvas_x, canvas_y)) {
                // Clicked outside text box
                if (event->button == 1) {
                    // Left-click - finalize text
                    finalize_text();
                } else {
                    // Right-click - cancel text
                    cancel_text();
                }
                return TRUE;
            } else if (!app_state.text_active) {
                // Start new text box (only with left-click)
                if (event->button == 1) {
                    app_state.text_active = true;
                    app_state.text_x = canvas_x;
                    app_state.text_y = canvas_y;
                    app_state.text_content.clear();
                    
                    // Initialize text box size
                    update_text_box_size();
                    
                    create_text_window(canvas_x, canvas_y);
                    start_ant_animation();
                    gtk_widget_queue_draw(widget);
                }
                return TRUE;
            }
            // If clicking inside text box, do nothing (keep editing)
            return TRUE;
        }

        if ((app_state.current_tool == TOOL_RECT_SELECT || app_state.current_tool == TOOL_LASSO_SELECT) &&
            app_state.has_selection && point_in_selection(canvas_x, canvas_y)) {
            start_selection_drag();
            if (app_state.floating_selection_active) {
                app_state.dragging_selection = true;
                app_state.selection_drag_offset_x = canvas_x - fmin(app_state.selection_x1, app_state.selection_x2);
                app_state.selection_drag_offset_y = canvas_y - fmin(app_state.selection_y1, app_state.selection_y2);
                app_state.is_drawing = true;
                return TRUE;
            }
        }

        // Check if clicking outside selection area - clear selection
        if (app_state.has_selection && !point_in_selection(canvas_x, canvas_y)) {
            clear_selection();
        }
        
        // Finalize text if active and clicking with different tool
        if (app_state.text_active && app_state.current_tool != TOOL_TEXT) {
            finalize_text();
        }

        app_state.is_right_button = (event->button == 3);

        if (app_state.current_tool == TOOL_EYEDROPPER) {
            pick_color_at(static_cast<int>(canvas_x), static_cast<int>(canvas_y), app_state.is_right_button);
            return TRUE;
        }

        if (app_state.current_tool == TOOL_FILL) {
            push_undo_state();
            flood_fill_at(static_cast<int>(canvas_x), static_cast<int>(canvas_y));
            gtk_widget_queue_draw(widget);
            return TRUE;
        }

        if (app_state.current_tool == TOOL_GRADIENT_FILL) {
            if (!app_state.gradient_fill_first_point_set) {
                app_state.gradient_fill_first_point_set = true;
                app_state.gradient_fill_circular = ((event->state & GDK_CONTROL_MASK) != 0);
                app_state.start_x = canvas_x;
                app_state.start_y = canvas_y;
                app_state.current_x = canvas_x;
                app_state.current_y = canvas_y;
                app_state.is_drawing = true;
                start_ant_animation();
            } else {
                bool circular_gradient = ((event->state & GDK_CONTROL_MASK) != 0) || app_state.gradient_fill_circular;
                push_undo_state();
                gradient_fill_at(
                    static_cast<int>(app_state.start_x),
                    static_cast<int>(app_state.start_y),
                    static_cast<int>(canvas_x),
                    static_cast<int>(canvas_y),
                    circular_gradient
                );
                app_state.gradient_fill_first_point_set = false;
                app_state.gradient_fill_circular = false;
                app_state.is_drawing = false;
                stop_ant_animation();
            }
            gtk_widget_queue_draw(widget);
            return TRUE;
        }

        if (app_state.current_tool == TOOL_LASSO_SELECT) {
            if (event->button == 1) {
                if (app_state.lasso_polygon_mode) {
                    app_state.lasso_points.push_back({canvas_x, canvas_y});
                    app_state.current_x = canvas_x;
                    app_state.current_y = canvas_y;
                    app_state.is_drawing = true;
                    gtk_widget_queue_draw(widget);
                    return TRUE;
                }

                app_state.is_drawing = true;
                app_state.lasso_polygon_mode = ((event->state & GDK_CONTROL_MASK) != 0);
                app_state.lasso_points.clear();
                app_state.lasso_points.push_back({canvas_x, canvas_y});
                app_state.current_x = canvas_x;
                app_state.current_y = canvas_y;
                start_ant_animation();
                gtk_widget_queue_draw(widget);
                return TRUE;
            }

            if (event->button == 3 && app_state.lasso_polygon_mode) {
                finalize_lasso_selection();
                gtk_widget_queue_draw(widget);
                return TRUE;
            }
        }

        if (app_state.current_tool == TOOL_POLYGON) {
            if (event->button == 1) {
                if (app_state.polygon_finished) {
                    push_undo_state();
                    app_state.is_right_button = false;
                    cairo_t* cr = cairo_create(app_state.surface);
                    configure_crisp_rendering(cr);
                    draw_polygon(cr, app_state.polygon_points);
                    cairo_destroy(cr);

                    app_state.polygon_points.clear();
                    app_state.polygon_finished = false;
                    app_state.is_drawing = false;
                    stop_ant_animation();
                    gtk_widget_queue_draw(widget);
                    return TRUE;
                }

                app_state.polygon_points.push_back({canvas_x, canvas_y});
                app_state.is_drawing = true;
                app_state.current_x = canvas_x;
                app_state.current_y = canvas_y;
                start_ant_animation();
                gtk_widget_queue_draw(widget);
                return TRUE;
            }

            if (event->button == 3) {
                if (!app_state.polygon_finished && app_state.polygon_points.size() >= 2) {
                    app_state.polygon_finished = true;
                    app_state.is_drawing = true;
                } else if (app_state.polygon_finished) {
                    push_undo_state();
                    cairo_t* cr = cairo_create(app_state.surface);
                    configure_crisp_rendering(cr);
                    draw_polygon(cr, app_state.polygon_points);
                    cairo_destroy(cr);
                    app_state.polygon_points.clear();
                    app_state.polygon_finished = false;
                    app_state.is_drawing = false;
                    stop_ant_animation();
                }

                gtk_widget_queue_draw(widget);
                return TRUE;
            }
        }

        if (app_state.current_tool == TOOL_ELLIPSE && app_state.ellipse_center_mode && event->button == 1) {
            push_undo_state();

            double radius = std::hypot(canvas_x - app_state.start_x, canvas_y - app_state.start_y);
            double x1 = app_state.start_x - radius;
            double y1 = app_state.start_y - radius;
            double x2 = app_state.start_x + radius;
            double y2 = app_state.start_y + radius;

            cairo_t* cr = cairo_create(app_state.surface);
            configure_crisp_rendering(cr);
            draw_ellipse(cr, x1, y1, x2, y2, false);
            cairo_destroy(cr);

            app_state.ellipse_center_mode = false;
            app_state.is_drawing = false;
            stop_ant_animation();
            gtk_widget_queue_draw(widget);
            return TRUE;
        }

        if (app_state.current_tool == TOOL_ELLIPSE && event->button == 1 &&
            ((event->state & GDK_CONTROL_MASK) != 0)) {
            app_state.ellipse_center_mode = true;
            app_state.is_drawing = true;
            app_state.is_right_button = false;
            app_state.start_x = canvas_x;
            app_state.start_y = canvas_y;
            app_state.current_x = canvas_x;
            app_state.current_y = canvas_y;
            start_ant_animation();
            gtk_widget_queue_draw(widget);
            return TRUE;
        }
        
        if (app_state.current_tool == TOOL_CURVE) {
            if (!app_state.curve_active) {
                app_state.curve_active = true;
                app_state.curve_has_end = false;
                app_state.curve_has_control = false;
                app_state.curve_primary_right_button = (event->button == 3);
                app_state.curve_start_x = canvas_x;
                app_state.curve_start_y = canvas_y;
                app_state.is_drawing = true;
                app_state.current_x = canvas_x;
                app_state.current_y = canvas_y;
                start_ant_animation();
                gtk_widget_queue_draw(widget);
                return TRUE;
            }

            bool used_primary_button = ((event->button == 3) == app_state.curve_primary_right_button);
            if (!used_primary_button) {
                if (app_state.curve_has_end) {
                    cairo_t* cr = cairo_create(app_state.surface);
                    configure_crisp_rendering(cr);

                    app_state.is_right_button = app_state.curve_primary_right_button;
                    if (app_state.curve_has_control) {
                        draw_curve(
                            cr,
                            app_state.curve_start_x,
                            app_state.curve_start_y,
                            app_state.curve_control_x,
                            app_state.curve_control_y,
                            app_state.curve_end_x,
                            app_state.curve_end_y
                        );
                    } else {
                        draw_line(
                            cr,
                            app_state.curve_start_x,
                            app_state.curve_start_y,
                            app_state.curve_end_x,
                            app_state.curve_end_y
                        );
                    }

                    cairo_destroy(cr);
                }

                app_state.curve_active = false;
                app_state.curve_has_end = false;
                app_state.curve_has_control = false;
                app_state.is_drawing = false;
                app_state.is_right_button = false;
                stop_ant_animation();
                gtk_widget_queue_draw(widget);
                return TRUE;
            }

            if (app_state.curve_has_end && (event->state & GDK_SHIFT_MASK)) {
                app_state.curve_start_x = canvas_x;
                app_state.curve_start_y = canvas_y;
                app_state.current_x = canvas_x;
                app_state.current_y = canvas_y;
                gtk_widget_queue_draw(widget);
                return TRUE;
            }

            if (app_state.curve_has_end && (event->state & GDK_CONTROL_MASK)) {
                app_state.curve_end_x = canvas_x;
                app_state.curve_end_y = canvas_y;
                app_state.current_x = canvas_x;
                app_state.current_y = canvas_y;
                gtk_widget_queue_draw(widget);
                return TRUE;
            }

            if (!app_state.curve_has_end) {
                app_state.curve_end_x = canvas_x;
                app_state.curve_end_y = canvas_y;
                app_state.curve_has_end = true;
            } else {
                app_state.curve_control_x = canvas_x;
                app_state.curve_control_y = canvas_y;
                app_state.curve_has_control = true;
            }

            app_state.is_drawing = true;
            app_state.current_x = canvas_x;
            app_state.current_y = canvas_y;
            gtk_widget_queue_draw(widget);
            return TRUE;
        }

        app_state.is_drawing = true;
        if (app_state.current_tool == TOOL_PENCIL || app_state.current_tool == TOOL_PAINTBRUSH ||
            app_state.current_tool == TOOL_AIRBRUSH || app_state.current_tool == TOOL_ERASER ||
            app_state.current_tool == TOOL_SMUDGE ||
            app_state.current_tool == TOOL_LINE || app_state.current_tool == TOOL_CURVE ||
			app_state.current_tool == TOOL_RECTANGLE || app_state.current_tool == TOOL_ELLIPSE ||
            app_state.current_tool == TOOL_REGULAR_POLYGON || app_state.current_tool == TOOL_ROUNDED_RECT) {
            push_undo_state();
        }
        app_state.last_x = canvas_x;
        app_state.last_y = canvas_y;
        app_state.start_x = canvas_x;
        app_state.start_y = canvas_y;
        app_state.current_x = canvas_x;
        app_state.current_y = canvas_y;
        if (app_state.current_tool == TOOL_REGULAR_POLYGON) {
            app_state.ctrl_pressed = ((event->state & GDK_CONTROL_MASK) != 0);
        }
        
        if (app_state.current_tool == TOOL_AIRBRUSH) {
            cairo_t* cr = cairo_create(app_state.surface);
            configure_crisp_rendering(cr);
            draw_airbrush(cr, canvas_x, canvas_y);
            cairo_destroy(cr);
            gtk_widget_queue_draw(widget);
        }
        
        if (tool_needs_preview(app_state.current_tool)) {
            start_ant_animation();
        }
    }
    return TRUE;
}

// Mouse motion
gboolean on_motion_notify(GtkWidget* widget, GdkEventMotion* event, gpointer data) {
    if (app_state.surface) {
        double canvas_x = to_canvas_coordinate(event->x);
        double canvas_y = to_canvas_coordinate(event->y);
        app_state.hover_in_canvas = true;
        app_state.hover_x = canvas_x;
        app_state.hover_y = canvas_y;
        update_cursor_position_label(canvas_x, canvas_y, true);

        if (!app_state.is_drawing) {
            if (tool_shows_brush_hover_outline(app_state.current_tool) ||
                tool_shows_vertex_hover_markers(app_state.current_tool)) {
                gtk_widget_queue_draw(widget);
            }
            return TRUE;
        }

        app_state.current_x = canvas_x;
        app_state.current_y = canvas_y;

        if (app_state.current_tool == TOOL_REGULAR_POLYGON) {
            app_state.ctrl_pressed = ((event->state & GDK_CONTROL_MASK) != 0);
        }

        if (app_state.dragging_selection && app_state.has_selection) {
            double old_x = fmin(app_state.selection_x1, app_state.selection_x2);
            double old_y = fmin(app_state.selection_y1, app_state.selection_y2);
            double width = fabs(app_state.selection_x2 - app_state.selection_x1);
            double height = fabs(app_state.selection_y2 - app_state.selection_y1);
            double new_x = std::round(canvas_x - app_state.selection_drag_offset_x);
            double new_y = std::round(canvas_y - app_state.selection_drag_offset_y);

            double dx = new_x - old_x;
            double dy = new_y - old_y;

            app_state.selection_x1 = new_x;
            app_state.selection_y1 = new_y;
            app_state.selection_x2 = new_x + width;
            app_state.selection_y2 = new_y + height;

            if (!app_state.selection_is_rect) {
                for (auto& point : app_state.selection_path) {
                    point.first += dx;
                    point.second += dy;
                }
            }

            gtk_widget_queue_draw(widget);
        } else if (app_state.current_tool == TOOL_LASSO_SELECT && !app_state.lasso_polygon_mode) {
            app_state.lasso_points.push_back({canvas_x, canvas_y});
            gtk_widget_queue_draw(widget);
        } else if (tool_needs_preview(app_state.current_tool)) {
            gtk_widget_queue_draw(widget);
        } else {
            cairo_t* cr = cairo_create(app_state.surface);
            configure_crisp_rendering(cr);
            switch (app_state.current_tool) {
                case TOOL_PENCIL:
                    draw_pencil(cr, canvas_x, canvas_y);
                    break;
                case TOOL_PAINTBRUSH:
                    draw_paintbrush(cr, canvas_x, canvas_y);
                    break;
                case TOOL_AIRBRUSH:
                    draw_airbrush(cr, canvas_x, canvas_y);
                    break;
                case TOOL_ERASER:
                    draw_eraser(cr, canvas_x, canvas_y);
                    break;
                case TOOL_SMUDGE:
                    cairo_destroy(cr);
                    draw_smudge(canvas_x, canvas_y);
                    cr = nullptr;
                    break;
            }
            
            if (cr) {
                cairo_destroy(cr);
            }
            app_state.last_x = canvas_x;
            app_state.last_y = canvas_y;
            gtk_widget_queue_draw(widget);
        }
    }
    return TRUE;
}

gboolean on_leave_notify(GtkWidget* widget, GdkEventCrossing* event, gpointer data) {
    if (app_state.hover_in_canvas) {
        app_state.hover_in_canvas = false;
        update_cursor_position_label(0.0, 0.0, false);
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

// Mouse button release
gboolean on_button_release(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    if ((event->button == 1 || event->button == 3) && app_state.surface && app_state.is_drawing) {
        if (app_state.current_tool == TOOL_ELLIPSE && app_state.ellipse_center_mode) {
            return TRUE;
        }

        if (app_state.current_tool == TOOL_GRADIENT_FILL) {
            return TRUE;
        }

        if (app_state.current_tool == TOOL_CURVE) {
            return TRUE;
        }

        if (app_state.current_tool == TOOL_POLYGON) {
            return TRUE;
        }

        if (app_state.current_tool == TOOL_LASSO_SELECT && app_state.lasso_polygon_mode) {
            return TRUE;
        }

        if (app_state.dragging_selection) {
            app_state.dragging_selection = false;
            app_state.is_drawing = false;
            app_state.floating_drag_completed = true;
            commit_floating_selection(false);
            gtk_widget_queue_draw(widget);
            return TRUE;
        }

        double end_x = to_canvas_coordinate(event->x);
        double end_y = to_canvas_coordinate(event->y);
        
        if (app_state.shift_pressed) {
            if (app_state.current_tool == TOOL_LINE) {
                constrain_line(app_state.start_x, app_state.start_y, end_x, end_y);
            } else if (app_state.current_tool == TOOL_ELLIPSE) {
                constrain_to_circle(app_state.start_x, app_state.start_y, end_x, end_y);
            } else if (app_state.current_tool == TOOL_RECTANGLE ||
                       app_state.current_tool == TOOL_ROUNDED_RECT ||
                       app_state.current_tool == TOOL_RECT_SELECT) {
                constrain_to_square(app_state.start_x, app_state.start_y, end_x, end_y);
            }
        }
        
        cairo_t* cr = cairo_create(app_state.surface);
        configure_crisp_rendering(cr);        
        switch (app_state.current_tool) {
            case TOOL_LINE:
                draw_line(cr, app_state.start_x, app_state.start_y, end_x, end_y);
                stop_ant_animation();
                break;
            case TOOL_RECTANGLE:
                draw_rectangle(cr, app_state.start_x, app_state.start_y, end_x, end_y, false);
                stop_ant_animation();
                break;
            case TOOL_ELLIPSE:
                draw_ellipse(cr, app_state.start_x, app_state.start_y, end_x, end_y, false);
                stop_ant_animation();
                break;
            case TOOL_REGULAR_POLYGON: {
                std::vector<std::pair<double, double>> points;
                build_regular_polygon_points(
                    app_state.start_x,
                    app_state.start_y,
                    end_x,
                    end_y,
                    ((event->state & GDK_CONTROL_MASK) != 0) || app_state.ctrl_pressed,
                    app_state.shift_pressed,
                    app_state.regular_polygon_sides,
                    points
                );
                draw_regular_polygon(cr, points);
                stop_ant_animation();
                break;
            }
            case TOOL_ROUNDED_RECT:
                draw_rounded_rectangle(cr, app_state.start_x, app_state.start_y, end_x, end_y, false);
                stop_ant_animation();
                break;
            case TOOL_RECT_SELECT:
                app_state.has_selection = true;
                app_state.selection_is_rect = true;
                app_state.floating_selection_active = false;
                app_state.selection_x1 = app_state.start_x;
                app_state.selection_y1 = app_state.start_y;
                app_state.selection_x2 = end_x;
                app_state.selection_y2 = end_y;
                break;
            case TOOL_LASSO_SELECT:
                finalize_lasso_selection();
                break;
        }
        
        cairo_destroy(cr);
        app_state.is_drawing = false;
        app_state.is_right_button = false;
        app_state.ellipse_center_mode = false;
        app_state.last_x = 0;
        app_state.last_y = 0;
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
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

std::string get_file_extension_lowercase(const std::string& filename) {
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
            CAIRO_FORMAT_RGB24,
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

struct ColorBalanceWindowState {
    cairo_surface_t* original_surface = nullptr;
    GtkWidget* red_scale = nullptr;
    GtkWidget* green_scale = nullptr;
    GtkWidget* blue_scale = nullptr;
    int layer_index = -1;
    bool undo_pushed = false;
};

void apply_color_balance_from_original(ColorBalanceWindowState* state) {
    if (!state || state->layer_index < 0 || state->layer_index >= (int)app_state.layers.size()) {
        return;
    }
    cairo_surface_t* target_surface = app_state.layers[state->layer_index].surface;
    if (!state->original_surface || !target_surface) {
        return;
    }

    if (!state->undo_pushed) {
        push_undo_state();
        state->undo_pushed = true;
    }

    const double red_factor = 1.0 + (gtk_range_get_value(GTK_RANGE(state->red_scale)) / 100.0);
    const double green_factor = 1.0 + (gtk_range_get_value(GTK_RANGE(state->green_scale)) / 100.0);
    const double blue_factor = 1.0 + (gtk_range_get_value(GTK_RANGE(state->blue_scale)) / 100.0);

    cairo_surface_flush(state->original_surface);
    cairo_surface_flush(target_surface);

    unsigned char* source_data = cairo_image_surface_get_data(state->original_surface);
    unsigned char* target_data = cairo_image_surface_get_data(target_surface);
    const int source_stride = cairo_image_surface_get_stride(state->original_surface);
    const int target_stride = cairo_image_surface_get_stride(target_surface);

    for (int y = 0; y < app_state.canvas_height; ++y) {
        guint32* source_row = reinterpret_cast<guint32*>(source_data + y * source_stride);
        guint32* target_row = reinterpret_cast<guint32*>(target_data + y * target_stride);
        for (int x = 0; x < app_state.canvas_width; ++x) {
            const guint32 pixel = source_row[x];
            const guint8 alpha = (pixel >> 24) & 0xFF;
            const guint8 premul_red = (pixel >> 16) & 0xFF;
            const guint8 premul_green = (pixel >> 8) & 0xFF;
            const guint8 premul_blue = pixel & 0xFF;

            double red = 0.0;
            double green = 0.0;
            double blue = 0.0;
            if (alpha > 0) {
                const double alpha_factor = 255.0 / static_cast<double>(alpha);
                red = std::min(255.0, premul_red * alpha_factor);
                green = std::min(255.0, premul_green * alpha_factor);
                blue = std::min(255.0, premul_blue * alpha_factor);
            }

            red = clamp_double(red * red_factor, 0.0, 255.0);
            green = clamp_double(green * green_factor, 0.0, 255.0);
            blue = clamp_double(blue * blue_factor, 0.0, 255.0);

            const double premul_scale = static_cast<double>(alpha) / 255.0;
            const guint8 out_red = static_cast<guint8>(std::lround(red * premul_scale));
            const guint8 out_green = static_cast<guint8>(std::lround(green * premul_scale));
            const guint8 out_blue = static_cast<guint8>(std::lround(blue * premul_scale));

            target_row[x] = (static_cast<guint32>(alpha) << 24) |
                            (static_cast<guint32>(out_red) << 16) |
                            (static_cast<guint32>(out_green) << 8) |
                            static_cast<guint32>(out_blue);
        }
    }

    cairo_surface_mark_dirty(target_surface);
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

struct BrightnessContrastWindowState {
    cairo_surface_t* original_surface = nullptr;
    GtkWidget* brightness_scale = nullptr;
    GtkWidget* contrast_scale = nullptr;
    int layer_index = -1;
    bool undo_pushed = false;
};

void apply_brightness_contrast_from_original(BrightnessContrastWindowState* state) {
    if (!state || state->layer_index < 0 || state->layer_index >= (int)app_state.layers.size()) {
        return;
    }

    cairo_surface_t* target_surface = app_state.layers[state->layer_index].surface;
    if (!state->original_surface || !target_surface) {
        return;
    }

    if (!state->undo_pushed) {
        push_undo_state();
        state->undo_pushed = true;
    }

    const double brightness = gtk_range_get_value(GTK_RANGE(state->brightness_scale));
    const double contrast = gtk_range_get_value(GTK_RANGE(state->contrast_scale));
    const double brightness_offset = (brightness / 100.0) * 255.0;
    const double contrast_255 = contrast * 2.55;
    const double contrast_factor = (259.0 * (contrast_255 + 255.0)) / (255.0 * (259.0 - contrast_255));

    cairo_surface_flush(state->original_surface);
    cairo_surface_flush(target_surface);

    unsigned char* source_data = cairo_image_surface_get_data(state->original_surface);
    unsigned char* target_data = cairo_image_surface_get_data(target_surface);
    const int source_stride = cairo_image_surface_get_stride(state->original_surface);
    const int target_stride = cairo_image_surface_get_stride(target_surface);

    for (int y = 0; y < app_state.canvas_height; ++y) {
        guint32* source_row = reinterpret_cast<guint32*>(source_data + y * source_stride);
        guint32* target_row = reinterpret_cast<guint32*>(target_data + y * target_stride);
        for (int x = 0; x < app_state.canvas_width; ++x) {
            const guint32 pixel = source_row[x];
            const guint8 alpha = (pixel >> 24) & 0xFF;
            const guint8 premul_red = (pixel >> 16) & 0xFF;
            const guint8 premul_green = (pixel >> 8) & 0xFF;
            const guint8 premul_blue = pixel & 0xFF;

            double red = 0.0;
            double green = 0.0;
            double blue = 0.0;
            if (alpha > 0) {
                const double alpha_factor = 255.0 / static_cast<double>(alpha);
                red = std::min(255.0, premul_red * alpha_factor);
                green = std::min(255.0, premul_green * alpha_factor);
                blue = std::min(255.0, premul_blue * alpha_factor);
            }

            red = clamp_double((contrast_factor * (red - 128.0)) + 128.0 + brightness_offset, 0.0, 255.0);
            green = clamp_double((contrast_factor * (green - 128.0)) + 128.0 + brightness_offset, 0.0, 255.0);
            blue = clamp_double((contrast_factor * (blue - 128.0)) + 128.0 + brightness_offset, 0.0, 255.0);

            const double premul_scale = static_cast<double>(alpha) / 255.0;
            const guint8 out_red = static_cast<guint8>(std::lround(red * premul_scale));
            const guint8 out_green = static_cast<guint8>(std::lround(green * premul_scale));
            const guint8 out_blue = static_cast<guint8>(std::lround(blue * premul_scale));

            target_row[x] = (static_cast<guint32>(alpha) << 24) |
                            (static_cast<guint32>(out_red) << 16) |
                            (static_cast<guint32>(out_green) << 8) |
                            static_cast<guint32>(out_blue);
        }
    }

    cairo_surface_mark_dirty(target_surface);
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

void on_layer_color_balance(GtkMenuItem* item, gpointer data) {
    if (app_state.layers.empty() || app_state.active_layer_index < 0 || app_state.active_layer_index >= (int)app_state.layers.size()) {
        return;
    }

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("Color Balance"));
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(app_state.window));
    gtk_window_set_default_size(GTK_WINDOW(window), 320, -1);

    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);
    gtk_container_add(GTK_CONTAINER(window), content);

    ColorBalanceWindowState* state = new ColorBalanceWindowState();
    state->layer_index = app_state.active_layer_index;
    state->original_surface = clone_surface(
        app_state.layers[state->layer_index].surface,
        app_state.canvas_width,
        app_state.canvas_height
    );
    if (!state->original_surface) {
        delete state;
        gtk_widget_destroy(window);
        return;
    }

    auto add_slider_row = [&](const char* label_text, GtkWidget** out_scale) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* label = gtk_label_new(label_text);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -100.0, 100.0, 1.0);
        gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
        gtk_range_set_value(GTK_RANGE(scale), 0.0);
        gtk_widget_set_hexpand(scale, TRUE);

        gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), scale, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
        *out_scale = scale;
    };

    add_slider_row(_("Red"), &state->red_scale);
    add_slider_row(_("Green"), &state->green_scale);
    add_slider_row(_("Blue"), &state->blue_scale);

    GtkWidget* reset_button = gtk_button_new_with_label(_("Reset"));
    gtk_box_pack_start(GTK_BOX(content), reset_button, FALSE, FALSE, 0);

    g_signal_connect(state->red_scale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer user_data) {
        apply_color_balance_from_original(static_cast<ColorBalanceWindowState*>(user_data));
    }), state);
    g_signal_connect(state->green_scale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer user_data) {
        apply_color_balance_from_original(static_cast<ColorBalanceWindowState*>(user_data));
    }), state);
    g_signal_connect(state->blue_scale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer user_data) {
        apply_color_balance_from_original(static_cast<ColorBalanceWindowState*>(user_data));
    }), state);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer user_data) {
        ColorBalanceWindowState* state = static_cast<ColorBalanceWindowState*>(user_data);
        gtk_range_set_value(GTK_RANGE(state->red_scale), 0.0);
        gtk_range_set_value(GTK_RANGE(state->green_scale), 0.0);
        gtk_range_set_value(GTK_RANGE(state->blue_scale), 0.0);
    }), state);

    g_signal_connect(window, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer user_data) {
        ColorBalanceWindowState* state = static_cast<ColorBalanceWindowState*>(user_data);
        if (state->original_surface) {
            cairo_surface_destroy(state->original_surface);
            state->original_surface = nullptr;
        }
        delete state;
    }), state);

    gtk_widget_show_all(window);
}

void on_layer_brightness_contrast(GtkMenuItem* item, gpointer data) {
    if (app_state.layers.empty() || app_state.active_layer_index < 0 || app_state.active_layer_index >= (int)app_state.layers.size()) {
        return;
    }

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("Brightness/Contrast"));
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(app_state.window));
    gtk_window_set_default_size(GTK_WINDOW(window), 320, -1);

    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);
    gtk_container_add(GTK_CONTAINER(window), content);

    BrightnessContrastWindowState* state = new BrightnessContrastWindowState();
    state->layer_index = app_state.active_layer_index;
    state->original_surface = clone_surface(
        app_state.layers[state->layer_index].surface,
        app_state.canvas_width,
        app_state.canvas_height
    );
    if (!state->original_surface) {
        delete state;
        gtk_widget_destroy(window);
        return;
    }

    auto add_slider_row = [&](const char* label_text, GtkWidget** out_scale) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* label = gtk_label_new(label_text);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -100.0, 100.0, 1.0);
        gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
        gtk_range_set_value(GTK_RANGE(scale), 0.0);
        gtk_widget_set_hexpand(scale, TRUE);

        gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), scale, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
        *out_scale = scale;
    };

    add_slider_row(_("Brightness"), &state->brightness_scale);
    add_slider_row(_("Contrast"), &state->contrast_scale);

    GtkWidget* reset_button = gtk_button_new_with_label(_("Reset"));
    gtk_box_pack_start(GTK_BOX(content), reset_button, FALSE, FALSE, 0);

    g_signal_connect(state->brightness_scale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer user_data) {
        apply_brightness_contrast_from_original(static_cast<BrightnessContrastWindowState*>(user_data));
    }), state);
    g_signal_connect(state->contrast_scale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer user_data) {
        apply_brightness_contrast_from_original(static_cast<BrightnessContrastWindowState*>(user_data));
    }), state);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer user_data) {
        BrightnessContrastWindowState* state = static_cast<BrightnessContrastWindowState*>(user_data);
        gtk_range_set_value(GTK_RANGE(state->brightness_scale), 0.0);
        gtk_range_set_value(GTK_RANGE(state->contrast_scale), 0.0);
    }), state);

    g_signal_connect(window, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer user_data) {
        BrightnessContrastWindowState* state = static_cast<BrightnessContrastWindowState*>(user_data);
        if (state->original_surface) {
            cairo_surface_destroy(state->original_surface);
            state->original_surface = nullptr;
        }
        delete state;
    }), state);

    gtk_widget_show_all(window);
}

void on_layer_add(GtkMenuItem* item, gpointer data) {
    add_new_layer();
}

void on_layer_delete(GtkMenuItem* item, gpointer data) {
    delete_layer(app_state.active_layer_index);
}

static bool has_active_layer_surface() {
    return !app_state.layers.empty() &&
           app_state.active_layer_index >= 0 &&
           app_state.active_layer_index < (int)app_state.layers.size() &&
           app_state.surface != nullptr;
}

static void stroke_horizontal_lines(int line_count, int vertical_offset, int spacing) {
    cairo_t* cr = cairo_create(app_state.surface);
    configure_crisp_rendering(cr);
    cairo_set_source_rgba(cr, app_state.fg_color.red, app_state.fg_color.green, app_state.fg_color.blue, app_state.fg_color.alpha);
    cairo_set_line_width(cr, app_state.line_width);

    for (int i = 0; i < line_count; ++i) {
        const double y = vertical_offset + (i * spacing);
        if (y < 0.0 || y > app_state.canvas_height) continue;
        cairo_move_to(cr, 0.0, y);
        cairo_line_to(cr, app_state.canvas_width, y);
    }

    cairo_stroke(cr);
    cairo_destroy(cr);
}

static void stroke_vertical_lines(int line_count, int horizontal_offset, int spacing) {
    cairo_t* cr = cairo_create(app_state.surface);
    configure_crisp_rendering(cr);
    cairo_set_source_rgba(cr, app_state.fg_color.red, app_state.fg_color.green, app_state.fg_color.blue, app_state.fg_color.alpha);
    cairo_set_line_width(cr, app_state.line_width);

    for (int i = 0; i < line_count; ++i) {
        const double x = horizontal_offset + (i * spacing);
        if (x < 0.0 || x > app_state.canvas_width) continue;
        cairo_move_to(cr, x, 0.0);
        cairo_line_to(cr, x, app_state.canvas_height);
    }

    cairo_stroke(cr);
    cairo_destroy(cr);
}

static void queue_canvas_redraw_after_layer_draw() {
    if (!app_state.layers.empty()) {
        app_state.layers[app_state.active_layer_index].surface = app_state.surface;
    }
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

static void clear_line_pattern_preview() {
    app_state.show_line_pattern_preview = false;
    app_state.preview_show_horizontal_lines = false;
    app_state.preview_show_vertical_lines = false;
    app_state.preview_line_count = 0;
    app_state.preview_horizontal_offset = 0;
    app_state.preview_vertical_offset = 0;
    app_state.preview_horizontal_spacing = 0;
    app_state.preview_vertical_spacing = 0;
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

static void update_line_pattern_preview(bool show_horizontal, bool show_vertical, int line_count, int horizontal_offset, int vertical_offset, int horizontal_spacing, int vertical_spacing) {
    app_state.show_line_pattern_preview = true;
    app_state.preview_show_horizontal_lines = show_horizontal;
    app_state.preview_show_vertical_lines = show_vertical;
    app_state.preview_line_count = std::max(0, line_count);
    app_state.preview_horizontal_offset = std::max(0, horizontal_offset);
    app_state.preview_vertical_offset = std::max(0, vertical_offset);
    app_state.preview_horizontal_spacing = std::max(1, horizontal_spacing);
    app_state.preview_vertical_spacing = std::max(1, vertical_spacing);
    if (app_state.drawing_area) {
        gtk_widget_queue_draw(app_state.drawing_area);
    }
}

static void update_horizontal_lines_preview_from_button(GtkWidget* button) {
    GtkWidget* count_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "count-spin"));
    GtkWidget* offset_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "offset-spin"));
    GtkWidget* spacing_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "spacing-spin"));
    update_line_pattern_preview(
        true,
        false,
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin)),
        0,
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(offset_spin)),
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spacing_spin)),
        1
    );
}

static void update_vertical_lines_preview_from_button(GtkWidget* button) {
    GtkWidget* count_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "count-spin"));
    GtkWidget* offset_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "offset-spin"));
    GtkWidget* spacing_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "spacing-spin"));
    update_line_pattern_preview(
        false,
        true,
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin)),
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(offset_spin)),
        0,
        1,
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spacing_spin))
    );
}

static void update_grid_preview_from_button(GtkWidget* button) {
    GtkWidget* count_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "count-spin"));
    GtkWidget* h_offset_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "h-offset-spin"));
    GtkWidget* v_offset_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "v-offset-spin"));

    const int line_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin));
    const int horizontal_offset = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(h_offset_spin));
    const int vertical_offset = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(v_offset_spin));
    const int vertical_spacing = std::max(1, (app_state.canvas_width - horizontal_offset) / std::max(1, line_count));
    const int horizontal_spacing = std::max(1, (app_state.canvas_height - vertical_offset) / std::max(1, line_count));

    update_line_pattern_preview(
        true,
        true,
        line_count,
        horizontal_offset,
        vertical_offset,
        horizontal_spacing,
        vertical_spacing
    );
}

static void draw_line_pattern_preview_overlay(cairo_t* cr) {
    if (!app_state.show_line_pattern_preview || app_state.preview_line_count <= 0) {
        return;
    }

    cairo_save(cr);
    cairo_set_source_rgba(
        cr,
        app_state.fg_color.red,
        app_state.fg_color.green,
        app_state.fg_color.blue,
        std::max(0.25, app_state.fg_color.alpha)
    );
    cairo_set_line_width(cr, app_state.line_width);

    if (app_state.preview_show_horizontal_lines) {
        for (int i = 0; i < app_state.preview_line_count; ++i) {
            const double y = app_state.preview_vertical_offset + (i * app_state.preview_horizontal_spacing);
            if (y < 0.0 || y > app_state.canvas_height) continue;
            cairo_move_to(cr, 0.0, y);
            cairo_line_to(cr, app_state.canvas_width, y);
        }
    }

    if (app_state.preview_show_vertical_lines) {
        for (int i = 0; i < app_state.preview_line_count; ++i) {
            const double x = app_state.preview_horizontal_offset + (i * app_state.preview_vertical_spacing);
            if (x < 0.0 || x > app_state.canvas_width) continue;
            cairo_move_to(cr, x, 0.0);
            cairo_line_to(cr, x, app_state.canvas_height);
        }
    }

    cairo_stroke(cr);
    cairo_restore(cr);
}

void on_layer_draw_horizontal_lines(GtkMenuItem* item, gpointer data) {
    if (!has_active_layer_surface()) return;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Draw Horizontal Lines"),
        GTK_WINDOW(app_state.window),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Draw"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget* count_label = gtk_label_new(_("Number Of Lines:"));
    GtkWidget* offset_label = gtk_label_new(_("Vertical Offset:"));
    GtkWidget* spacing_label = gtk_label_new(_("Spacing:"));
    gtk_widget_set_halign(count_label, GTK_ALIGN_START);
    gtk_widget_set_halign(offset_label, GTK_ALIGN_START);
    gtk_widget_set_halign(spacing_label, GTK_ALIGN_START);

    GtkWidget* count_spin = gtk_spin_button_new_with_range(1, 1000, 1);
    GtkWidget* offset_spin = gtk_spin_button_new_with_range(0, 10000, 1);
    GtkWidget* spacing_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(offset_spin), 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spacing_spin), 25);

    GtkWidget* reset_button = gtk_button_new_with_label(_("Reset"));

    gtk_grid_attach(GTK_GRID(grid), count_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), count_spin, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), offset_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), offset_spin, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spacing_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spacing_spin, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), reset_button, 0, 3, 2, 1);

    g_object_set_data(G_OBJECT(reset_button), "count-spin", count_spin);
    g_object_set_data(G_OBJECT(reset_button), "offset-spin", offset_spin);
    g_object_set_data(G_OBJECT(reset_button), "spacing-spin", spacing_spin);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer user_data) {
        GtkWidget* count_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "count-spin"));
        GtkWidget* offset_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "offset-spin"));
        GtkWidget* spacing_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "spacing-spin"));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), 5);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(offset_spin), 10);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spacing_spin), 25);
    }), NULL);

    g_signal_connect(count_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_horizontal_lines_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);
    g_signal_connect(offset_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_horizontal_lines_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);
    g_signal_connect(spacing_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_horizontal_lines_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);

    update_horizontal_lines_preview_from_button(reset_button);
    gtk_widget_show_all(dialog);
    const int response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK && has_active_layer_surface()) {
        const int line_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin));
        const int vertical_offset = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(offset_spin));
        const int spacing = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spacing_spin));
        push_undo_state();
        stroke_horizontal_lines(line_count, vertical_offset, spacing);
        queue_canvas_redraw_after_layer_draw();
    }

    clear_line_pattern_preview();
    gtk_widget_destroy(dialog);
}

void on_layer_draw_vertical_lines(GtkMenuItem* item, gpointer data) {
    if (!has_active_layer_surface()) return;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Draw Vertical Lines"),
        GTK_WINDOW(app_state.window),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Draw"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget* count_label = gtk_label_new(_("Number Of Lines:"));
    GtkWidget* offset_label = gtk_label_new(_("Horizontal Offset:"));
    GtkWidget* spacing_label = gtk_label_new(_("Spacing:"));
    gtk_widget_set_halign(count_label, GTK_ALIGN_START);
    gtk_widget_set_halign(offset_label, GTK_ALIGN_START);
    gtk_widget_set_halign(spacing_label, GTK_ALIGN_START);

    GtkWidget* count_spin = gtk_spin_button_new_with_range(1, 1000, 1);
    GtkWidget* offset_spin = gtk_spin_button_new_with_range(0, 10000, 1);
    GtkWidget* spacing_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(offset_spin), 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spacing_spin), 25);

    GtkWidget* reset_button = gtk_button_new_with_label(_("Reset"));

    gtk_grid_attach(GTK_GRID(grid), count_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), count_spin, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), offset_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), offset_spin, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spacing_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spacing_spin, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), reset_button, 0, 3, 2, 1);

    g_object_set_data(G_OBJECT(reset_button), "count-spin", count_spin);
    g_object_set_data(G_OBJECT(reset_button), "offset-spin", offset_spin);
    g_object_set_data(G_OBJECT(reset_button), "spacing-spin", spacing_spin);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer user_data) {
        GtkWidget* count_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "count-spin"));
        GtkWidget* offset_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "offset-spin"));
        GtkWidget* spacing_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "spacing-spin"));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), 5);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(offset_spin), 10);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spacing_spin), 25);
    }), NULL);

    g_signal_connect(count_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_vertical_lines_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);
    g_signal_connect(offset_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_vertical_lines_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);
    g_signal_connect(spacing_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_vertical_lines_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);

    update_vertical_lines_preview_from_button(reset_button);
    gtk_widget_show_all(dialog);
    const int response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK && has_active_layer_surface()) {
        const int line_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin));
        const int horizontal_offset = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(offset_spin));
        const int spacing = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spacing_spin));
        push_undo_state();
        stroke_vertical_lines(line_count, horizontal_offset, spacing);
        queue_canvas_redraw_after_layer_draw();
    }

    clear_line_pattern_preview();
    gtk_widget_destroy(dialog);
}

void on_layer_draw_grid(GtkMenuItem* item, gpointer data) {
    if (!has_active_layer_surface()) return;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Draw Grid"),
        GTK_WINDOW(app_state.window),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Draw"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget* count_label = gtk_label_new(_("Number Of Lines:"));
    GtkWidget* h_offset_label = gtk_label_new(_("Horizontal Offset:"));
    GtkWidget* v_offset_label = gtk_label_new(_("Vertical Offset:"));
    gtk_widget_set_halign(count_label, GTK_ALIGN_START);
    gtk_widget_set_halign(h_offset_label, GTK_ALIGN_START);
    gtk_widget_set_halign(v_offset_label, GTK_ALIGN_START);

    GtkWidget* count_spin = gtk_spin_button_new_with_range(1, 1000, 1);
    GtkWidget* h_offset_spin = gtk_spin_button_new_with_range(0, 10000, 1);
    GtkWidget* v_offset_spin = gtk_spin_button_new_with_range(0, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(h_offset_spin), 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v_offset_spin), 10);

    GtkWidget* reset_button = gtk_button_new_with_label(_("Reset"));

    gtk_grid_attach(GTK_GRID(grid), count_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), count_spin, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), h_offset_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), h_offset_spin, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), v_offset_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), v_offset_spin, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), reset_button, 0, 3, 2, 1);

    g_object_set_data(G_OBJECT(reset_button), "count-spin", count_spin);
    g_object_set_data(G_OBJECT(reset_button), "h-offset-spin", h_offset_spin);
    g_object_set_data(G_OBJECT(reset_button), "v-offset-spin", v_offset_spin);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer user_data) {
        GtkWidget* count_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "count-spin"));
        GtkWidget* h_offset_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "h-offset-spin"));
        GtkWidget* v_offset_spin = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "v-offset-spin"));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), 5);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(h_offset_spin), 10);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(v_offset_spin), 10);
    }), NULL);

    g_signal_connect(count_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_grid_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);
    g_signal_connect(h_offset_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_grid_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);
    g_signal_connect(v_offset_spin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer user_data) {
        update_grid_preview_from_button(GTK_WIDGET(user_data));
    }), reset_button);

    update_grid_preview_from_button(reset_button);
    gtk_widget_show_all(dialog);
    const int response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK && has_active_layer_surface()) {
        const int line_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin));
        const int horizontal_offset = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(h_offset_spin));
        const int vertical_offset = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(v_offset_spin));

        const int vertical_spacing = std::max(1, (app_state.canvas_width - horizontal_offset) / std::max(1, line_count));
        const int horizontal_spacing = std::max(1, (app_state.canvas_height - vertical_offset) / std::max(1, line_count));

        push_undo_state();
        stroke_vertical_lines(line_count, horizontal_offset, vertical_spacing);
        stroke_horizontal_lines(line_count, vertical_offset, horizontal_spacing);
        queue_canvas_redraw_after_layer_draw();
    }

    clear_line_pattern_preview();
    gtk_widget_destroy(dialog);
}

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

// Tool button callback
void on_tool_clicked(GtkButton* button, gpointer data) {
    Tool new_tool = (Tool)GPOINTER_TO_INT(data);

    if (new_tool == TOOL_REGULAR_POLYGON && !ask_regular_polygon_sides(app_state.window)) {
        return;
    }
    
    // Cancel text if switching away from text tool (don't finalize empty text)
    if (app_state.text_active && new_tool != TOOL_TEXT) {
        cancel_text();
    }
    
    // Clear or commit selection when switching tools
    if (new_tool != app_state.current_tool) {
        if (new_tool != TOOL_RECT_SELECT && new_tool != TOOL_LASSO_SELECT) {
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

const char* get_palette_button_label(int index, bool is_custom_slot) {
    if (index == 0) {
        return "T";
    }

    if (is_custom_slot) {
        return "c";
    }

    return "";
}

const char* get_tool_icon_filename(Tool tool) {
    switch (tool) {
        case TOOL_LASSO_SELECT: return "stock-tool-free-select.png";
        case TOOL_RECT_SELECT: return "stock-tool-rect-select.png";
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
        case TOOL_ELLIPSE: return "stock_draw-ellipse.png";
        case TOOL_REGULAR_POLYGON: return "stock_draw-pentagon.png";
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

// Create color button
GtkWidget* create_color_button(GdkRGBA color, int index, bool is_custom_slot) {
    GtkWidget* button = gtk_button_new_with_label(get_palette_button_label(index, is_custom_slot));
    gtk_widget_set_size_request(button, 18, 18);

    apply_color_button_style(button, color, is_custom_slot);

    gtk_widget_add_events(button, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(button, "button-press-event", G_CALLBACK(on_color_button_press), GINT_TO_POINTER(index));

    return button;
}

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
    
    GtkWidget* menubar = gtk_menu_bar_new();
    
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

    GtkWidget* layer_menu = gtk_menu_new();
    GtkWidget* layer_menu_item = gtk_menu_item_new_with_label(_("Layer"));
    GtkWidget* layer_add = gtk_menu_item_new_with_label(_("New Layer"));
    GtkWidget* layer_delete = gtk_menu_item_new_with_label(_("Delete Layer"));
    GtkWidget* layer_color_balance = gtk_menu_item_new_with_label(_("Color Balance"));
    GtkWidget* layer_brightness_contrast = gtk_menu_item_new_with_label(_("Brightness/Contrast"));

    GtkWidget* draw_menu = gtk_menu_new();
    GtkWidget* draw_menu_item = gtk_menu_item_new_with_label(_("Draw"));
    GtkWidget* draw_horizontal_lines = gtk_menu_item_new_with_label(_("Draw Horizontal Lines"));
    GtkWidget* draw_vertical_lines = gtk_menu_item_new_with_label(_("Draw Vertical Lines"));
    GtkWidget* draw_grid = gtk_menu_item_new_with_label(_("Draw Grid"));

    g_signal_connect(layer_add, "activate", G_CALLBACK(on_layer_add), NULL);
    g_signal_connect(layer_delete, "activate", G_CALLBACK(on_layer_delete), NULL);
    g_signal_connect(layer_color_balance, "activate", G_CALLBACK(on_layer_color_balance), NULL);
    g_signal_connect(layer_brightness_contrast, "activate", G_CALLBACK(on_layer_brightness_contrast), NULL);
    g_signal_connect(draw_horizontal_lines, "activate", G_CALLBACK(on_layer_draw_horizontal_lines), NULL);
    g_signal_connect(draw_vertical_lines, "activate", G_CALLBACK(on_layer_draw_vertical_lines), NULL);
    g_signal_connect(draw_grid, "activate", G_CALLBACK(on_layer_draw_grid), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), layer_add);
    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), layer_delete);
    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), layer_color_balance);
    gtk_menu_shell_append(GTK_MENU_SHELL(layer_menu), layer_brightness_contrast);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(layer_menu_item), layer_menu);

    gtk_menu_shell_append(GTK_MENU_SHELL(draw_menu), draw_horizontal_lines);
    gtk_menu_shell_append(GTK_MENU_SHELL(draw_menu), draw_vertical_lines);
    gtk_menu_shell_append(GTK_MENU_SHELL(draw_menu), draw_grid);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(draw_menu_item), draw_menu);

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

    GtkWidget* help_menu = gtk_menu_new();
    GtkWidget* help_menu_item = gtk_menu_item_new_with_label(_("Help"));
    GtkWidget* help_manual = gtk_menu_item_new_with_label(_("Contents"));
    GtkWidget* help_about = gtk_menu_item_new_with_label(_("About"));

    g_signal_connect(help_manual, "activate", G_CALLBACK(on_help_manual), NULL);
    g_signal_connect(help_about, "activate", G_CALLBACK(on_help_about), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), help_manual);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), help_about);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_menu_item), help_menu);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), layer_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), draw_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), image_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_menu_item);
    
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
    
    // Create tool buttons with tooltips
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_LASSO_SELECT, 
            _("Lasso Select - Draw freehand selection")), 
        0, 0, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_RECT_SELECT, 
            _("Rectangle Select - Select rectangular regions (Ctrl+C to copy, Ctrl+X to cut)")), 
        1, 0, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_FILL, 
            _("Fill Tool - Fill areas with color")), 
        0, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_GRADIENT_FILL,
            _("Gradient Fill - Click start and end points (hold Ctrl for circular gradient)")),
        0, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_SMUDGE,
            _("Smudge Tool - Drag to smear existing pixels")),
        1, 2, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_EYEDROPPER, 
            _("Eyedropper - Pick color from canvas")), 
        1, 1, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_ERASER, 
            _("Eraser - Erase to transparency")), 
        0, 3, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_ZOOM, 
            _("Zoom Tool - Zoom in/out")), 
        1, 9, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_PENCIL, 
            _("Pencil - Draw thin lines")), 
        0, 4, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_PAINTBRUSH, 
            _("Paintbrush - Draw with brush strokes")), 
        1, 3, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_AIRBRUSH, 
            _("Airbrush - Spray paint effect")), 
        0, 5, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_TEXT, 
            _("Text Tool - Add text (Left-click outside to finalize, Right-click outside to cancel)")), 
        1, 4, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_LINE, 
            _("Line Tool - Draw straight lines (hold Shift for horizontal/vertical)")), 
        0, 6, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_CURVE, 
            _("Curve Tool - Draw curved lines")), 
        1, 5, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_RECTANGLE, 
            _("Rectangle - Draw rectangles")), 
        0, 7, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_POLYGON, 
            _("Polygon - Draw multi-sided shapes")), 
        1, 6, 1, 1);
    
    gtk_grid_attach(GTK_GRID(toolbox), 
        create_tool_button(TOOL_ELLIPSE, 
            _("Ellipse/Circle - Draw ellipses (hold Shift for circles)")), 
        0, 8, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_ROUNDED_RECT,
            _("Rounded Rectangle - Draw rectangles with rounded corners")),
        1, 7, 1, 1);

    gtk_grid_attach(GTK_GRID(toolbox),
        create_tool_button(TOOL_REGULAR_POLYGON,
            _("Polygon Button - Draw regular polygons (asks for 3-50 sides, hold Ctrl for center, Shift for uniform)")),
        0, 9, 1, 1);
    
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
    app_state.merge_layer_button = gtk_button_new_with_label(_("Merge Down"));
    gtk_widget_set_tooltip_text(app_state.merge_layer_button, _("Merge the selected layer into the layer below"));
    g_signal_connect(app_state.merge_layer_button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
        merge_layer_down(app_state.active_layer_index);
    }), NULL);
    gtk_box_pack_start(GTK_BOX(layer_action_row), app_state.add_layer_button, TRUE, TRUE, 0);
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
