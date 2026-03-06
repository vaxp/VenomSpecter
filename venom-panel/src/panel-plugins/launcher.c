#include <gtk/gtk.h>
#include <stdlib.h>
#include "../../include/venom-panel-plugin-api.h"

/* Launcher entries */
typedef struct {
    const char *icon;
    const char *tooltip;
    const char *command;
} LauncherEntry;

static const LauncherEntry LAUNCHERS[] = {
    { "utilities-terminal",          "Terminal",     "x-terminal-emulator"      },
    { "system-file-manager",         "Files",        "xdg-open ~"               },
    { "firefox",                     "Firefox",      "firefox"                  },
    { "text-editor",                 "Text Editor",  "xdg-open ."               },
};

static void on_launch_clicked(GtkButton *btn, gpointer data) {
    const char *cmd = (const char *)data;
    if (cmd) g_spawn_command_line_async(cmd, NULL);
}

static GtkWidget* create_launcher_widget(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_margin_start(box, 8);

    /* Apply subtle padding so icons sit centered vertically in 40px bar */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "button { padding: 2px 4px; border-radius: 6px; }"
        "button:hover { background-color: rgba(255,255,255,0.12); }", -1, NULL);

    for (guint i = 0; i < G_N_ELEMENTS(LAUNCHERS); i++) {
        const LauncherEntry *e = &LAUNCHERS[i];

        GtkWidget *btn = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text(btn, e->tooltip);

        GtkWidget *img = gtk_image_new_from_icon_name(e->icon, GTK_ICON_SIZE_MENU);
        gtk_container_add(GTK_CONTAINER(btn), img);

        GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
        gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(css),
                                       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        /* Store command string as static data (safe for lifetime of process) */
        g_signal_connect(btn, "clicked", G_CALLBACK(on_launch_clicked),
                         (gpointer)e->command);

        gtk_box_pack_start(GTK_BOX(box), btn, FALSE, FALSE, 0);
    }
    g_object_unref(css);

    gtk_widget_show_all(box);
    return box;
}

VenomPanelPluginAPI* venom_panel_plugin_init(void) {
    static VenomPanelPluginAPI api;
    api.name        = "App Launcher";
    api.description = "Quick-launch buttons on the left of the panel.";
    api.author      = "Venom Core";
    api.zone        = VENOM_PLUGIN_ZONE_LEFT;
    api.priority    = 0;      /* pack_start, closest to the left edge */
    api.create_widget = create_launcher_widget;
    return &api;
}
