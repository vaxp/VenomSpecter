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

static GtkWidget *control_center_window = NULL;
static GtkWidget *search_entry = NULL;

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
    

    
    /* Tray Area */
    GtkWidget *tray_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(r_box), tray_box, FALSE, FALSE, 0);

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
    g_signal_connect(panel, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(panel);
    gtk_main();
    return 0;
}