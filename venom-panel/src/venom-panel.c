#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <dlfcn.h>
#include <glib-unix.h>
#include <sys/stat.h>
#include <unistd.h>

/* Headers (Assumed present in your project) */
#include "venom-panel.h"
#include "clock-widget.h"
#include "system-icons.h"
#include "control-center.h"
#include "venom-panel-plugin-api.h"

#include "sni-client.h"
#include "shot-client.h"
#include "power-client.h"

static GtkWidget *control_center_window = NULL;



static GtkWidget *rec_stop_button = NULL;
static GtkWidget *power_profile_btn = NULL;
static GtkWidget *power_profile_label = NULL;
static GtkWidget *power_actions_box = NULL;

typedef struct {
    GtkWidget *tray_box;
} TrayUI;

static TrayUI g_tray_ui = {0};

static void set_widget_weak(GtkWidget **slot, GtkWidget *widget) {
    if (*slot) g_object_remove_weak_pointer(G_OBJECT(*slot), (gpointer *)slot);
    *slot = widget;
    if (widget) g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)slot);
}



/* =====================================================================
 * 0. CONFIG-DRIVEN LAYOUT ENGINE
 * ===================================================================== */

static char* panel_config_dir(void) {
    return g_build_filename(g_get_user_config_dir(), "venom", NULL);
}

static char* panel_config_file(void) {
    return g_build_filename(g_get_user_config_dir(), "venom", "panel.conf", NULL);
}

static char* panel_plugins_dir(void) {
    return g_build_filename(g_get_user_config_dir(), "venom", "panel-plugins", NULL);
}

typedef enum {
    ITEM_TYPE_PLUGIN,
    ITEM_TYPE_BUILTIN,
    ITEM_TYPE_SPACER,
    ITEM_TYPE_SEPARATOR,
} PanelItemType;

typedef struct {
    PanelItemType  type;
    char           file[256];
    char           name[64];
    gboolean       expand;
    int            padding;
} PanelItem;

static void write_default_config(void) {
    char *cfg_dir = panel_config_dir();
    char *cfg_file = panel_config_file();
    g_mkdir_with_parents(cfg_dir, 0755);
    const char *cfg =
        "# venom-panel layout — edit and restart to apply changes\n"
        "# Types: plugin, builtin, spacer, separator\n"
        "# Builtins: tray, power, system-icons, clock, control-center\n"
        "\n"
        "[item]\ntype=plugin\nfile=launcher.so\nexpand=false\npadding=4\n"
        "\n"
        "[item]\ntype=spacer\nexpand=true\npadding=0\n"
        "\n"
        "[item]\ntype=builtin\nname=tray\nexpand=false\npadding=4\n"
        "\n"
        "[item]\ntype=separator\n"
        "\n"
        "[item]\ntype=builtin\nname=power\nexpand=false\npadding=2\n"
        "\n"
        "[item]\ntype=separator\n"
        "\n"
        "[item]\ntype=builtin\nname=system-icons\nexpand=false\npadding=2\n"
        "\n"
        "[item]\ntype=builtin\nname=clock\nexpand=false\npadding=4\n"
        "\n"
        "[item]\ntype=separator\n"
        "\n"
        "[item]\ntype=builtin\nname=control-center\nexpand=false\npadding=2\n";
    g_file_set_contents(cfg_file, cfg, -1, NULL);
    g_print("[Panel] Created default config at %s\n", cfg_file);
    g_free(cfg_dir);
    g_free(cfg_file);
}

