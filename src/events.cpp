#include "events.h"
#include "utils.h"
#include "drawing.h"
#include "selection.h"
#include "text_tool.h"
#include "undo.h"
#include "rendering.h"
#include "layers.h"
#include "ui_widgets.h"
#include "image_ops.h"
#include "palette.h"

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

        if (app_state.current_tool == TOOL_CROP && app_state.has_selection) {
            if (point_in_selection(canvas_x, canvas_y)) {
                if (event->button == 1) {
                    crop_to_rectangle(
                        app_state.selection_x1,
                        app_state.selection_y1,
                        app_state.selection_x2,
                        app_state.selection_y2
                    );
                }
                return TRUE;
            }

            clear_selection();
            gtk_widget_queue_draw(widget);
            return TRUE;
        }

        if (tool_is_selection_tool(app_state.current_tool) &&
            app_state.current_tool != TOOL_CROP &&
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

        if (app_state.current_tool == TOOL_SELECT_BY_COLOR || app_state.current_tool == TOOL_FUZZY_SELECT) {
            if (event->button == 1) {
                bool contiguous_only = app_state.current_tool == TOOL_FUZZY_SELECT;
                int tolerance = contiguous_only ? 32 : 0;
                select_pixels_by_color(static_cast<int>(canvas_x), static_cast<int>(canvas_y), contiguous_only, tolerance);
                gtk_widget_queue_draw(widget);
            }
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
            app_state.current_tool == TOOL_REGULAR_POLYGON || app_state.current_tool == TOOL_STAR || app_state.current_tool == TOOL_ROUNDED_RECT) {
            push_undo_state();
        }
        app_state.last_x = canvas_x;
        app_state.last_y = canvas_y;
        app_state.start_x = canvas_x;
        app_state.start_y = canvas_y;
        app_state.current_x = canvas_x;
        app_state.current_y = canvas_y;
        if (app_state.current_tool == TOOL_REGULAR_POLYGON || app_state.current_tool == TOOL_STAR) {
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

        if (app_state.current_tool == TOOL_REGULAR_POLYGON || app_state.current_tool == TOOL_STAR) {
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
                       app_state.current_tool == TOOL_RECT_SELECT ||
                       app_state.current_tool == TOOL_CROP) {
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
            case TOOL_STAR: {
                std::vector<std::pair<double, double>> points;
                build_star_points(
                    app_state.start_x,
                    app_state.start_y,
                    end_x,
                    end_y,
                    ((event->state & GDK_CONTROL_MASK) != 0) || app_state.ctrl_pressed,
                    app_state.shift_pressed,
                    app_state.star_points,
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
            case TOOL_CROP:
                app_state.has_selection = true;
                app_state.selection_is_rect = true;
                app_state.floating_selection_active = false;
                app_state.selection_has_mask = false;
                app_state.selection_mask.clear();
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
