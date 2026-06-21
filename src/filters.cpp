#include "filters.h"
#include "utils.h"
#include "undo.h"

guint32 sample_surface_pixel(cairo_surface_t* surface, int x, int y) {
    unsigned char* data = cairo_image_surface_get_data(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    guint32* row = reinterpret_cast<guint32*>(data + y * stride);
    return row[x];
}

guint32 compose_premultiplied_pixel(double red, double green, double blue, double alpha) {
    alpha = clamp_double(alpha, 0.0, 1.0);
    red = clamp_double(red, 0.0, 255.0);
    green = clamp_double(green, 0.0, 255.0);
    blue = clamp_double(blue, 0.0, 255.0);

    const guint8 out_alpha = static_cast<guint8>(std::lround(alpha * 255.0));
    const double premul_factor = static_cast<double>(out_alpha) / 255.0;
    const guint8 out_red = static_cast<guint8>(std::lround(red * premul_factor));
    const guint8 out_green = static_cast<guint8>(std::lround(green * premul_factor));
    const guint8 out_blue = static_cast<guint8>(std::lround(blue * premul_factor));

    return (static_cast<guint32>(out_alpha) << 24) |
           (static_cast<guint32>(out_red) << 16) |
           (static_cast<guint32>(out_green) << 8) |
           static_cast<guint32>(out_blue);
}

void apply_convolution_to_active_layer(const ConvolutionKernel& kernel) {
    if (app_state.layers.empty() || app_state.active_layer_index < 0 || app_state.active_layer_index >= (int)app_state.layers.size()) {
        return;
    }

    cairo_surface_t* target_surface = app_state.layers[app_state.active_layer_index].surface;
    if (!target_surface || kernel.size <= 0 || kernel.values.empty()) {
        return;
    }

    const int width = app_state.canvas_width;
    const int height = app_state.canvas_height;
    cairo_surface_t* source_surface = clone_surface(target_surface, width, height);
    if (!source_surface) {
        return;
    }

    push_undo_state();

    const int radius = kernel.size / 2;
    const double divisor = (std::abs(kernel.divisor) < 1e-9) ? 1.0 : kernel.divisor;

    cairo_surface_flush(source_surface);
    cairo_surface_flush(target_surface);
    unsigned char* target_data = cairo_image_surface_get_data(target_surface);
    const int target_stride = cairo_image_surface_get_stride(target_surface);

    for (int y = 0; y < height; ++y) {
        guint32* target_row = reinterpret_cast<guint32*>(target_data + y * target_stride);
        for (int x = 0; x < width; ++x) {
            double red_sum = 0.0;
            double green_sum = 0.0;
            double blue_sum = 0.0;
            double alpha_sum = 0.0;

            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    const int sx = std::max(0, std::min(width - 1, x + kx));
                    const int sy = std::max(0, std::min(height - 1, y + ky));
                    const double weight = kernel.values[(ky + radius) * kernel.size + (kx + radius)] / divisor;

                    const guint32 pixel = sample_surface_pixel(source_surface, sx, sy);
                    const double alpha = ((pixel >> 24) & 0xFF) / 255.0;
                    const double premul_red = (pixel >> 16) & 0xFF;
                    const double premul_green = (pixel >> 8) & 0xFF;
                    const double premul_blue = pixel & 0xFF;

                    double red = 0.0;
                    double green = 0.0;
                    double blue = 0.0;
                    if (alpha > 0.0) {
                        const double inv_alpha = 1.0 / alpha;
                        red = clamp_double(premul_red * inv_alpha, 0.0, 255.0);
                        green = clamp_double(premul_green * inv_alpha, 0.0, 255.0);
                        blue = clamp_double(premul_blue * inv_alpha, 0.0, 255.0);
                    }

                    red_sum += red * weight;
                    green_sum += green * weight;
                    blue_sum += blue * weight;
                    alpha_sum += alpha * weight;
                }
            }

            target_row[x] = compose_premultiplied_pixel(red_sum, green_sum, blue_sum, alpha_sum);
        }
    }

    cairo_surface_mark_dirty(target_surface);
    cairo_surface_destroy(source_surface);
    gtk_widget_queue_draw(app_state.drawing_area);
}