static GArray* parse_panel_config(void) {
    char *cfg_file = panel_config_file();
    if (!g_file_test(cfg_file, G_FILE_TEST_EXISTS))
        write_default_config();

    GArray *items = g_array_new(FALSE, TRUE, sizeof(PanelItem));
    char *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(cfg_file, &contents, &length, NULL)) {
        g_warning("[Panel] Could not read %s", cfg_file);
        g_free(cfg_file);
        return items;
    }
    g_free(cfg_file);

    PanelItem cur;
    memset(&cur, 0, sizeof(cur));
    cur.expand = FALSE; cur.padding = 2;
    gboolean in_item = FALSE;

    char **lines = g_strsplit(contents, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        char *line = g_strstrip(lines[i]);
        if (line[0] == '#' || line[0] == '\0') continue;
        if (g_strcmp0(line, "[item]") == 0) {
            if (in_item) g_array_append_val(items, cur);
            memset(&cur, 0, sizeof(cur));
            cur.expand = FALSE; cur.padding = 2;
            in_item = TRUE; continue;
        }
        if (!in_item) continue;
        char **kv = g_strsplit(line, "=", 2);
        if (!kv[0] || !kv[1]) { g_strfreev(kv); continue; }
        char *k = g_strstrip(kv[0]), *v = g_strstrip(kv[1]);
        if      (!g_strcmp0(k,"type"))    {
            if      (!g_strcmp0(v,"plugin"))    cur.type = ITEM_TYPE_PLUGIN;
            else if (!g_strcmp0(v,"builtin"))   cur.type = ITEM_TYPE_BUILTIN;
            else if (!g_strcmp0(v,"spacer"))    cur.type = ITEM_TYPE_SPACER;
            else if (!g_strcmp0(v,"separator")) cur.type = ITEM_TYPE_SEPARATOR;
        }
        else if (!g_strcmp0(k,"file"))    strncpy(cur.file, v, sizeof(cur.file)-1);
        else if (!g_strcmp0(k,"name"))    strncpy(cur.name, v, sizeof(cur.name)-1);
        else if (!g_strcmp0(k,"expand"))  cur.expand  = !g_strcmp0(v,"true");
        else if (!g_strcmp0(k,"padding")) {
            cur.padding = atoi(v);
            if (cur.padding < 0) cur.padding = 0;
            if (cur.padding > 128) cur.padding = 128;
        }
        g_strfreev(kv);
    }
    if (in_item) g_array_append_val(items, cur);
    g_strfreev(lines);
    g_free(contents);
    return items;
}

/* Forward declarations for builtin builders (defined after create_venom_panel) */
static GtkWidget* build_builtin_tray(GtkWidget *hbox);
static GtkWidget* build_builtin_power(void);
static GtkWidget* build_builtin_system_icons(void);
static GtkWidget* build_builtin_clock(void);
static GtkWidget* build_builtin_control_center(void);

typedef struct {
    const VenomPanelPluginAPIv2 *api_v2;
    gboolean destroyed;
} PanelPluginInstance;

static GHashTable *g_plugin_handles = NULL;

static gboolean plugin_is_safe_basename(const char *name) {
    if (!name || !name[0]) return FALSE;
    if (g_path_is_absolute(name)) return FALSE;
    if (strchr(name, G_DIR_SEPARATOR)) return FALSE;
    if (g_strrstr(name, "..")) return FALSE;
    if (!g_str_has_suffix(name, ".so")) return FALSE;
    return TRUE;
}

static gboolean path_is_regular_nosymlink(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return FALSE;
    if (S_ISLNK(st.st_mode)) return FALSE;
    return S_ISREG(st.st_mode) ? TRUE : FALSE;
}

static void plugin_instance_destroy(PanelPluginInstance *inst, GtkWidget *widget) {
    if (!inst || inst->destroyed) return;
    inst->destroyed = TRUE;
    if (inst->api_v2 && inst->api_v2->destroy_widget) inst->api_v2->destroy_widget(widget);
}

static void on_plugin_widget_destroy(GtkWidget *widget, gpointer user_data) {
    plugin_instance_destroy((PanelPluginInstance *)user_data, widget);
}

static void plugin_instance_free(gpointer data) {
    PanelPluginInstance *inst = (PanelPluginInstance *)data;
    if (!inst) return;
    g_free(inst);
}

