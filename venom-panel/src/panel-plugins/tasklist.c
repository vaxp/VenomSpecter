#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gio/gdesktopappinfo.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <string.h>
#include "venom-panel-plugin-api.h"

typedef struct {
    GtkWidget *box;
    guint timer_id;
    Display *dpy;
    Window root;
    Window active_win;
    
    /* Cache to prevent rebuilding too often */
    Window *client_list;
    int num_clients;
} TasklistData;

/* Helper to get property of type Window */
static Window* get_x11_prop_windows(Display *dpy, Window win, const char *prop_name, int *count_out) {
    Atom prop = XInternAtom(dpy, prop_name, True);
    if (prop == None) { *count_out = 0; return NULL; }
    
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop_retval = NULL;
    
    int status = XGetWindowProperty(dpy, win, prop, 0, 1024, False, AnyPropertyType,
                                    &actual_type, &actual_format, &nitems, &bytes_after, &prop_retval);
    
    if (status == Success && prop_retval && actual_format == 32) {
        *count_out = (int)nitems;
        Window *windows = g_memdup2(prop_retval, nitems * sizeof(Window));
        XFree(prop_retval);
        return windows;
    }
    
    if (prop_retval) XFree(prop_retval);
    *count_out = 0;
    return NULL;
}

/* Helper to get string property */
static char* get_x11_prop_string(Display *dpy, Window win, const char *prop_name) {
    Atom prop = XInternAtom(dpy, prop_name, True);
    if (prop == None) return NULL;
    
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop_retval = NULL;
    
    /* Try UTF8 first */
    int status = XGetWindowProperty(dpy, win, prop, 0, 1024, False, XInternAtom(dpy, "UTF8_STRING", False),
                                    &actual_type, &actual_format, &nitems, &bytes_after, &prop_retval);
                                    
    if (status != Success || prop_retval == NULL) {
        /* Fallback to STRING */
        status = XGetWindowProperty(dpy, win, prop, 0, 1024, False, XA_STRING,
                                    &actual_type, &actual_format, &nitems, &bytes_after, &prop_retval);
    }
    
    char *result = NULL;
    if (status == Success && prop_retval) {
        result = g_strdup((char*)prop_retval);
        XFree(prop_retval);
    }
    return result;
}

/* Check if window is a normal app window (not a dock, desktop, etc) */
static gboolean is_normal_window(Display *dpy, Window win) {
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) return FALSE;
    
    /* We cannot rely purely on IsViewable because minimized windows are Unmapped.
       Instead, we filter out explicitly hidden windows and override_redirects. */
    if (attrs.override_redirect) return FALSE;

    /* Check _NET_WM_STATE for SKIP_TASKBAR */
    Atom state_prop = XInternAtom(dpy, "_NET_WM_STATE", True);
    Atom skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", True);
    if (state_prop != None && skip_taskbar != None) {
        Atom actual_type; int actual_format; unsigned long nitems; unsigned long bytes_after;
        unsigned char *prop_retval = NULL;
        if (XGetWindowProperty(dpy, win, state_prop, 0, 1024, False, XA_ATOM,
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop_retval) == Success && prop_retval) {
            Atom *atoms = (Atom*)prop_retval;
            gboolean skip = FALSE;
            for (unsigned long i = 0; i < nitems; i++) {
                if (atoms[i] == skip_taskbar) { skip = TRUE; break; }
            }
            XFree(prop_retval);
            if (skip) return FALSE;
        } else if (prop_retval) {
            XFree(prop_retval);
        }
    }

    /* Check _NET_WM_WINDOW_TYPE */
    Atom type_prop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", True);
    if (type_prop == None) return TRUE; 
    
    Atom normal = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", True);
    Atom dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", True);
    Atom dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", True);
    Atom desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", True);
    Atom utility = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", True);
    
    Atom actual_type; int actual_format; unsigned long nitems; unsigned long bytes_after;
    unsigned char *prop_retval = NULL;
    
    if (XGetWindowProperty(dpy, win, type_prop, 0, 1024, False, XA_ATOM,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop_retval) == Success && prop_retval && actual_format == 32) {
        Atom *atoms = (Atom*)prop_retval;
        gboolean is_normal = FALSE;
        gboolean is_dock = FALSE;
        
        for (unsigned long i = 0; i < nitems; i++) {
            if (atoms[i] == normal || atoms[i] == dialog) is_normal = TRUE;
            if (atoms[i] == dock || atoms[i] == desktop || atoms[i] == utility) is_dock = TRUE;
        }
        XFree(prop_retval);
        
        if (is_dock) return FALSE;
        if (is_normal) return TRUE;
    } else if (prop_retval) {
        XFree(prop_retval);
    }
    
    return TRUE;
}

