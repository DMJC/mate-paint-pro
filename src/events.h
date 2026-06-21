#pragma once

#include "app_state.h"

gboolean on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer data);
gboolean on_key_release(GtkWidget* widget, GdkEventKey* event, gpointer data);
gboolean on_button_press(GtkWidget* widget, GdkEventButton* event, gpointer data);
gboolean on_motion_notify(GtkWidget* widget, GdkEventMotion* event, gpointer data);
gboolean on_leave_notify(GtkWidget* widget, GdkEventCrossing* event, gpointer data);
gboolean on_button_release(GtkWidget* widget, GdkEventButton* event, gpointer data);
