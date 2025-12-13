#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* Headers (Assumed present in your project) */
#include "venom-panel.h"
#include "clock-widget.h"
#include "system-icons.h"
#include "control-center.h"
#include "search.h"
#include "sni-client.h"
#include "shot-client.h"
#include "power-client.h"

static GtkWidget *control_center_window = NULL;

static GtkWidget *search_entry = NULL;

static GtkWidget *rec_stop_button = NULL;
static GtkWidget *power_profile_btn = NULL;
static GtkWidget *power_profile_label = NULL;
static GtkWidget *power_actions_box = NULL;

/* =====================================================================
 * 1. SEARCH HANDLER & LAUNCHER FIX
 * ===================================================================== */

/* ✅ Wrapper for web search - prevents closing the main panel */
static void on_web_search_wrapper(const gchar *term, const gchar *engine) {
    gchar *url = NULL;
    if (g_strcmp0(engine, "github") == 0) {
        url = g_strdup_printf("https://github.com/search?q=%s", term);
    } else if (g_strcmp0(engine, "youtube") == 0) {
        url = g_strdup_printf("https://www.youtube.com/results?search_query=%s", term);
    } else {
        url = g_strdup_printf("https://www.google.com/search?q=%s", term);
    }
    
    if (url) {
        g_app_info_launch_default_for_uri(url, NULL, NULL);
        g_free(url);
    }
}

/* ✅ Fix for Linker Error: Definition of on_launcher_app_clicked */
void on_launcher_app_clicked(GtkWidget *widget, gpointer data) {
    const gchar *type = (const gchar *)data;
    
    /* Case 1: Direct .desktop file path */
    if (g_str_has_suffix(type, ".desktop")) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(type);
        if (app_info) {
            g_app_info_launch(G_APP_INFO(app_info), NULL, NULL, NULL);
            g_object_unref(app_info);
        }
        return;
    }

    /* Case 2: Data attached to widget */
    const gchar *desktop_file = g_object_get_data(G_OBJECT(widget), "desktop-file");
    if (desktop_file != NULL) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(desktop_file);
        if (app_info != NULL) {
            g_app_info_launch(G_APP_INFO(app_info), NULL, NULL, NULL);
            g_object_unref(app_info);
        }
    }
}

static void on_search_activate(GtkEntry *entry, gpointer data) {
    const gchar *text = gtk_entry_get_text(entry);
    if (!text || strlen(text) == 0) return;
    
    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(entry));
    
    if (g_str_has_prefix(text, "vater:")) {
        execute_vater(text + 6, window);
        gtk_entry_set_text(entry, "");
        return;
    }
    if (g_str_has_prefix(text, "!:")) {
        execute_math(text + 2, window);
        gtk_entry_set_text(entry, "");
        return;
    }
    if (g_str_has_prefix(text, "vafile:")) {
        execute_file_search(text + 7, window);
        gtk_entry_set_text(entry, "");
        return;
    }
    if (g_str_has_prefix(text, "g:")) {
        on_web_search_wrapper(text + 2, "github");
        gtk_entry_set_text(entry, "");
        return;
    }
    if (g_str_has_prefix(text, "s:")) {
        on_web_search_wrapper(text + 2, "google");
        gtk_entry_set_text(entry, "");
        return;
    }
    if (g_str_has_prefix(text, "y:")) {
        on_web_search_wrapper(text + 2, "youtube");
        gtk_entry_set_text(entry, "");
        return;
    }
    if (g_str_has_prefix(text, "ai:")) {
        execute_ai_chat(text + 3, window);
        gtk_entry_set_text(entry, "");
        return;
    }
    
    /* Default: Execute command */
    g_spawn_command_line_async(text, NULL);
    gtk_entry_set_text(entry, "");
}

/* =====================================================================
 * 3. FOCUS FIX LOGIC (CRITICAL FOR DOCK)
 * ===================================================================== */

/* ✅ هذه الدالة تجبر النظام على إعطاء الكيبورد للبنل عند النقر */
static gboolean force_panel_focus(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    GtkWindow *window = GTK_WINDOW(data);
    if (event->type == GDK_BUTTON_PRESS) {
        gtk_window_present_with_time(window, event->time);
    }
    return FALSE; /* Continue processing the click (so text cursor moves) */
}

/* =====================================================================
 * 4. PANEL SETUP
 * ===================================================================== */