static void activate_window(Display *dpy, Window root, Window win) {
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 2; /* 2 = Pager/Taskbar */
    xev.xclient.data.l[1] = CurrentTime;
    xev.xclient.data.l[2] = 0;

    XSendEvent(dpy, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &xev);
    XMapRaised(dpy, win);
    XFlush(dpy);
}

static void close_window(Display *dpy, Window root, Window win) {
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = CurrentTime;
    xev.xclient.data.l[1] = 2; /* 2 = Pager/Taskbar */

    XSendEvent(dpy, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &xev);
    XFlush(dpy);
}

static void send_window_to_desktop(Display *dpy, Window root, Window win, int desktop) {
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = desktop;
    xev.xclient.data.l[1] = 2; /* 2 = Pager/Taskbar */

    XSendEvent(dpy, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &xev);
    XFlush(dpy);
}

static char* get_wm_class(Display *dpy, Window win) {
    XClassHint hint;
    memset(&hint, 0, sizeof(hint));
    if (XGetClassHint(dpy, win, &hint)) {
        char *res = NULL;
        if (hint.res_name) res = g_strdup(hint.res_name);
        else if (hint.res_class) res = g_strdup(hint.res_class);
        
        if (hint.res_name) XFree(hint.res_name);
        if (hint.res_class) XFree(hint.res_class);
        
        if (res) {
            for (int i = 0; res[i]; i++) res[i] = g_ascii_tolower(res[i]);
        }
        return res;
    }
    return NULL;
}

static GdkPixbuf* get_window_icon(Display *dpy, Window xwindow, const char *class_name) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    GdkPixbuf *pixbuf = NULL;

    /* Method 1: Try _NET_WM_ICON (modern EWMH standard, raw pixels) */
    Atom net_wm_icon = XInternAtom(dpy, "_NET_WM_ICON", False);
    if (XGetWindowProperty(dpy, xwindow, net_wm_icon, 0, 65536, False,
                           XA_CARDINAL, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop && actual_format == 32 && nitems > 2) {
            unsigned long *data = (unsigned long *)prop;
            int width = data[0];
            int height = data[1];
            int size = width * height;

            if (nitems >= (unsigned long)(size + 2) && width > 0 && height > 0 && width < 512 && height < 512) {
                pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
                guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

                for (int i = 0; i < size; i++) {
                    unsigned long argb = data[i + 2];
                    pixels[i * 4 + 0] = (argb >> 16) & 0xFF;
                    pixels[i * 4 + 1] = (argb >> 8) & 0xFF;
                    pixels[i * 4 + 2] = argb & 0xFF;
                    pixels[i * 4 + 3] = (argb >> 24) & 0xFF;
                }
            }
            XFree(prop);
            if (pixbuf != NULL) {
                GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, 24, 24, GDK_INTERP_BILINEAR);
                g_object_unref(pixbuf);
                return scaled;
            }
        } else if (prop) {
            XFree(prop);
        }
    }

    /* Method 2: Try GDesktopAppInfo to locate the exact themed icon via WM_CLASS */
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    GError *error = NULL;
    
    if (class_name) {
        GDesktopAppInfo *app_info = NULL;
        
        gchar *desktop_id = g_strdup_printf("%s.desktop", class_name);
        app_info = g_desktop_app_info_new(desktop_id);
        g_free(desktop_id);
        
        if (!app_info) {
            gchar ***desktop_ids = g_desktop_app_info_search(class_name);
            if (desktop_ids != NULL && desktop_ids[0] != NULL) {
                app_info = g_desktop_app_info_new(desktop_ids[0][0]);
            }
            if (desktop_ids != NULL) {
                for (gchar ***p = desktop_ids; *p != NULL; p++) g_strfreev(*p);
                g_free(desktop_ids);
            }
        }
        
        if (app_info) {
            gchar *icon_name = g_desktop_app_info_get_string(app_info, "Icon");
            if (icon_name) {
                if (g_path_is_absolute(icon_name)) {
                    pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_name, 24, 24, TRUE, &error);
                } else {
                    pixbuf = gtk_icon_theme_load_icon(icon_theme, icon_name, 24, GTK_ICON_LOOKUP_FORCE_SIZE, &error);
                }
                g_free(icon_name);
                if (error) { g_error_free(error); error = NULL; }
            }
            g_object_unref(app_info);
            if (pixbuf) return pixbuf;
        }
        
        /* Direct class name lookup fallback */
        pixbuf = gtk_icon_theme_load_icon(icon_theme, class_name, 24, GTK_ICON_LOOKUP_FORCE_SIZE, &error);
        if (error) { g_error_free(error); error = NULL; }
        if (pixbuf) return pixbuf;
    }
    
    /* Method 3: Generic fallback */
    pixbuf = gtk_icon_theme_load_icon(icon_theme, "application-x-executable", 24, GTK_ICON_LOOKUP_FORCE_SIZE, &error);
    if (error) { g_error_free(error); error = NULL; }
    return pixbuf;
}