static void load_items_from_config(GtkWidget *hbox) {
    GArray *items = parse_panel_config();
    char *plugins_dir = panel_plugins_dir();
    g_mkdir_with_parents(plugins_dir, 0755);

    for (guint i = 0; i < items->len; i++) {
        PanelItem *item = &g_array_index(items, PanelItem, i);
        GtkWidget *widget = NULL;
        gboolean expand  = item->expand;
        int      padding = item->padding;

        switch (item->type) {
            case ITEM_TYPE_SPACER:
                widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                expand = TRUE;
                break;
            case ITEM_TYPE_SEPARATOR:
                widget = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
                gtk_widget_set_margin_top(widget, 6);
                gtk_widget_set_margin_bottom(widget, 6);
                expand = FALSE; padding = 4;
                break;
            case ITEM_TYPE_BUILTIN:
                if      (!g_strcmp0(item->name,"tray"))           widget = build_builtin_tray(hbox);
                else if (!g_strcmp0(item->name,"power"))          widget = build_builtin_power();
                else if (!g_strcmp0(item->name,"system-icons"))   widget = build_builtin_system_icons();
                else if (!g_strcmp0(item->name,"clock"))          widget = build_builtin_clock();
                else if (!g_strcmp0(item->name,"control-center")) widget = build_builtin_control_center();
                else g_warning("[Panel] Unknown builtin: '%s'", item->name);
                break;
            case ITEM_TYPE_PLUGIN: {
                if (!plugin_is_safe_basename(item->file)) {
                    g_warning("[Panel] Refusing plugin path '%s' (must be a basename ending with .so)", item->file);
                    GtkWidget *warn = gtk_image_new_from_icon_name("dialog-warning-symbolic", GTK_ICON_SIZE_MENU);
                    gtk_widget_set_tooltip_text(warn, "Invalid plugin filename in config");
                    widget = warn;
                    break;
                }

                char *fp = g_build_filename(plugins_dir, item->file, NULL);
                if (!path_is_regular_nosymlink(fp)) {
                    g_warning("[Panel] Refusing plugin '%s' (not a regular file or is a symlink)", item->file);
                    GtkWidget *warn = gtk_image_new_from_icon_name("dialog-warning-symbolic", GTK_ICON_SIZE_MENU);
                    char *tip = g_strdup_printf("Plugin is not a regular file: %s", item->file);
                    gtk_widget_set_tooltip_text(warn, tip);
                    g_free(tip);
                    widget = warn;
                    g_free(fp);
                    break;
                }

                gboolean opened_new = FALSE;
                void *h = NULL;
                if (!g_plugin_handles) g_plugin_handles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
                h = g_hash_table_lookup(g_plugin_handles, fp);
                if (!h) {
                    opened_new = TRUE;
                    h = dlopen(fp, RTLD_NOW | RTLD_LOCAL);
                }
                if (!h) { 
                    g_warning("[Panel] dlopen %s: %s", item->file, dlerror());
                    /* Graceful fallback: show a warning icon instead of crashing */
                    GtkWidget *warn = gtk_image_new_from_icon_name("dialog-warning-symbolic", GTK_ICON_SIZE_MENU);
                    char *tip = g_strdup_printf("Plugin missing or crashed: %s", item->file);
                    gtk_widget_set_tooltip_text(warn, tip);
                    g_free(tip);
                    widget = warn;
                } else {
                    VenomPanelPluginInitFnV2 fn2 = (VenomPanelPluginInitFnV2)dlsym(h, "venom_panel_plugin_init_v2");
                    VenomPanelPluginInitFn fn1 = (VenomPanelPluginInitFn)dlsym(h, "venom_panel_plugin_init");

                    const char *plugin_display_name = item->file;
                    const VenomPanelPluginAPIv2 *api2 = NULL;
                    const VenomPanelPluginAPI *api1 = NULL;

                    if (fn2) {
                        api2 = fn2();
                        const size_t min_sz = offsetof(VenomPanelPluginAPIv2, create_widget) + sizeof(((VenomPanelPluginAPIv2 *)0)->create_widget);
                        if (!api2 || api2->api_version != VENOM_PANEL_PLUGIN_API_VERSION || api2->struct_size < min_sz) {
                            g_warning("[Panel] Invalid v2 API in %s", item->file);
                            api2 = NULL;
                        } else if (api2->name) {
                            plugin_display_name = api2->name;
                        }
                    } else if (fn1) {
                        api1 = fn1();
                        if (api1 && api1->name) plugin_display_name = api1->name;
                    }

                    if ((!api2 || !api2->create_widget) && (!api1 || !api1->create_widget)) {
                        g_warning("[Panel] No valid init symbol in %s", item->file);
                        if (opened_new) dlclose(h);
                        GtkWidget *warn = gtk_image_new_from_icon_name("dialog-error-symbolic", GTK_ICON_SIZE_MENU);
                        char *tip = g_strdup_printf("Invalid plugin: %s", item->file);
                        gtk_widget_set_tooltip_text(warn, tip);
                        g_free(tip);
                        widget = warn;
                    } else {
                        if (opened_new) g_hash_table_insert(g_plugin_handles, g_strdup(fp), h);
                        widget = api2 && api2->create_widget ? api2->create_widget() : api1->create_widget();
                        if (!widget) {
                            GtkWidget *warn = gtk_image_new_from_icon_name("dialog-error-symbolic", GTK_ICON_SIZE_MENU);
                            char *tip = g_strdup_printf("Plugin %s failed to create UI", plugin_display_name);
                            gtk_widget_set_tooltip_text(warn, tip);
                            g_free(tip);
                            widget = warn;
                        } else {
                            PanelPluginInstance *inst = g_new0(PanelPluginInstance, 1);
                            inst->api_v2 = api2;
                            inst->destroyed = FALSE;
                            g_signal_connect(widget, "destroy", G_CALLBACK(on_plugin_widget_destroy), inst);
                            g_object_set_data_full(G_OBJECT(widget), "venom-panel-plugin-instance", inst, plugin_instance_free);
                            g_print("[Panel] Plugin '%s' loaded (expand=%d pad=%d)\n", plugin_display_name, expand, padding);
                        }
                    }
                }
                g_free(fp);
                break;
            }
        }

        if (widget) {
            gtk_box_pack_start(GTK_BOX(hbox), widget, expand, expand, padding);
            gtk_widget_show_all(widget);
        }
    }
    g_array_free(items, TRUE);
    g_free(plugins_dir);
}

