#include "venom-panel.h"
#include "clock-widget.h"
#include "system-icons.h"
#include "control-center.h"
#include <gdk/gdkx.h>

static GtkWidget *control_center_window = NULL;

static void toggle_control_center(GtkWidget *button, gpointer data) {
    (void)button;
    (void)data;
    
    if (control_center_window && gtk_widget_get_visible(control_center_window)) {
        gtk_widget_hide(control_center_window);
    } else {
        if (!control_center_window) {
            control_center_window = create_control_center();
        }
        gtk_widget_show_all(control_center_window);
    }
}

GtkWidget* create_venom_panel(void) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    /* ✅ بداية التعديل: تفعيل الشفافية للنافذة */
    GdkScreen *screen = gtk_widget_get_screen(window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    
    if (visual != NULL && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(window, visual);
        gtk_widget_set_app_paintable(window, TRUE);
    }
    /* ✅ نهاية التعديل */

    gtk_window_set_title(GTK_WINDOW(window), "venom Panel");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DOCK);
    
    /* Set window properties for panel */
    gtk_window_set_default_size(GTK_WINDOW(window), 1620, 32);
    
    /* Make it stay on top and on all workspaces */
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_window_stick(GTK_WINDOW(window));
    
    /* Position at top */
    gtk_window_move(GTK_WINDOW(window), 0, 0);
    
    /* Create horizontal box for panel items */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), hbox);
    
    /* Add spacing on the left */
    GtkWidget *left_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(left_spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), left_spacer, TRUE, TRUE, 0);
    
    /* Right side container */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_end(right_box, 12);
    gtk_box_pack_end(GTK_BOX(hbox), right_box, FALSE, FALSE, 0);
    
    /* Add system icons */
    GtkWidget *icons = create_system_icons();
    gtk_box_pack_start(GTK_BOX(right_box), icons, FALSE, FALSE, 0);
    
    /* Add clock */
    GtkWidget *clock = create_clock_widget();
    gtk_box_pack_start(GTK_BOX(right_box), clock, FALSE, FALSE, 0);
    
    /* Add control center button */
    GtkWidget *cc_button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(cc_button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(cc_button, "Control Center");
    
    GtkWidget *cc_icon = gtk_image_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_container_add(GTK_CONTAINER(cc_button), cc_icon);
    
    /* Add CSS class for styling */
    GtkStyleContext *btn_ctx = gtk_widget_get_style_context(cc_button);
    gtk_style_context_add_class(btn_ctx, "control-center-button");
    
    g_signal_connect(cc_button, "clicked", G_CALLBACK(toggle_control_center), NULL);
    gtk_box_pack_start(GTK_BOX(right_box), cc_button, FALSE, FALSE, 0);
    
    /* Load CSS */
    GtkCssProvider *provider = gtk_css_provider_new();
    // GdkDisplay *display = gdk_display_get_default(); // تم إخفاؤه لأنه غير مستخدم هنا مباشرة
    GdkScreen *default_screen = gdk_display_get_default_screen(gdk_display_get_default());
    
    gchar *css_path = g_build_filename(g_get_current_dir(), "style.css", NULL);
    gtk_css_provider_load_from_path(provider, css_path, NULL);
    g_free(css_path);
    
    gtk_style_context_add_provider_for_screen(default_screen,
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    /* Add CSS class */
    GtkStyleContext *context = gtk_widget_get_style_context(window);
    gtk_style_context_add_class(context, "venom-panel");
    
    return window;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    GtkWidget *panel = create_venom_panel();
    
    g_signal_connect(panel, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_widget_show_all(panel);
    gtk_main();
    
    return 0;
}