static void on_task_clicked(GtkButton *btn, gpointer user_data) {
    TasklistData *data = (TasklistData*)user_data;
    Window win = (Window)GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(btn), "x11_win"));
    
    if (win) {
        /* If it's the active window, we might want to minimize it, but EWMH minimization is complex.
           For now, simply mapping and raising makes it active. */
        activate_window(data->dpy, data->root, win);
    }
}

/* Context menu handlers */
static void on_menu_close(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    gpointer *args = (gpointer*)user_data;
    TasklistData *data = args[0];
    Window win = (Window)GPOINTER_TO_SIZE(args[1]);
    
    close_window(data->dpy, data->root, win);
}

static void on_menu_move_to_ws(GtkMenuItem *item, gpointer user_data) {
    gpointer *args = (gpointer*)user_data;
    TasklistData *data = args[0];
    Window win = (Window)GPOINTER_TO_SIZE(args[1]);
    int target_ws = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "target_ws"));
    
    send_window_to_desktop(data->dpy, data->root, win, target_ws);
}

static void on_menu_args_free(gpointer data) {
    g_free(data);
}

static gboolean on_task_button_press(GtkWidget *btn, GdkEventButton *event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) { /* Right click */
        TasklistData *data = (TasklistData*)user_data;
        Window win = (Window)GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(btn), "x11_win"));
        
        GtkWidget *menu = gtk_menu_new();
        
        /* Create args array to pass multiple values to handlers */
        gpointer *args = g_new(gpointer, 2);
        args[0] = data;
        args[1] = GSIZE_TO_POINTER((gsize)win);
        g_object_set_data_full(G_OBJECT(menu), "menu_args", args, on_menu_args_free);
        
        /* Max Desktops handling */
        Atom prop = XInternAtom(data->dpy, "_NET_NUMBER_OF_DESKTOPS", True);
        Atom actual_type; int actual_format; unsigned long nitems; unsigned long bytes_after;
        unsigned char *prop_retval = NULL;
        int num_desktops = 4;
        
        if (XGetWindowProperty(data->dpy, data->root, prop, 0, 1, False, AnyPropertyType,
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop_retval) == Success && prop_retval) {
            num_desktops = (int)(*(long*)prop_retval);
            XFree(prop_retval);
        }
        
        /* Move to workspace submenu */
        GtkWidget *move_item = gtk_menu_item_new_with_label("Move to Workspace...");
        GtkWidget *submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(move_item), submenu);
        
        for (int i = 0; i < num_desktops; i++) {
            char label[32];
            snprintf(label, sizeof(label), "Workspace %d", i + 1);
            GtkWidget *ws_item = gtk_menu_item_new_with_label(label);
            g_object_set_data(G_OBJECT(ws_item), "target_ws", GINT_TO_POINTER(i));
            g_signal_connect(ws_item, "activate", G_CALLBACK(on_menu_move_to_ws), args);
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), ws_item);
        }
        
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), move_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        
        /* Close Item */
        GtkWidget *close_item = gtk_menu_item_new_with_label("Close Window");
        g_signal_connect(close_item, "activate", G_CALLBACK(on_menu_close), args);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), close_item);
        
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
        return TRUE; /* Event handled */
    }
    return FALSE; /* Not right click */
}

