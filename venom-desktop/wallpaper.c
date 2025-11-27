/*
 * wallpaper.c
 * Wallpaper and window management
 */

#include "wallpaper.h"
#include <gdk/gdkx.h>

/* Global Variables */
GtkWidget *main_window = NULL;
GtkWidget *icon_layout = NULL;
int screen_w = 0;
int screen_h = 0;

/* --- Drawing Functions --- */

gboolean on_layout_draw_bg(GtkWidget *widget, cairo_t *cr, gpointer data) {
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    return FALSE; 
}

/* --- Window Initialization --- */

void init_main_window(void) {
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "Venom Pro Desktop");
    gtk_window_set_type_hint(GTK_WINDOW(main_window), GDK_WINDOW_TYPE_HINT_DESKTOP);
    
    GdkScreen *screen = gtk_widget_get_screen(main_window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(main_window, visual);
        gtk_widget_set_app_paintable(main_window, TRUE);
    }

    GdkRectangle r;
    gdk_monitor_get_geometry(gdk_display_get_primary_monitor(gdk_display_get_default()), &r);
    screen_w = r.width;
    screen_h = r.height;
    gtk_window_set_default_size(GTK_WINDOW(main_window), screen_w, screen_h);
    gtk_window_move(GTK_WINDOW(main_window), 0, 0);

    icon_layout = gtk_layout_new(NULL, NULL);
    gtk_widget_set_size_request(icon_layout, screen_w, screen_h);
    gtk_widget_set_app_paintable(icon_layout, TRUE);
    
    gtk_widget_add_events(icon_layout, 
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    
    g_signal_connect(icon_layout, "draw", G_CALLBACK(on_layout_draw_bg), NULL);

    gtk_container_add(GTK_CONTAINER(main_window), icon_layout);
}