/* =====================================================================
 * 4. PANEL SETUP
 * ===================================================================== */

/* Central function: resize + reposition panel + update struts */
static void update_panel_geometry(GtkWindow *window, GdkDisplay *display) {
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    if (!monitor) return;

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);

    /* Resize to full width of the (new) primary monitor */
    gtk_window_resize(window, geometry.width, 40);
    gtk_window_move(window, geometry.x, geometry.y);

    /* Update _NET_WM_STRUT_PARTIAL so the WM reserves space correctly */
    GdkWindow *gdk_win = gtk_widget_get_window(GTK_WIDGET(window));
    if (gdk_win) {
        gulong strut[12] = {0};
        strut[2] = 40;                               /* top */
        strut[8] = (gulong)geometry.x;               /* top_start_x */
        strut[9] = (gulong)(geometry.x + geometry.width - 1); /* top_end_x */
        gdk_property_change(gdk_win,
                           gdk_atom_intern("_NET_WM_STRUT_PARTIAL", FALSE),
                           gdk_atom_intern("CARDINAL", FALSE),
                           32, GDK_PROP_MODE_REPLACE,
                           (guchar *)strut, 12);

        gulong simple_strut[4] = {0, 0, 40, 0}; /* left, right, top, bottom */
        gdk_property_change(gdk_win,
                           gdk_atom_intern("_NET_WM_STRUT", FALSE),
                           gdk_atom_intern("CARDINAL", FALSE),
                           32, GDK_PROP_MODE_REPLACE,
                           (guchar *)simple_strut, 4);
    }
}

static void on_panel_realize(GtkWidget *widget, gpointer data) {
    (void)data;
    GdkWindow *gdk_window = gtk_widget_get_window(widget);

    /* Set window as a panel/dock */
    gdk_window_set_type_hint(gdk_window, GDK_WINDOW_TYPE_HINT_DOCK);

    /* Apply struts based on current geometry */
    GdkDisplay *display = gdk_window_get_display(gdk_window);
    update_panel_geometry(GTK_WINDOW(widget), display);
}

/* Called when a monitor is added/removed or its resolution changes */
static void on_screen_size_changed(GdkScreen *screen, gpointer user_data) {
    GdkDisplay *display = gdk_screen_get_display(screen);
    update_panel_geometry(GTK_WINDOW(user_data), display);
}

/* Global reference to the main hbox so we can reload it on signal */
static GtkWidget *g_panel_hbox = NULL;

static gboolean on_panel_sigusr1(gpointer user_data) {
    (void)user_data;
    g_print("[Panel] Received reload signal! Reloading config...\n");
    if (g_panel_hbox) {
        /* Prevent async callbacks/timers from touching freed widgets during reload */
        rec_stop_button = NULL;
        power_profile_btn = NULL;
        power_profile_label = NULL;
        power_actions_box = NULL;
        set_widget_weak(&g_tray_ui.tray_box, NULL);

        /* Destroy all current children */
        GList *children = gtk_container_get_children(GTK_CONTAINER(g_panel_hbox));
        for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
        g_list_free(children);
        /* Reload */
        load_items_from_config(g_panel_hbox);
    }
    return G_SOURCE_CONTINUE;
}

static void on_menu_preferences(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    char *settings_bin = g_find_program_in_path("venom-panel-settings");
    if (settings_bin) {
        g_spawn_command_line_async(settings_bin, NULL);
        g_free(settings_bin);
        return;
    }
    /* Dev fallback: local binary next to cwd */
    g_spawn_command_line_async("./venom-panel-settings", NULL);
}

static void on_menu_restart(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    char *panel_bin = g_find_program_in_path("venom-panel");
    const char *panel_cmd = panel_bin ? panel_bin : "./venom-panel";
    char *cmd = g_strdup_printf("bash -c 'pidof venom-panel | xargs -r kill; nohup %s >/dev/null 2>&1 &'", panel_cmd);
    g_spawn_command_line_async(cmd, NULL);
    g_free(cmd);
    g_free(panel_bin);
}