static void rebuild_tasklist(TasklistData *data) {
    /* Clear current */
    GList *children = gtk_container_get_children(GTK_CONTAINER(data->box));
    for (GList *l = children; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    if (!data->client_list) return;

    for (int i = 0; i < data->num_clients; i++) {
        Window win = data->client_list[i];
        if (!is_normal_window(data->dpy, win)) continue;
        
        GtkWidget *btn = gtk_toggle_button_new();
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        gtk_widget_set_size_request(btn, 36, 36); /* Square icon button */
        
        char *wm_class = get_wm_class(data->dpy, win);
        GdkPixbuf *pixbuf = get_window_icon(data->dpy, win, wm_class);
        
        GtkWidget *icon;
        if (pixbuf) {
            icon = gtk_image_new_from_pixbuf(pixbuf);
            g_object_unref(pixbuf);
        } else {
            icon = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_LARGE_TOOLBAR);
        }
        gtk_container_add(GTK_CONTAINER(btn), icon);
        
        /* Add tooltip with full name */
        char *full_name = get_x11_prop_string(data->dpy, win, "_NET_WM_NAME");
        if (!full_name) full_name = get_x11_prop_string(data->dpy, win, "WM_NAME");
        if (full_name) {
            gtk_widget_set_tooltip_text(btn, full_name);
            g_free(full_name);
        }
        if (wm_class) g_free(wm_class);
        
        if (win == data->active_win) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), TRUE);
        }
        
        g_object_set_data(G_OBJECT(btn), "x11_win", GSIZE_TO_POINTER((gsize)win));
        g_signal_connect(btn, "clicked", G_CALLBACK(on_task_clicked), data);
        g_signal_connect(btn, "button-press-event", G_CALLBACK(on_task_button_press), data);
        
        gtk_box_pack_start(GTK_BOX(data->box), btn, FALSE, FALSE, 0);
    }
    
    gtk_widget_show_all(data->box);
}

static gboolean poll_tasklist(gpointer user_data) {
    TasklistData *data = (TasklistData*)user_data;
    if (!GTK_IS_WIDGET(data->box)) return G_SOURCE_REMOVE;
    if (!data->dpy) return G_SOURCE_CONTINUE;
    
    int new_count = 0;
    Window *new_list = get_x11_prop_windows(data->dpy, data->root, "_NET_CLIENT_LIST", &new_count);
    
    int active_count = 0;
    Window *active_wins = get_x11_prop_windows(data->dpy, data->root, "_NET_ACTIVE_WINDOW", &active_count);
    Window new_active = (active_count > 0 && active_wins) ? active_wins[0] : None;
    if (active_wins) g_free(active_wins);
    
    gboolean needs_rebuild = FALSE;
    
    if (new_count != data->num_clients || new_active != data->active_win) {
        needs_rebuild = TRUE;
    } else if (new_list && data->client_list) {
        if (memcmp(new_list, data->client_list, new_count * sizeof(Window)) != 0) {
            needs_rebuild = TRUE;
        }
    }
    
    if (needs_rebuild) {
        if (data->client_list) g_free(data->client_list);
        data->client_list = new_list;
        data->num_clients = new_count;
        data->active_win = new_active;
        rebuild_tasklist(data);
    } else {
        if (new_list) g_free(new_list);
    }
    
    return G_SOURCE_CONTINUE;
}

static void on_widget_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    TasklistData *data = (TasklistData*)user_data;
    if (data->timer_id > 0) g_source_remove(data->timer_id);
    if (data->client_list) g_free(data->client_list);
    g_free(data);
}

static GtkWidget* create_tasklist_widget(void) {
    TasklistData *data = g_new0(TasklistData, 1);
    
    GdkDisplay *gdk_dpy = gdk_display_get_default();
    if (GDK_IS_X11_DISPLAY(gdk_dpy)) {
        data->dpy = gdk_x11_display_get_xdisplay(gdk_dpy);
        data->root = DefaultRootWindow(data->dpy);
    }
    
    data->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    
    /* Initial fetch */
    if (data->dpy) {
        data->client_list = get_x11_prop_windows(data->dpy, data->root, "_NET_CLIENT_LIST", &data->num_clients);
        int active_count = 0;
        Window *active = get_x11_prop_windows(data->dpy, data->root, "_NET_ACTIVE_WINDOW", &active_count);
        if (active) {
            data->active_win = active[0];
            g_free(active);
        }
    }
    rebuild_tasklist(data);
    
    /* Fast poll - 200ms is perfectly fine for basic X11 calls */
    data->timer_id = g_timeout_add(200, poll_tasklist, data);
    g_signal_connect(data->box, "destroy", G_CALLBACK(on_widget_destroy), data);

    return data->box;
}

VenomPanelPluginAPI* venom_panel_plugin_init(void) {
    static VenomPanelPluginAPI api;
    api.name          = "Window List";
    api.description   = "Displays open windows (Taskbar).";
    api.author        = "Venom";
    /* IMPORTANT: Taskbar MUST expand to fill available empty space in the panel */
    api.expand        = TRUE;
    api.padding       = 4;
    api.create_widget = create_tasklist_widget;
    return &api;
}