static void on_panel_realize(GtkWidget *widget, gpointer data) {
    (void)data;
    GdkWindow *gdk_window = gtk_widget_get_window(widget);
    GdkDisplay *display = gdk_window_get_display(gdk_window);
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    
    /* Set window as a panel/dock */
    gdk_window_set_type_hint(gdk_window, GDK_WINDOW_TYPE_HINT_DOCK);
    
    /* Reserve space at top */
    gulong strut[12] = {0};
    strut[2] = 40;  /* top */
    strut[8] = 0;   /* top_start_x */
    strut[9] = geometry.width;  /* top_end_x */
    
    gdk_property_change(gdk_window, 
                       gdk_atom_intern("_NET_WM_STRUT_PARTIAL", FALSE),
                       gdk_atom_intern("CARDINAL", FALSE),
                       32, GDK_PROP_MODE_REPLACE,
                       (guchar *)strut, 12);
    
    /* Also set _NET_WM_STRUT for older window managers */
    gulong simple_strut[4] = {0, 0, 40, 0};  /* left, right, top, bottom */
    gdk_property_change(gdk_window,
                       gdk_atom_intern("_NET_WM_STRUT", FALSE),
                       gdk_atom_intern("CARDINAL", FALSE),
                       32, GDK_PROP_MODE_REPLACE,
                       (guchar *)simple_strut, 4);
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
 * 5. SYSTEM TRAY LOGIC
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
    GtkWidget *box = GTK_WIDGET(user_data);
    
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
    GtkWidget *box = GTK_WIDGET(user_data);
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
    
    /* Size & Position */
    GdkDisplay *display = gdk_display_get_default();
    GdkRectangle geometry;
    gdk_monitor_get_geometry(gdk_display_get_primary_monitor(display), &geometry);
    gtk_window_set_default_size(GTK_WINDOW(window), geometry.width, 40);
    gtk_window_move(GTK_WINDOW(window), 0, 0);
    
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
    
    /* Layout */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), hbox);
    
    /* Left Spacer */
    GtkWidget *l_sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(l_sp, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), l_sp, TRUE, TRUE, 0);
    
    /* --- SEARCH BAR --- */
    search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Admiral: (vater:, !:, s:, g:, y:)");
    gtk_widget_set_size_request(search_entry, 350, 24);
    gtk_widget_set_valign(search_entry, GTK_ALIGN_CENTER);
    
    /* ✅ ربط دالة خطف التركيز هنا */
    g_signal_connect(search_entry, "button-press-event", G_CALLBACK(force_panel_focus), window);
    
    /* Load Search CSS */
    GtkCssProvider *s_css = gtk_css_provider_new();
    /* Fix backdrop dimming in CSS */
    const char *search_css = 
        "entry { background: rgba(255,255,255,0.1); color: white; border-radius: 6px; border: none; }"
        "entry:focus { background: rgba(255,255,255,0.15); }"
        "entry:backdrop { background: rgba(255,255,255,0.1); color: white; }";
    gtk_css_provider_load_from_data(s_css, search_css, -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(search_entry), GTK_STYLE_PROVIDER(s_css), 800);
    
    g_signal_connect(search_entry, "activate", G_CALLBACK(on_search_activate), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), search_entry, FALSE, FALSE, 0);
    
    /* Right Spacer */
    GtkWidget *r_sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(r_sp, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), r_sp, TRUE, TRUE, 0);
    
    /* Right Widgets */
    GtkWidget *r_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_end(r_box, 12);

    gtk_box_pack_end(GTK_BOX(hbox), r_box, FALSE, FALSE, 0);
    
    /* Recording Stop Button (Hidden by default) */
    rec_stop_button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(rec_stop_button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(rec_stop_button, "Stop Recording");
    GtkWidget *rec_icon = gtk_image_new_from_icon_name("media-playback-stop-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(rec_stop_button), rec_icon);
    
    /* Style it: maybe red background or red icon? */
    GtkStyleContext *rec_ctx = gtk_widget_get_style_context(rec_stop_button);
    gtk_style_context_add_class(rec_ctx, "destructive-action"); /* Assume this class exists or add inline style */
    
    /* Apply custom red color just in case */
    GtkCssProvider *rec_css = gtk_css_provider_new();
    const char *css_red = "button { color: #ff5555; }"; 
    gtk_css_provider_load_from_data(rec_css, css_red, -1, NULL);
    gtk_style_context_add_provider(rec_ctx, GTK_STYLE_PROVIDER(rec_css), 800);
    
    g_signal_connect(rec_stop_button, "clicked", G_CALLBACK(on_stop_recording_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(r_box), rec_stop_button, FALSE, FALSE, 0);
    /* Ensure it's hidden initially */
    /* gtk_widget_show_all checks visibility prop, but we will manage it manually */
    gtk_widget_set_no_show_all(rec_stop_button, TRUE);
    gtk_widget_hide(rec_stop_button);
    
    /* Register callback */
    shot_client_on_recording_state(on_rec_state_changed, NULL);
    

    
    /* Tray Area */
    GtkWidget *tray_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(r_box), tray_box, FALSE, FALSE, 0);


    
    /* Register callback */
    shot_client_on_recording_state(on_rec_state_changed, NULL);

    /* SNI Init */
    sni_client_init();
    
    sni_client_on_item_added(on_tray_item_added, tray_box);
    sni_client_on_item_removed(on_tray_item_removed, tray_box);
    
    /* Initial Fetch */
    GList *items = sni_client_get_items();
    for(GList *l=items; l; l=l->next) {
        on_tray_item_added((TrayItem*)l->data, tray_box);
        /* Note: on_tray_item_added does not free the item passed to it if we want to reuse logic cleanly,
           BUT our create_tray_item allocates. on_tray_item_added copies ID. 
           We should free the item here. */
    }
    tray_item_list_free(items);
    
    /* --- Power Management UI --- */
    power_client_init();
    
    /* 1. Power Profile Section */
    GtkWidget *power_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    
    /* Toggle Button */
    power_profile_btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(power_profile_btn), GTK_RELIEF_NONE);
    GtkWidget *prof_icon = gtk_image_new_from_icon_name("power-profile-balanced-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(power_profile_btn), prof_icon);
    g_signal_connect(power_profile_btn, "clicked", G_CALLBACK(on_power_profile_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(power_box), power_profile_btn, FALSE, FALSE, 0);
    
    /* Profile Lable */
    power_profile_label = gtk_label_new("Balanced"); /* value updated by callback */
    /* Add style class for label */
    GtkStyleContext *pl_ctx = gtk_widget_get_style_context(power_profile_label);
    gtk_style_context_add_class(pl_ctx, "power-label");
    /* Apply some padding */
    gtk_widget_set_margin_end(power_profile_label, 8);
    gtk_box_pack_start(GTK_BOX(power_box), power_profile_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(r_box), power_box, FALSE, FALSE, 0);
    
    /* 2. Power Actions Section */
    GtkWidget *actions_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    
    /* Expanded Actions Box (Hidden) */
    power_actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    
    /* Helper to create action buttons */
    GtkWidget *btn_shut = gtk_button_new_from_icon_name("system-shutdown-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(btn_shut), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(btn_shut, "Shutdown");
    g_signal_connect(btn_shut, "clicked", G_CALLBACK(on_power_action_shutdown), NULL);
    gtk_box_pack_start(GTK_BOX(power_actions_box), btn_shut, FALSE, FALSE, 0);
    
    GtkWidget *btn_reb = gtk_button_new_from_icon_name("system-reboot-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(btn_reb), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(btn_reb, "Reboot");
    g_signal_connect(btn_reb, "clicked", G_CALLBACK(on_power_action_reboot), NULL);
    gtk_box_pack_start(GTK_BOX(power_actions_box), btn_reb, FALSE, FALSE, 0);
    
    GtkWidget *btn_susp = gtk_button_new_from_icon_name("system-suspend-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(btn_susp), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(btn_susp, "Suspend");
    g_signal_connect(btn_susp, "clicked", G_CALLBACK(on_power_action_suspend), NULL);
    gtk_box_pack_start(GTK_BOX(power_actions_box), btn_susp, FALSE, FALSE, 0);
    
    GtkWidget *btn_lock = gtk_button_new_from_icon_name("system-lock-screen-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(btn_lock), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(btn_lock, "Lock Screen");
    g_signal_connect(btn_lock, "clicked", G_CALLBACK(on_power_action_lock), NULL);
    gtk_box_pack_start(GTK_BOX(power_actions_box), btn_lock, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(actions_container), power_actions_box, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(power_actions_box, TRUE);
    gtk_widget_hide(power_actions_box);
    
    /* Main Toggle Button */
    GtkWidget *power_toggle_btn = gtk_button_new_from_icon_name("system-shutdown-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(power_toggle_btn), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(power_toggle_btn, "Power Menu");
    g_signal_connect(power_toggle_btn, "clicked", G_CALLBACK(on_power_action_toggle), NULL);
    gtk_box_pack_start(GTK_BOX(actions_container), power_toggle_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(r_box), actions_container, FALSE, FALSE, 0);
    
    /* Register Profile Callback */
    power_client_on_profile_changed(on_power_profile_changed, NULL);
    power_get_active_profile();
    
    gtk_box_pack_start(GTK_BOX(r_box), create_system_icons(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(r_box), create_clock_widget(), FALSE, FALSE, 0);
    
    GtkWidget *cc_btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(cc_btn), gtk_image_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_button_set_relief(GTK_BUTTON(cc_btn), GTK_RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(cc_btn), "control-center-button");
    g_signal_connect(cc_btn, "clicked", G_CALLBACK(toggle_control_center), NULL);
    gtk_box_pack_start(GTK_BOX(r_box), cc_btn, FALSE, FALSE, 0);
    
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
    GtkWidget *panel = create_venom_panel();
    g_signal_connect(panel, "destroy", G_CALLBACK(power_client_cleanup), NULL);
    g_signal_connect(panel, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(panel);
    gtk_main();
    return 0;
}