static gboolean on_panel_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget; (void)data;
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        /* Right click -> show Edit Mode context menu */
        GtkWidget *menu = gtk_menu_new();
        
        GtkWidget *item_prefs = gtk_menu_item_new_with_label("Panel Preferences...");
        g_signal_connect(item_prefs, "activate", G_CALLBACK(on_menu_preferences), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_prefs);
        
        GtkWidget *item_restart = gtk_menu_item_new_with_label("Restart Panel");
        g_signal_connect(item_restart, "activate", G_CALLBACK(on_menu_restart), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_restart);
        
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
        return TRUE;
    }
    return FALSE;
}

static void toggle_control_center(GtkWidget *button, gpointer data) {
    if (control_center_window && gtk_widget_get_visible(control_center_window)) {
        gtk_widget_hide(control_center_window);
    } else {
        if (!control_center_window) {
            control_center_window = create_control_center();
        }
        gtk_widget_show_all(control_center_window);
        gtk_window_present(GTK_WINDOW(control_center_window));
    }
}

/* =====================================================================
 * 6. SYSTEM TRAY LOGIC
 * ===================================================================== */

static void on_menu_item_activate(GtkMenuItem *menuitem, gpointer data) {
    const gchar *item_id = g_object_get_data(G_OBJECT(menuitem), "menu-id");
    const gchar *tray_id = g_object_get_data(G_OBJECT(menuitem), "tray-id");
    gint mid = atoi(item_id); // Simple atoi since id is int 
    sni_client_menu_click(tray_id, mid);
}

static void on_rec_state_changed(gboolean is_recording, gpointer user_data) {
    (void)user_data;
    g_print("[Panel] Recording state changed: %d\n", is_recording);
    if (rec_stop_button) {
        if (is_recording) {
            g_print("[Panel] Showing stop button\n");
            gtk_widget_set_no_show_all(rec_stop_button, FALSE); /* clear flag to allow show */
            gtk_widget_show_all(rec_stop_button);
            gtk_widget_set_visible(rec_stop_button, TRUE);
        } else {
            g_print("[Panel] Hiding stop button\n");
            gtk_widget_hide(rec_stop_button);
        }
    } else {
        g_print("[Panel] Stop button is NULL!\n");
    }
}

static void on_stop_recording_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    shot_stop_record();
}

/* =====================================================================
 * POWER MANAGEMENT UI
 * ===================================================================== */

static void on_power_profile_changed(const gchar *profile, gpointer user_data) {
    (void)user_data;
    if (!profile || !power_profile_label) return;
    
    gtk_label_set_text(GTK_LABEL(power_profile_label), profile);
    
    /* Update Icon and Color */
    GtkWidget *img = gtk_bin_get_child(GTK_BIN(power_profile_btn));
    GtkStyleContext *ctx = gtk_widget_get_style_context(power_profile_btn);
    GtkCssProvider *css = g_object_get_data(G_OBJECT(power_profile_btn), "profile-css");
    
    if (!css) {
        css = gtk_css_provider_new();
        g_object_set_data_full(G_OBJECT(power_profile_btn), "profile-css", css, g_object_unref);
        gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    
    const char *color = "#ffffff"; /* default */
    const gchar *icon_name = "power-profile-balanced-symbolic";
    
    if (g_strcmp0(profile, "performance") == 0) {
        icon_name = "power-profile-performance-symbolic";
        color = "#ff5555"; /* Red */
    } else if (g_strcmp0(profile, "power-saver") == 0) {
        icon_name = "power-profile-power-saver-symbolic";
        color = "#50fa7b"; /* Green */
    } else {
        /* balanced */
        color = "#8be9fd"; /* Cyan/Blue */
    }
    
    if (img) gtk_image_set_from_icon_name(GTK_IMAGE(img), icon_name, GTK_ICON_SIZE_MENU);
    
    gchar *css_data = g_strdup_printf("button { color: %s; }", color);
    gtk_css_provider_load_from_data(css, css_data, -1, NULL);
    g_free(css_data);
}

static void on_power_profile_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    const gchar *current = gtk_label_get_text(GTK_LABEL(power_profile_label));
    
    if (g_strcmp0(current, "balanced") == 0) power_set_active_profile("performance");
    else if (g_strcmp0(current, "performance") == 0) power_set_active_profile("power-saver");
    else power_set_active_profile("balanced");
}

