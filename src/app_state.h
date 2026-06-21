#pragma once

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
#include <random>

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
    TOOL_SELECT_BY_COLOR,
    TOOL_FUZZY_SELECT,
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
    TOOL_CROP,
    TOOL_ELLIPSE,
    TOOL_REGULAR_POLYGON,
    TOOL_STAR,
    TOOL_ROUNDED_RECT,
    TOOL_COUNT
};

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
    int star_points = 5;

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
    std::vector<bool> selection_mask;
    bool selection_has_mask = false;
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
    GtkWidget* duplicate_layer_button = nullptr;
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

extern AppState app_state;

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

struct SelectionPixelBounds {
    int x;
    int y;
    int width;
    int height;
};

struct ConvolutionKernel {
    std::vector<double> values;
    int size = 0;
    double divisor = 1.0;
};

struct ColorBalanceWindowState {
    cairo_surface_t* original_surface = nullptr;
    GtkWidget* red_scale = nullptr;
    GtkWidget* green_scale = nullptr;
    GtkWidget* blue_scale = nullptr;
    int layer_index = -1;
    bool undo_pushed = false;
};

struct BrightnessContrastWindowState {
    cairo_surface_t* original_surface = nullptr;
    GtkWidget* brightness_scale = nullptr;
    GtkWidget* contrast_scale = nullptr;
    int layer_index = -1;
    bool undo_pushed = false;
};
