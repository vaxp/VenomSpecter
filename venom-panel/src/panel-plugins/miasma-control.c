#include <gtk/gtk.h>
#include <gio/gio.h>
#include "../../include/venom-panel-plugin-api.h"

static GDBusConnection *dbus_conn = NULL;

static void send_opts_set_int(const char *opt, int val) {
    if (!dbus_conn) return;
    GError *error = NULL;
    g_dbus_connection_call_sync(dbus_conn,
        "org.venomlinux.miasma._0",
        "/org/venomlinux/miasma",
        "org.venomlinux.miasma",
        "opts_set",
        g_variant_new("(si)", opt, val),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (error) {
        g_warning("Miasma Plugin: Failed to set %s: %s", opt, error->message);
        g_error_free(error);
    }
}

static void send_opts_set_double(const char *opt, double val) {
    if (!dbus_conn) return;
    GError *error = NULL;
    g_dbus_connection_call_sync(dbus_conn,
        "org.venomlinux.miasma._0",
        "/org/venomlinux/miasma",
        "org.venomlinux.miasma",
        "opts_set",
        g_variant_new("(sd)", opt, val),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (error) {
        g_warning("Miasma Plugin: Failed to set %s: %s", opt, error->message);
        g_error_free(error);
    }
}

static void send_effect_set(const char *effect, gboolean active) {
    if (!dbus_conn) return;
    GError *error = NULL;
    g_dbus_connection_call_sync(dbus_conn,
        "org.venomlinux.miasma._0",
        "/org/venomlinux/miasma",
        "org.venomlinux.miasma",
        "effect_set",
        g_variant_new("(sb)", effect, active),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (error) {
        g_warning("Miasma Plugin: Failed to set effect %s: %s", effect, error->message);
        g_error_free(error);
    }
}

static void on_shadow_radius_changed(GtkRange *range, gpointer data) {
    int val = (int)gtk_range_get_value(range);
    send_opts_set_int("shadow_radius", val);
}

static void on_blur_strength_changed(GtkRange *range, gpointer data) {
    int val = (int)gtk_range_get_value(range);
    send_opts_set_int("blur_strength", val);
}

static void on_shadow_opacity_changed(GtkRange *range, gpointer data) {
    double val = gtk_range_get_value(range);
    send_opts_set_double("shadow_opacity", val);
}

static void on_shadow_red_changed(GtkRange *range, gpointer data) {
    double val = gtk_range_get_value(range);
    send_opts_set_double("shadow_red", val);
}

static void on_shadow_green_changed(GtkRange *range, gpointer data) {
    double val = gtk_range_get_value(range);
    send_opts_set_double("shadow_green", val);
}

static void on_shadow_blue_changed(GtkRange *range, gpointer data) {
    double val = gtk_range_get_value(range);
    send_opts_set_double("shadow_blue", val);
}

static void on_effect_toggled(GtkSwitch *sw, GParamSpec *pspec, gpointer data) {
    const char *effect = (const char *)data;
    gboolean active = gtk_switch_get_active(sw);
    send_effect_set(effect, active);
}

static GtkWidget* create_scale(const char *label_text, double min, double max, double step, double initial, GCallback callback) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_range_set_value(GTK_RANGE(scale), initial);
    gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
    g_signal_connect(scale, "value-changed", callback, NULL);
    
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    return box;
}

static GtkWidget* create_effect_switch(const char *label_text, const char *effect_name) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    
    GtkWidget *sw = gtk_switch_new();
    gtk_widget_set_halign(sw, GTK_ALIGN_END);
    g_signal_connect(sw, "notify::active", G_CALLBACK(on_effect_toggled), (gpointer)effect_name);
    
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), sw, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *miasma_window = NULL;

static void create_miasma_window(void) {
    miasma_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    /* Activate Transparency */
    GdkScreen *screen = gtk_widget_get_screen(miasma_window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual != NULL && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(miasma_window, visual);
        gtk_widget_set_app_paintable(miasma_window, TRUE);
    }

    gtk_window_set_title(GTK_WINDOW(miasma_window), "Miasma Control");
    gtk_window_set_decorated(GTK_WINDOW(miasma_window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(miasma_window), 320, 480);
    gtk_window_set_type_hint(GTK_WINDOW(miasma_window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(miasma_window), TRUE);

    /* Positioning logic like control-center */
    gint screen_width = gdk_screen_get_width(screen);
    gtk_window_move(GTK_WINDOW(miasma_window), screen_width - 340, 40);

    /* Add CSS class that matches control center */
    GtkStyleContext *win_ctx = gtk_widget_get_style_context(miasma_window);
    gtk_style_context_add_class(win_ctx, "control-center");

    /* Hide on close request */
    g_signal_connect(miasma_window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 15);
    gtk_widget_set_margin_end(vbox, 15);
    gtk_widget_set_margin_top(vbox, 15);
    gtk_widget_set_margin_bottom(vbox, 15);

    // Title
    GtkWidget *title = gtk_label_new("<span weight='bold' size='large'>Miasma Compositor</span>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);
    GtkStyleContext *title_ctx = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(title_ctx, "section-header");
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    // Sliders
    gtk_box_pack_start(GTK_BOX(vbox), create_scale("Blur Strength", 0, 20, 1, 5, G_CALLBACK(on_blur_strength_changed)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_scale("Shadow Radius", 0, 100, 1, 12, G_CALLBACK(on_shadow_radius_changed)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_scale("Shadow Opacity", 0.0, 1.0, 0.05, 0.75, G_CALLBACK(on_shadow_opacity_changed)), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    // Shadow Colors
    GtkWidget *color_label = gtk_label_new("<span weight='bold'>Shadow Colors</span>");
    gtk_label_set_use_markup(GTK_LABEL(color_label), TRUE);
    gtk_widget_set_halign(color_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), color_label, FALSE, FALSE, 5);

    gtk_box_pack_start(GTK_BOX(vbox), create_scale("Red", 0.0, 1.0, 0.05, 0.0, G_CALLBACK(on_shadow_red_changed)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_scale("Green", 0.0, 1.0, 0.05, 0.0, G_CALLBACK(on_shadow_green_changed)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_scale("Blue", 0.0, 1.0, 0.05, 0.0, G_CALLBACK(on_shadow_blue_changed)), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    // Effects
    GtkWidget *eff_label = gtk_label_new("<span weight='bold'>Visualizer Effects</span>");
    gtk_label_set_use_markup(GTK_LABEL(eff_label), TRUE);
    gtk_widget_set_halign(eff_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), eff_label, FALSE, FALSE, 5);

    gtk_box_pack_start(GTK_BOX(vbox), create_effect_switch("Pulse (Beat-reactive)", "pulse"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_effect_switch("Flash (Vibrant colors)", "flash"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_effect_switch("Neon (Rotating aura)", "neon"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_effect_switch("Chromatic (Glitch)", "chromatic"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_effect_switch("Shockwave (Bass drops)", "shockwave"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_effect_switch("Pixelation (High energy)", "pixelation"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_effect_switch("Radial Zoom (Snare hits)", "radial_zoom"), FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(miasma_window), vbox);
    gtk_widget_show_all(vbox);
}

static void toggle_miasma_window(GtkWidget *button, gpointer data) {
    if (miasma_window && gtk_widget_get_visible(miasma_window)) {
        gtk_widget_hide(miasma_window);
    } else {
        if (!miasma_window) {
            create_miasma_window();
        }
        
        /* Re-position logic to handle screen resolution changes */
        GdkScreen *screen = gtk_widget_get_screen(miasma_window);
        gint screen_width = gdk_screen_get_width(screen);
        gtk_window_move(GTK_WINDOW(miasma_window), screen_width - 340, 40);

        gtk_widget_show_all(miasma_window);
        gtk_window_present(GTK_WINDOW(miasma_window));
    }
}

static GtkWidget* create_miasma_widget(void) {
    if (!dbus_conn) {
        GError *error = NULL;
        dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
        if (error) {
            g_warning("Miasma Plugin: Failed to connect to session bus: %s", error->message);
            g_error_free(error);
        }
    }

    GtkWidget *button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(button, "Miasma Compositor Settings");
    
    GtkWidget *icon = NULL;
    GError *err = NULL;
    /* Try to load logo.png next to the running panel binary (dev-friendly),
     * otherwise fallback to an icon-theme symbol. */
    char *exe = g_file_read_link("/proc/self/exe", NULL);
    char *exe_dir = exe ? g_path_get_dirname(exe) : NULL;
    char *logo_path = exe_dir ? g_build_filename(exe_dir, "logo.png", NULL) : NULL;
    GdkPixbuf *pixbuf = logo_path ? gdk_pixbuf_new_from_file_at_scale(logo_path, 22, 22, TRUE, &err) : NULL;
    if (pixbuf) {
        icon = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
    } else {
        if (err) {
            g_warning("Miasma Plugin: Failed to load logo: %s", err->message);
            g_error_free(err);
        }
        icon = gtk_image_new_from_icon_name("preferences-desktop-display", GTK_ICON_SIZE_LARGE_TOOLBAR);
    }
    g_free(logo_path);
    g_free(exe_dir);
    g_free(exe);
    gtk_container_add(GTK_CONTAINER(button), icon);
    
    GtkStyleContext *ctx = gtk_widget_get_style_context(button);
    gtk_style_context_add_class(ctx, "control-center-button");

    g_signal_connect(button, "clicked", G_CALLBACK(toggle_miasma_window), NULL);

    gtk_widget_show_all(button);
    return button;
}

VenomPanelPluginAPI* venom_panel_plugin_init(void) {
    static VenomPanelPluginAPI api;
    api.name        = "Miasma Control";
    api.description = "Control center for Venom Miasma compositor";
    api.author      = "Venom Core";
    api.zone        = VENOM_PLUGIN_ZONE_RIGHT;
    api.priority    = 10;
    api.expand      = FALSE;
    api.padding     = 0;
    api.create_widget = create_miasma_widget;
    return &api;
}

VenomPanelPluginAPIv2* venom_panel_plugin_init_v2(void) {
    static VenomPanelPluginAPIv2 api = {
        .api_version = VENOM_PANEL_PLUGIN_API_VERSION,
        .struct_size = sizeof(VenomPanelPluginAPIv2),
        .name = "Miasma Control",
        .description = "Control center for Venom Miasma compositor",
        .author = "Venom Core",
        .zone = VENOM_PLUGIN_ZONE_RIGHT,
        .priority = 10,
        .expand = FALSE,
        .padding = 0,
        .create_widget = create_miasma_widget,
        .destroy_widget = NULL,
    };
    return &api;
}