static void on_power_action_toggle(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    g_print("[Panel] Power action toggle clicked\n");
    if (power_actions_box) {
        if (gtk_widget_get_visible(power_actions_box)) {
            g_print("[Panel] Hiding actions box\n");
            gtk_widget_hide(power_actions_box);
        } else {
            g_print("[Panel] Showing actions box\n");
            gtk_widget_set_no_show_all(power_actions_box, FALSE);
            gtk_widget_show_all(power_actions_box);
            gtk_widget_set_visible(power_actions_box, TRUE);
        }
    } else {
        g_print("[Panel] Error: power_actions_box is NULL\n");
    }
}

static void on_power_action_shutdown(GtkButton *btn, gpointer data) { (void)btn; (void)data; power_client_shutdown(); }
static void on_power_action_reboot(GtkButton *btn, gpointer data) { (void)btn; (void)data; power_client_reboot(); }
static void on_power_action_suspend(GtkButton *btn, gpointer data) { (void)btn; (void)data; power_client_suspend(); }
static void on_power_action_lock(GtkButton *btn, gpointer data) { (void)btn; (void)data; power_client_lock_screen(); }

static void on_tray_button_press(GtkButton *btn, GdkEventButton *event, gpointer data) {
     (void)data;
     const gchar *id = g_object_get_data(G_OBJECT(btn), "tray-id");
     
     /* Left Click -> Activate */
     if (event->button == GDK_BUTTON_PRIMARY) {
         if(id) {
             gint x, y;
             gdk_window_get_origin(gtk_widget_get_window(GTK_WIDGET(btn)), &x, &y);
             GtkAllocation alloc;
             gtk_widget_get_allocation(GTK_WIDGET(btn), &alloc);
             sni_client_activate(id, x + alloc.width/2, y + alloc.height/2);
         }
     }
     /* Right Click -> Menu */
     else if (event->button == GDK_BUTTON_SECONDARY) {
         if(!id) return;
         
         GList *menu_items = sni_client_get_menu(id);
         if(!menu_items) return;
         
         GtkWidget *menu = gtk_menu_new();
         
         for(GList *l=menu_items; l; l=l->next) {
             TrayMenuItem *item = (TrayMenuItem*)l->data;
             
             GtkWidget *mi;
             if (g_strcmp0(item->type, "separator") == 0) {
                 mi = gtk_separator_menu_item_new();
             } else {
                 if (item->toggle_type && strlen(item->toggle_type) > 0) {
                     mi = gtk_check_menu_item_new_with_label(item->label ? item->label : "");
                     gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), item->toggle_state == 1);
                 } else {
                     mi = gtk_menu_item_new_with_label(item->label ? item->label : "");
                 }
                 
                 /* Store IDs */
                 gchar *mid_str = g_strdup_printf("%d", item->id);
                 g_object_set_data_full(G_OBJECT(mi), "menu-id", mid_str, g_free);
                 g_object_set_data_full(G_OBJECT(mi), "tray-id", g_strdup(id), g_free);
                 
                 g_signal_connect(mi, "activate", G_CALLBACK(on_menu_item_activate), NULL);
                 
                 if (!item->enabled) gtk_widget_set_sensitive(mi, FALSE);
                 if (!item->visible) gtk_widget_set_visible(mi, FALSE);
             }
             
             gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
             gtk_widget_show(mi);
         }
         
         tray_menu_list_free(menu_items);
         
         gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(btn), GDK_GRAVITY_NORTH, GDK_GRAVITY_SOUTH, (GdkEvent*)event);
         // Note: popup_at_widget handles allocation automatically
     }
}


static void on_tray_item_added(TrayItem *item, gpointer user_data) {
    TrayUI *ui = (TrayUI *)user_data;
    GtkWidget *box = ui ? ui->tray_box : NULL;
    if (!box) return;
    
    /* Dedupe */
    GList *children = gtk_container_get_children(GTK_CONTAINER(box));
    for(GList *l=children; l; l=l->next) {
        const char *eid = g_object_get_data(G_OBJECT(l->data), "tray-id");
        if(eid && item->id && g_strcmp0(eid, item->id) == 0) {
             g_list_free(children);
             /* We could update icon here, but for now return */
             return; 
        }
    }
    g_list_free(children);
    
    GtkWidget *btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    if(item->title) gtk_widget_set_tooltip_text(btn, item->title);
    g_object_set_data_full(G_OBJECT(btn), "tray-id", g_strdup(item->id), g_free);
    
    GtkWidget *img = NULL;
    if (item->icon_pixbuf) {
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(item->icon_pixbuf, 20, 20, GDK_INTERP_BILINEAR);
        img = gtk_image_new_from_pixbuf(scaled);
        g_object_unref(scaled);
    } else {
         img = gtk_image_new_from_icon_name(item->icon_name ? item->icon_name : "image-missing", GTK_ICON_SIZE_MENU);
    }
    gtk_container_add(GTK_CONTAINER(btn), img);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "tray-icon");
    g_signal_connect(btn, "button-press-event", G_CALLBACK(on_tray_button_press), NULL);
    
    gtk_box_pack_start(GTK_BOX(box), btn, FALSE, FALSE, 0);
    gtk_widget_show_all(btn);
}