void apply_cartoonify_to_active_layer() {
    if (app_state.layers.empty() || app_state.active_layer_index < 0 || app_state.active_layer_index >= (int)app_state.layers.size()) {
        return;
    }

    cairo_surface_t* target_surface = app_state.layers[app_state.active_layer_index].surface;
    if (!target_surface) {
        return;
    }

    const int width = app_state.canvas_width;
    const int height = app_state.canvas_height;
    cairo_surface_t* source_surface = clone_surface(target_surface, width, height);
    if (!source_surface) {
        return;
    }

    push_undo_state();
    cairo_surface_flush(source_surface);
    cairo_surface_flush(target_surface);

    unsigned char* source_data = cairo_image_surface_get_data(source_surface);
    unsigned char* target_data = cairo_image_surface_get_data(target_surface);
    const int source_stride = cairo_image_surface_get_stride(source_surface);
    const int target_stride = cairo_image_surface_get_stride(target_surface);

    const int quantization_step = 64;
    const double edge_threshold = 35.0;

    for (int y = 0; y < height; ++y) {
        guint32* target_row = reinterpret_cast<guint32*>(target_data + y * target_stride);
        for (int x = 0; x < width; ++x) {
            auto to_luma = [&](int sx, int sy) {
                guint32* src_row = reinterpret_cast<guint32*>(source_data + sy * source_stride);
                const guint32 pixel = src_row[sx];
                const double alpha = ((pixel >> 24) & 0xFF) / 255.0;
                if (alpha <= 0.0) {
                    return 0.0;
                }
                const double red = ((pixel >> 16) & 0xFF) / alpha;
                const double green = ((pixel >> 8) & 0xFF) / alpha;
                const double blue = (pixel & 0xFF) / alpha;
                return (red * 0.299) + (green * 0.587) + (blue * 0.114);
            };

            guint32* src_row = reinterpret_cast<guint32*>(source_data + y * source_stride);
            const guint32 src_pixel = src_row[x];
            const double alpha = ((src_pixel >> 24) & 0xFF) / 255.0;

            if (alpha <= 0.0) {
                target_row[x] = 0;
                continue;
            }

            const double red = (((src_pixel >> 16) & 0xFF) / alpha);
            const double green = (((src_pixel >> 8) & 0xFF) / alpha);
            const double blue = ((src_pixel & 0xFF) / alpha);

            const double luma_here = to_luma(x, y);
            const double luma_right = to_luma(std::min(width - 1, x + 1), y);
            const double luma_down = to_luma(x, std::min(height - 1, y + 1));
            const bool edge = (std::abs(luma_here - luma_right) > edge_threshold) ||
                              (std::abs(luma_here - luma_down) > edge_threshold);

            if (edge) {
                target_row[x] = compose_premultiplied_pixel(0.0, 0.0, 0.0, alpha);
                continue;
            }

            const auto quantize = [&](double value) {
                return clamp_double(std::floor(value / quantization_step) * quantization_step, 0.0, 255.0);
            };

            target_row[x] = compose_premultiplied_pixel(
                quantize(red),
                quantize(green),
                quantize(blue),
                alpha
            );
        }
    }

    cairo_surface_mark_dirty(target_surface);
    cairo_surface_destroy(source_surface);
    gtk_widget_queue_draw(app_state.drawing_area);
}

void on_filters_noise(GtkMenuItem* item, gpointer data) {
    if (app_state.layers.empty() || app_state.active_layer_index < 0 || app_state.active_layer_index >= (int)app_state.layers.size()) {
        return;
    }

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Noise"),
        GTK_WINDOW(app_state.window),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Apply"), GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(container), 10);
    gtk_container_add(GTK_CONTAINER(content), container);

    GtkWidget* seed_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* seed_label = gtk_label_new(_("Seed:"));
    GtkWidget* seed_spin = gtk_spin_button_new_with_range(0.0, 2147483647.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(seed_spin), 0.0);

    GtkWidget* monochrome_check = gtk_check_button_new_with_label(_("Monochrome"));

    gtk_box_pack_start(GTK_BOX(seed_row), seed_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(seed_row), seed_spin, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(container), seed_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), monochrome_check, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    const int response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    const guint32 seed = static_cast<guint32>(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(seed_spin)));
    const bool monochrome = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(monochrome_check));
    gtk_widget_destroy(dialog);

    cairo_surface_t* target_surface = app_state.layers[app_state.active_layer_index].surface;
    if (!target_surface) {
        return;
    }

    push_undo_state();
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> distribution(0, 255);

    cairo_surface_flush(target_surface);
    unsigned char* target_data = cairo_image_surface_get_data(target_surface);
    const int stride = cairo_image_surface_get_stride(target_surface);

    for (int y = 0; y < app_state.canvas_height; ++y) {
        guint32* row = reinterpret_cast<guint32*>(target_data + y * stride);
        for (int x = 0; x < app_state.canvas_width; ++x) {
            const guint32 pixel = row[x];
            const double alpha = ((pixel >> 24) & 0xFF) / 255.0;

            const int mono_value = distribution(rng);
            const double red = monochrome ? mono_value : distribution(rng);
            const double green = monochrome ? mono_value : distribution(rng);
            const double blue = monochrome ? mono_value : distribution(rng);
            row[x] = compose_premultiplied_pixel(red, green, blue, alpha);
        }
    }

    cairo_surface_mark_dirty(target_surface);
    gtk_widget_queue_draw(app_state.drawing_area);
}

void on_filters_gaussian_blur(GtkMenuItem* item, gpointer data) {
    ConvolutionKernel kernel;
    kernel.values = {
        1, 4, 6, 4, 1,
        4, 16, 24, 16, 4,
        6, 24, 36, 24, 6,
        4, 16, 24, 16, 4,
        1, 4, 6, 4, 1
    };
    kernel.size = 5;
    kernel.divisor = 256.0;
    apply_convolution_to_active_layer(kernel);
}

void on_filters_blur(GtkMenuItem* item, gpointer data) {
    ConvolutionKernel kernel;
    kernel.values = {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1
    };
    kernel.size = 3;
    kernel.divisor = 9.0;
    apply_convolution_to_active_layer(kernel);
}

void on_filters_sharpen(GtkMenuItem* item, gpointer data) {
    ConvolutionKernel kernel;
    kernel.values = {
         0, -1,  0,
        -1,  5, -1,
         0, -1,  0
    };
    kernel.size = 3;
    kernel.divisor = 1.0;
    apply_convolution_to_active_layer(kernel);
}

void on_filters_cartoonify(GtkMenuItem* item, gpointer data) {
    apply_cartoonify_to_active_layer();
}

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

    const double brightness_offset = gtk_range_get_value(GTK_RANGE(state->brightness_scale));
    const double contrast = gtk_range_get_value(GTK_RANGE(state->contrast_scale));
    const double contrast_factor = (259.0 * (contrast + 255.0)) / (255.0 * (259.0 - contrast));

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
        GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -127.0, 127.0, 1.0);
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

// --- Static helpers for line pattern drawing ---

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

void draw_line_pattern_preview_overlay(cairo_t* cr) {
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