static void on_tray_item_removed(const gchar *id, gpointer user_data) {
    TrayUI *ui = (TrayUI *)user_data;
    GtkWidget *box = ui ? ui->tray_box : NULL;
    if (!box) return;
    GList *children = gtk_container_get_children(GTK_CONTAINER(box));
    for(GList *l=children; l; l=l->next) {
        const char *eid = g_object_get_data(G_OBJECT(l->data), "tray-id");
        if(eid && id && g_strcmp0(eid, id) == 0) {
             gtk_widget_destroy(GTK_WIDGET(l->data));
             break;
        }
    }
    g_list_free(children);
}


GtkWidget* create_venom_panel(void) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    /* Transparency */
    GdkScreen *screen = gtk_widget_get_screen(window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(window, visual);
        gtk_widget_set_app_paintable(window, TRUE);
    }

    gtk_window_set_title(GTK_WINDOW(window), "vaxp-panel");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    
    /* ✅ DOCK Type: This makes it a real panel */
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DOCK);
    
    /* Size & Position (initial – will also be updated dynamically) */
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *primary = gdk_display_get_primary_monitor(display);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(primary, &geometry);
    gtk_window_set_default_size(GTK_WINDOW(window), geometry.width, 40);
    gtk_window_move(GTK_WINDOW(window), geometry.x, geometry.y);

    /* Listen for monitor changes — only use GdkScreen on X11 (GdkX11Display
     * does not support the monitors-changed signal) */

    /* Fallback: GdkScreen::size-changed fires on older WMs */
    GdkScreen *screen_ev = gtk_widget_get_screen(window);
    g_signal_connect(screen_ev, "size-changed",
                     G_CALLBACK(on_screen_size_changed), window);
    
    /* Properties */
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_window_stick(GTK_WINDOW(window));
    
    /* ✅ FOCUS SETTINGS: Critical for typing */
    gtk_window_set_accept_focus(GTK_WINDOW(window), TRUE);   // Allow focus
    gtk_window_set_focus_on_map(GTK_WINDOW(window), FALSE);  // Don't steal it on startup
    
    /* ✅ Skip taskbar and pager */
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);

    g_signal_connect(window, "realize", G_CALLBACK(on_panel_realize), NULL);
    /* Right click menu handler */
    g_signal_connect(window, "button-press-event", G_CALLBACK(on_panel_button_press), NULL);

    /* Single horizontal container — items placed by config */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_panel_hbox = hbox; /* store for live reload */
    
    gtk_container_add(GTK_CONTAINER(window), hbox);
    gtk_widget_set_margin_start(hbox, 4);
    gtk_widget_set_margin_end(hbox, 4);

    /* Each builtin widget builder initialises its own client subsystem */

    /* Load all items from panel.conf (writes default on first run) */
    load_items_from_config(hbox);

    /* Main CSS */
    GtkCssProvider *p = gtk_css_provider_new();
    gchar *css_p = g_build_filename(g_get_current_dir(), "style.css", NULL);
    gtk_css_provider_load_from_path(p, css_p, NULL);
    g_free(css_p);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p), 800);
    gtk_style_context_add_class(gtk_widget_get_style_context(window), "venom-panel");

    return window;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    /* Set up signal handler for live reload */
    g_unix_signal_add(SIGUSR1, on_panel_sigusr1, NULL);

    GtkWidget *panel = create_venom_panel();
    g_signal_connect(panel, "destroy", G_CALLBACK(power_client_cleanup), NULL);
    g_signal_connect(panel, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(panel);
    gtk_main();
    return 0;
}

/* =====================================================================
 * BUILTIN WIDGET BUILDERS
 * Called by load_items_from_config() for type=builtin items.
 * ===================================================================== */

/* --- Tray --- */
static GtkWidget* build_builtin_tray(GtkWidget *hbox) {
    (void)hbox;
    /* Recording stop button */
    shot_client_init();   /* must be initialised before registering callback */
    rec_stop_button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(rec_stop_button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(rec_stop_button, "Stop Recording");
    GtkWidget *rec_icon = gtk_image_new_from_icon_name("media-playback-stop-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(rec_stop_button), rec_icon);
    GtkStyleContext *rec_ctx = gtk_widget_get_style_context(rec_stop_button);
    GtkCssProvider *rec_css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(rec_css, "button { color: #ff5555; }", -1, NULL);
    gtk_style_context_add_provider(rec_ctx, GTK_STYLE_PROVIDER(rec_css), 800);
    g_signal_connect(rec_stop_button, "clicked", G_CALLBACK(on_stop_recording_clicked), NULL);
    gtk_widget_set_no_show_all(rec_stop_button, TRUE);
    gtk_widget_hide(rec_stop_button);
    shot_client_on_recording_state(on_rec_state_changed, NULL);

    /* Tray icons box */
    GtkWidget *tray_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *tray_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(tray_container), rec_stop_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tray_container), tray_box, FALSE, FALSE, 0);

    sni_client_init();   /* initialise SNI D-Bus client before querying items */
    set_widget_weak(&g_tray_ui.tray_box, tray_box);
    sni_client_on_item_added(on_tray_item_added, &g_tray_ui);
    sni_client_on_item_removed(on_tray_item_removed, &g_tray_ui);
    GList *items = sni_client_get_items();
    for (GList *l = items; l; l = l->next)
        on_tray_item_added((TrayItem*)l->data, &g_tray_ui);
    tray_item_list_free(items);

    return tray_container;
}

/* --- Power --- */
static GtkWidget* build_builtin_power(void) {
    power_client_init();  /* safe to call multiple times */
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);

    /* Profile section */
    GtkWidget *power_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    power_profile_btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(power_profile_btn), GTK_RELIEF_NONE);
    GtkWidget *prof_icon = gtk_image_new_from_icon_name("power-profile-balanced-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(power_profile_btn), prof_icon);
    g_signal_connect(power_profile_btn, "clicked", G_CALLBACK(on_power_profile_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(power_box), power_profile_btn, FALSE, FALSE, 0);
    power_profile_label = gtk_label_new("balanced");
    GtkStyleContext *pl_ctx = gtk_widget_get_style_context(power_profile_label);
    gtk_style_context_add_class(pl_ctx, "power-label");
    gtk_widget_set_margin_end(power_profile_label, 8);
    gtk_box_pack_start(GTK_BOX(power_box), power_profile_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), power_box, FALSE, FALSE, 0);

    /* Actions section */
    GtkWidget *actions_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    power_actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);

    struct { const char *icon; const char *tip; GCallback cb; } btns[] = {
        { "system-shutdown-symbolic", "Shutdown",    G_CALLBACK(on_power_action_shutdown) },
        { "system-reboot-symbolic",   "Reboot",      G_CALLBACK(on_power_action_reboot)   },
        { "system-suspend-symbolic",  "Suspend",     G_CALLBACK(on_power_action_suspend)  },
        { "system-lock-screen-symbolic", "Lock",     G_CALLBACK(on_power_action_lock)     },
    };
    for (int i = 0; i < 4; i++) {
        GtkWidget *b = gtk_button_new_from_icon_name(btns[i].icon, GTK_ICON_SIZE_MENU);
        gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text(b, btns[i].tip);
        g_signal_connect(b, "clicked", btns[i].cb, NULL);
        gtk_box_pack_start(GTK_BOX(power_actions_box), b, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(actions_container), power_actions_box, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(power_actions_box, TRUE);
    gtk_widget_hide(power_actions_box);

    GtkWidget *toggle_btn = gtk_button_new_from_icon_name("system-shutdown-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(toggle_btn), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(toggle_btn, "Power Menu");
    g_signal_connect(toggle_btn, "clicked", G_CALLBACK(on_power_action_toggle), NULL);
    gtk_box_pack_start(GTK_BOX(actions_container), toggle_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), actions_container, FALSE, FALSE, 0);

    power_client_on_profile_changed(on_power_profile_changed, NULL);
    power_get_active_profile();
    return outer;
}

/* --- System Icons --- */
static GtkWidget* build_builtin_system_icons(void) {
    return create_system_icons();
}

/* --- Clock --- */
static GtkWidget* build_builtin_clock(void) {
    return create_clock_widget();
}

/* --- Control Center --- */
static GtkWidget* build_builtin_control_center(void) {
    GtkWidget *cc_btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(cc_btn),
        gtk_image_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_button_set_relief(GTK_BUTTON(cc_btn), GTK_RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(cc_btn), "control-center-button");
    g_signal_connect(cc_btn, "clicked", G_CALLBACK(toggle_control_center), NULL);
    return cc_btn;
}
