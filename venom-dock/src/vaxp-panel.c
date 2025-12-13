#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gio/gdesktopappinfo.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "launcher.h"
#include "pager.h"

/* Global X11 variables */
Display *xdisplay;
Window root_window;
Atom net_client_list_atom;
Atom net_wm_name_atom;
Atom wm_name_atom;
Atom net_wm_icon_atom;
Atom net_active_window_atom;
Atom utf8_string_atom;
Atom wm_class_atom;
Atom wm_hints_atom;


/* Window Group structure for grouping windows by WM_CLASS */
typedef struct {
    char *wm_class;
    GList *windows;  /* List of Window IDs */
    GdkPixbuf *icon;
    GtkWidget *button;
    int active_index;
    char *desktop_file_path;  /* Path to .desktop file */
    gboolean is_pinned;       /* Whether app is pinned */
} WindowGroup;

GtkWidget *box;
GHashTable *window_groups; /* wm_class -> WindowGroup */
GList *pinned_apps = NULL; /* List of pinned wm_class strings */


/* Dock Realize Callback - Set WM_STRUT for space reservation */
static void on_dock_realize(GtkWidget *widget, gpointer data) {
    (void)data;
    GdkWindow *gdk_window = gtk_widget_get_window(widget);
    GdkDisplay *display = gdk_window_get_display(gdk_window);
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    
    /* Set window as dock type */
    gdk_window_set_type_hint(gdk_window, GDK_WINDOW_TYPE_HINT_DOCK);
    
    /* Reserve 60px at bottom */
    gulong strut[12] = {0};
    strut[3] = 60;  /* bottom */
    strut[10] = 0;  /* bottom_start_x */
    strut[11] = geometry.width;  /* bottom_end_x */
    
    gdk_property_change(gdk_window, 
                       gdk_atom_intern("_NET_WM_STRUT_PARTIAL", FALSE),
                       gdk_atom_intern("CARDINAL", FALSE),
                       32, GDK_PROP_MODE_REPLACE,
                       (guchar *)strut, 12);
    
    /* Also set _NET_WM_STRUT for older window managers */
    gulong simple_strut[4] = {0, 0, 0, 60};  /* left, right, top, bottom */
    gdk_property_change(gdk_window,
                       gdk_atom_intern("_NET_WM_STRUT", FALSE),
                       gdk_atom_intern("CARDINAL", FALSE),
                       32, GDK_PROP_MODE_REPLACE,
                       (guchar *)simple_strut, 4);
}


/* Function prototypes */
static void on_dock_realize(GtkWidget *widget, gpointer data);
/* Helper for process detachment */
static void child_setup(gpointer user_data) {
    (void)user_data;
    setsid();
}
void update_window_list();
GdkPixbuf *get_window_icon(Window xwindow);
char *get_window_name(Window xwindow);
char *get_window_class(Window xwindow);
void on_button_clicked(GtkWidget *widget, gpointer data);
gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data);
void create_context_menu(WindowGroup *group, GdkEventButton *event);
void on_pin_clicked(GtkWidget *menuitem, gpointer data);
void on_new_window_clicked(GtkWidget *menuitem, gpointer data);
void on_close_all_clicked(GtkWidget *menuitem, gpointer data);
void on_run_with_gpu_clicked(GtkWidget *menuitem, gpointer data);
void load_pinned_apps();
void save_pinned_apps();
/* Launcher button */
/* Defined in launcher.c */
GdkFilterReturn event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data);

int main(int argc, char *argv[]) {
    /* Suppress accessibility bus warning */
    g_setenv("GTK_A11Y", "none", TRUE);

    gtk_init(&argc, &argv);

    /* Initialize X11 */
    xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    root_window = DefaultRootWindow(xdisplay);

    net_client_list_atom = XInternAtom(xdisplay, "_NET_CLIENT_LIST", False);
    net_wm_name_atom = XInternAtom(xdisplay, "_NET_WM_NAME", False);
    wm_name_atom = XInternAtom(xdisplay, "WM_NAME", False);
    net_wm_icon_atom = XInternAtom(xdisplay, "_NET_WM_ICON", False);
    net_active_window_atom = XInternAtom(xdisplay, "_NET_ACTIVE_WINDOW", False);
    utf8_string_atom = XInternAtom(xdisplay, "UTF8_STRING", False);
    wm_class_atom = XInternAtom(xdisplay, "WM_CLASS", False);
    wm_hints_atom = XInternAtom(xdisplay, "WM_HINTS", False);

    /* Load CSS */
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    
    gtk_style_context_add_provider_for_screen(screen,
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    GError *error = NULL;
    if (!gtk_css_provider_load_from_path(provider, "style.css", &error)) {
        g_warning("Failed to load CSS: %s", error->message);
        g_error_free(error);
    }
    g_object_unref(provider);

    /* UI Setup */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "vaxp-dock");
    
    /* Get screen dimensions for full-width sizing */
    GdkScreen *gdk_screen = gdk_display_get_default_screen(display);
    gint screen_width = gdk_screen_get_width(gdk_screen);
    gint screen_height = gdk_screen_get_height(gdk_screen);
    
    /* Set as DOCK - explicitly declare as dock window */
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_window_set_default_size(GTK_WINDOW(window), screen_width, 60);
    gtk_window_set_gravity(GTK_WINDOW(window), GDK_GRAVITY_SOUTH);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_set_app_paintable(window, TRUE);
    
    /* Window properties for dock behavior */
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_window_stick(GTK_WINDOW(window));
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);

    /* Enable transparency */
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual != NULL && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(window, visual);
    }

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_name(box, "dock-box"); /* ID for CSS */
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(box, 4);      /* Add vertical padding */
    gtk_widget_set_margin_bottom(box, 4);
    gtk_widget_set_margin_start(box, 8);    /* Add horizontal padding */
    gtk_widget_set_margin_end(box, 8);
    gtk_container_add(GTK_CONTAINER(window), box);

    g_signal_connect(window, "realize", G_CALLBACK(on_dock_realize), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* Initialize window groups hash table */
    window_groups = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    /* Load pinned apps */
    load_pinned_apps();

    /* Create launcher button */
    /* Create launcher button */
    create_launcher_button(box);

    /* Initial update */
    update_window_list();

    /* Setup event filter to listen for X11 property changes */
    XSelectInput(xdisplay, root_window, PropertyChangeMask);
    gdk_window_add_filter(NULL, event_filter, NULL);

    gtk_widget_show_all(window);
    
    /* Position at bottom center - full width */
    gtk_window_move(GTK_WINDOW(window), 0, screen_height - 60);
    
    gtk_main();

    return 0;
}

/* Update the list of windows in the panel */
void update_window_list() {
    /* Clear existing buttons (except launcher) */
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(box));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        GtkWidget *child = GTK_WIDGET(iter->data);
        /* Don't destroy the launcher button */
        if (child != launcher_button) {
            gtk_widget_destroy(child);
        }
    }
    g_list_free(children);

    /* Clear window lists from groups, but keep pinned apps */
    if (window_groups != NULL) {
        GHashTableIter hash_iter;
        gpointer key, value;
        g_hash_table_iter_init(&hash_iter, window_groups);
        while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
            WindowGroup *group = (WindowGroup *)value;
            
            /* Clear windows list */
            if (group->windows) {
                g_list_free(group->windows);
                group->windows = NULL;
            }
            
            /* Remove unpinned groups */
            if (!group->is_pinned) {
                if (group->icon) {
                    g_object_unref(group->icon);
                }
                if (group->desktop_file_path) {
                    g_free(group->desktop_file_path);
                }
                g_free(group->wm_class);
                g_free(group);
                g_hash_table_iter_remove(&hash_iter);
            }
        }
    }

    /* Get client list */
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(xdisplay, root_window, net_client_list_atom, 0, 1024, False,
                           XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop) {
            Window *list = (Window *)prop;
            
            /* First pass: Group windows by WM_CLASS */
            for (unsigned long i = 0; i < nitems; i++) {
                Window win = list[i];
                char *wm_class = get_window_class(win);
                
                if (wm_class) {
                    WindowGroup *group = g_hash_table_lookup(window_groups, wm_class);
                    
                    if (group == NULL) {
                        /* Create new group */
                        group = g_malloc0(sizeof(WindowGroup));
                        group->wm_class = strdup(wm_class);
                        group->windows = NULL;
                        group->icon = get_window_icon(win);
                        group->active_index = 0;
                        
                        /* Try to find .desktop file */
                        gchar *desktop_id = g_strdup_printf("%s.desktop", wm_class);
                        GDesktopAppInfo *app_info = g_desktop_app_info_new(desktop_id);
                        if (app_info == NULL) {
                            gchar *lowercase = g_ascii_strdown(wm_class, -1);
                            g_free(desktop_id);
                            desktop_id = g_strdup_printf("%s.desktop", lowercase);
                            app_info = g_desktop_app_info_new(desktop_id);
                            g_free(lowercase);
                        }
                        
                        if (app_info != NULL) {
                            group->desktop_file_path = g_strdup(g_desktop_app_info_get_filename(app_info));
                            g_object_unref(app_info);
                        } else {
                            group->desktop_file_path = NULL;
                        }
                        g_free(desktop_id);
                        
                        /* Check if pinned */
                        group->is_pinned = (g_list_find_custom(pinned_apps, wm_class, (GCompareFunc)g_strcmp0) != NULL);
                        
                        g_hash_table_insert(window_groups, strdup(wm_class), group);
                    }
                    
                    /* Add window to group */
                    group->windows = g_list_append(group->windows, GINT_TO_POINTER(win));
                    free(wm_class);
                }
            }
            
            /* Third pass: Add pinned apps that don't have windows */
            for (GList *l = pinned_apps; l != NULL; l = l->next) {
                const gchar *pinned_class = (const gchar *)l->data;
                
                /* Check if this pinned app already has a group */
                if (g_hash_table_lookup(window_groups, pinned_class) == NULL) {
                    /* Create group for pinned app without windows */
                    WindowGroup *group = g_malloc0(sizeof(WindowGroup));
                    group->wm_class = strdup(pinned_class);
                    group->windows = NULL;
                    group->active_index = 0;
                    group->is_pinned = TRUE;
                    
                    /* Try to find .desktop file and icon */
                    gchar *desktop_id = g_strdup_printf("%s.desktop", pinned_class);
                    GDesktopAppInfo *app_info = g_desktop_app_info_new(desktop_id);
                    if (app_info == NULL) {
                        gchar *lowercase = g_ascii_strdown(pinned_class, -1);
                        g_free(desktop_id);
                        desktop_id = g_strdup_printf("%s.desktop", lowercase);
                        app_info = g_desktop_app_info_new(desktop_id);
                        g_free(lowercase);
                    }
                    
                    if (app_info != NULL) {
                        group->desktop_file_path = g_strdup(g_desktop_app_info_get_filename(app_info));
                        
                        /* Get icon from desktop file */
                        gchar *icon_name = g_desktop_app_info_get_string(app_info, "Icon");
                        if (icon_name != NULL) {
                            GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
                            GError *error = NULL;
                            
                            if (g_path_is_absolute(icon_name)) {
                                group->icon = gdk_pixbuf_new_from_file_at_scale(icon_name, 32, 32, TRUE, &error);
                            } else {
                                group->icon = gtk_icon_theme_load_icon(icon_theme, icon_name, 32,
                                                                       GTK_ICON_LOOKUP_FORCE_SIZE, &error);
                            }
                            
                            if (error) {
                                g_error_free(error);
                            }
                            g_free(icon_name);
                        }
                        
                        g_object_unref(app_info);
                    } else {
                        group->desktop_file_path = NULL;
                        group->icon = NULL;
                    }
                    g_free(desktop_id);
                    
                    g_hash_table_insert(window_groups, strdup(pinned_class), group);
                }
            }
            
            /* Fourth pass: Create buttons for each group */
            GHashTableIter hash_iter;
            gpointer key, value;
            g_hash_table_iter_init(&hash_iter, window_groups);
            
            while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
                WindowGroup *group = (WindowGroup *)value;
                
                /* Create button for this group */
                GtkWidget *button = gtk_button_new();
                group->button = button;
                
                /* Set tooltip */
                int window_count = g_list_length(group->windows);
                if (window_count > 1) {
                    char tooltip[256];
                    snprintf(tooltip, sizeof(tooltip), "%s (%d windows)", group->wm_class, window_count);
                    gtk_widget_set_tooltip_text(button, tooltip);
                } else if (window_count == 1) {
                    /* Single window - use window name */
                    Window win = GPOINTER_TO_INT(g_list_first(group->windows)->data);
                    char *name = get_window_name(win);
                    gtk_widget_set_tooltip_text(button, name ? name : group->wm_class);
                    if (name) free(name);
                } else {
                    /* No windows - pinned app */
                    gtk_widget_set_tooltip_text(button, group->wm_class);
                }
                
                /* Create Overlay to isolate dot from layout flow */
                GtkWidget *overlay = gtk_overlay_new();
                
                /* Icon as main child (centered) */
                if (group->icon) {
                    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(group->icon, 24, 24, GDK_INTERP_BILINEAR);
                    GtkWidget *image = gtk_image_new_from_pixbuf(scaled);
                    gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
                    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                    gtk_container_add(GTK_CONTAINER(overlay), image);
                    g_object_unref(scaled);
                } else {
                    GtkWidget *label = gtk_label_new("?");
                    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
                    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
                    gtk_container_add(GTK_CONTAINER(overlay), label);
                }
                
                /* Indicator Dot as Overlay Child (Bottom) */
                GtkWidget *dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_widget_set_name(dot, "indicator-dot");
                gtk_widget_set_size_request(dot, 6, 6);
                gtk_widget_set_halign(dot, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(dot, GTK_ALIGN_END);
                gtk_widget_set_margin_bottom(dot, 2); /* Slight offset from very bottom */
                
                gtk_overlay_add_overlay(GTK_OVERLAY(overlay), dot);
                
                gtk_container_add(GTK_CONTAINER(button), overlay);
                
                /* Store group pointer in button */
                g_object_set_data(G_OBJECT(button), "group", group);
                g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), NULL);
                g_signal_connect(button, "button-press-event", G_CALLBACK(on_button_press), NULL);
                
                /* Add CSS class for styling */
                GtkStyleContext *context = gtk_widget_get_style_context(button);
                if (window_count > 0) {
                    gtk_style_context_add_class(context, "running-app");
                } else {
                    gtk_style_context_add_class(context, "pinned-app");
                }
                
                gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
            }
            
            XFree(prop);
        }
    }
    gtk_widget_show_all(box);
}

/* Get window name */
char *get_window_name(Window xwindow) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    char *name = NULL;

    /* Try _NET_WM_NAME first (UTF-8) */
    if (XGetWindowProperty(xdisplay, xwindow, net_wm_name_atom, 0, 1024, False,
                           utf8_string_atom, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop) {
            name = strdup((char *)prop);
            XFree(prop);
            return name;
        }
    }

    /* Fallback to WM_NAME */
    if (XGetWindowProperty(xdisplay, xwindow, wm_name_atom, 0, 1024, False,
                           XA_STRING, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop) {
            name = strdup((char *)prop);
            XFree(prop);
            return name;
        }
    }

    return NULL;
}

/* Get window class (WM_CLASS) */
char *get_window_class(Window xwindow) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    char *wm_class = NULL;

    if (XGetWindowProperty(xdisplay, xwindow, wm_class_atom, 0, 1024, False,
                           XA_STRING, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            /* WM_CLASS contains instance\0class\0 */
            char *instance = (char *)prop;
            char *class = instance + strlen(instance) + 1;
            
            /* Return the class name (second part) */
            if (strlen(class) > 0) {
                wm_class = strdup(class);
            } else if (strlen(instance) > 0) {
                /* Fallback to instance if class is empty */
                wm_class = strdup(instance);
            }
            
            XFree(prop);
        }
    }

    /* If WM_CLASS failed, use window name as fallback */
    if (wm_class == NULL) {
        char *name = get_window_name(xwindow);
        if (name) {
            wm_class = name;
        } else {
            wm_class = strdup("Unknown");
        }
    }

    return wm_class;
}


/* Get window icon */
GdkPixbuf *get_window_icon(Window xwindow) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    GdkPixbuf *pixbuf = NULL;

    /* Method 1: Try _NET_WM_ICON (modern EWMH standard) */
    if (XGetWindowProperty(xdisplay, xwindow, net_wm_icon_atom, 0, 65536, False,
                           XA_CARDINAL, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop && actual_format == 32 && nitems > 2) {
            unsigned long *data = (unsigned long *)prop;
            int width = data[0];
            int height = data[1];
            int size = width * height;

            if (nitems >= (unsigned long)(size + 2) && width > 0 && height > 0 && width < 512 && height < 512) {
                /* Create pixbuf */
                pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
                guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

                for (int i = 0; i < size; i++) {
                    unsigned long argb = data[i + 2];
                    int r = (argb >> 16) & 0xFF;
                    int g = (argb >> 8) & 0xFF;
                    int b = argb & 0xFF;
                    int a = (argb >> 24) & 0xFF;

                    pixels[i * 4 + 0] = r;
                    pixels[i * 4 + 1] = g;
                    pixels[i * 4 + 2] = b;
                    pixels[i * 4 + 3] = a;
                }
            }
            XFree(prop);
            if (pixbuf != NULL)
                return pixbuf;
        } else if (prop) {
            XFree(prop);
        }
    }


    /* Method 2: Try WM_CLASS to lookup icon via GDesktopAppInfo (professional approach) */
    prop = NULL;
    if (XGetWindowProperty(xdisplay, xwindow, wm_class_atom, 0, 1024, False,
                           XA_STRING, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            /* WM_CLASS contains instance\0class\0 */
            char *instance = (char *)prop;
            char *class = instance + strlen(instance) + 1;
            
            GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
            GError *error = NULL;
            GDesktopAppInfo *app_info = NULL;
            gchar *icon_name = NULL;
            
            /* Try to find .desktop file using class name */
            gchar *desktop_id = g_strdup_printf("%s.desktop", class);
            app_info = g_desktop_app_info_new(desktop_id);
            g_free(desktop_id);
            
            /* If not found, try lowercase */
            if (app_info == NULL) {
                gchar *lowercase_class = g_ascii_strdown(class, -1);
                desktop_id = g_strdup_printf("%s.desktop", lowercase_class);
                app_info = g_desktop_app_info_new(desktop_id);
                g_free(desktop_id);
                g_free(lowercase_class);
            }
            
            /* If still not found, try instance name */
            if (app_info == NULL && strlen(instance) > 0) {
                desktop_id = g_strdup_printf("%s.desktop", instance);
                app_info = g_desktop_app_info_new(desktop_id);
                g_free(desktop_id);
            }
            
            /* If still not found, search for it */
            if (app_info == NULL) {
                gchar ***desktop_ids = g_desktop_app_info_search(class);
                if (desktop_ids != NULL && desktop_ids[0] != NULL) {
                    app_info = g_desktop_app_info_new(desktop_ids[0][0]);
                }
                
                if (desktop_ids != NULL) {
                    for (gchar ***p = desktop_ids; *p != NULL; p++)
                        g_strfreev(*p);
                    g_free(desktop_ids);
                }
            }
            
            /* Extract icon from .desktop file */
            if (app_info != NULL) {
                icon_name = g_desktop_app_info_get_string(app_info, "Icon");
                
                if (icon_name != NULL) {
                    /* Check if it's an absolute path */
                    if (g_path_is_absolute(icon_name)) {
                        pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_name, 32, 32, TRUE, &error);
                        if (error) {
                            g_error_free(error);
                            error = NULL;
                        }
                    } else {
                        /* Load from icon theme */
                        pixbuf = gtk_icon_theme_load_icon(icon_theme, icon_name, 32,
                                                         GTK_ICON_LOOKUP_FORCE_SIZE, &error);
                        if (error) {
                            g_error_free(error);
                            error = NULL;
                        }
                    }
                    
                    g_free(icon_name);
                }
                
                g_object_unref(app_info);
                
                if (pixbuf != NULL) {
                    XFree(prop);
                    return pixbuf;
                }
            }
            
            /* Fallback: try class/instance names directly in icon theme */
            pixbuf = gtk_icon_theme_load_icon(icon_theme, class, 32,
                                             GTK_ICON_LOOKUP_FORCE_SIZE, &error);
            if (error) {
                g_error_free(error);
                error = NULL;
            }
            
            if (pixbuf != NULL) {
                XFree(prop);
                return pixbuf;
            }
            
            gchar *lowercase_class = g_ascii_strdown(class, -1);
            pixbuf = gtk_icon_theme_load_icon(icon_theme, lowercase_class, 32,
                                             GTK_ICON_LOOKUP_FORCE_SIZE, &error);
            g_free(lowercase_class);
            if (error) {
                g_error_free(error);
                error = NULL;
            }
            
            if (pixbuf != NULL) {
                XFree(prop);
                return pixbuf;
            }
            
            if (strlen(instance) > 0) {
                gchar *lowercase_instance = g_ascii_strdown(instance, -1);
                pixbuf = gtk_icon_theme_load_icon(icon_theme, lowercase_instance, 32,
                                                 GTK_ICON_LOOKUP_FORCE_SIZE, &error);
                g_free(lowercase_instance);
                if (error) {
                    g_error_free(error);
                    error = NULL;
                }
                
                if (pixbuf != NULL) {
                    XFree(prop);
                    return pixbuf;
                }
            }
            
            XFree(prop);
        } else if (prop) {
            XFree(prop);
        }
    }

    /* Method 3: Use generic fallback icon */
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    GError *error = NULL;
    pixbuf = gtk_icon_theme_load_icon(icon_theme, "application-x-executable", 32,
                                     GTK_ICON_LOOKUP_FORCE_SIZE, &error);
    if (error) {
        g_error_free(error);
    }
    
    return pixbuf;
}

/* Activate window on click - cycles through grouped windows */
void on_button_clicked(GtkWidget *widget, gpointer data) {
    (void)data; /* Unused */
    
    WindowGroup *group = (WindowGroup *)g_object_get_data(G_OBJECT(widget), "group");
    if (group == NULL) {
        return;
    }
    
    int window_count = g_list_length(group->windows);
    
    /* If no windows, launch the app (for pinned apps) */
    if (window_count == 0) {
        if (group->desktop_file_path != NULL) {
            GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(group->desktop_file_path);
            if (app_info != NULL) {
                GError *error = NULL;
                GdkAppLaunchContext *context = gdk_display_get_app_launch_context(gdk_display_get_default());
                
                /* Custom Detached Launch Logic for Dock */
                const gchar *raw_cmd = g_app_info_get_commandline(G_APP_INFO(app_info));
                if (raw_cmd) {
                    gchar *cmd_line = g_strdup(raw_cmd);
                    
                    /* Simple stripper for common desktop entry field codes */
                    const char *codes[] = {"%f", "%u", "%F", "%U", "%i", "%c", "%k", NULL};
                    for (int i = 0; codes[i] != NULL; i++) {
                        gchar *found;
                        while ((found = strstr(cmd_line, codes[i])) != NULL) {
                            found[0] = ' ';
                            found[1] = ' ';
                        }
                    }
                    
                    gint argc;
                    gchar **argv;
                    if (g_shell_parse_argv(cmd_line, &argc, &argv, &error)) {
                        /* Reuse child_setup if defined or define a lambda-like local if C allows? No.
                           We need to define child_setup globally or share it. 
                           Since I cannot easily see if I added it globally yet, I'll rely on a forward declaration or assumes I added it.
                           Wait, I need to add child_setup function first. 
                           I'll add it along with unistd.h in a separate chunk? 
                           No, I can't add it in the middle easily.
                           I will assume I will add it at the top or use a local definition if GCC supports it (nested functions are a GNU extension).
                           Better: I will define it at file scope in a previous chunk.
                           Wait, I'll use a separate tool call or a multi-chunk.
                           I will add the helper function at the top of the file in the first chunk.
                        */
                         if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, child_setup, NULL, NULL, &error)) {
                             g_warning("Failed to spawn app detached in dock: %s", error->message);
                             g_error_free(error);
                             /* Fallback */
                             g_app_info_launch(G_APP_INFO(app_info), NULL, G_APP_LAUNCH_CONTEXT(context), NULL);
                        }
                        g_strfreev(argv);
                    }
                    g_free(cmd_line);
                } else {
                     g_app_info_launch(G_APP_INFO(app_info), NULL, G_APP_LAUNCH_CONTEXT(context), &error);
                }
                
                g_object_unref(context);
                g_object_unref(app_info);
            }
        }
        return;
    }
    
    /* Cycle to next window */
    group->active_index = (group->active_index + 1) % window_count;
    
    /* Get the window to activate */
    GList *window_node = g_list_nth(group->windows, group->active_index);
    if (window_node == NULL) {
        group->active_index = 0;
        window_node = g_list_first(group->windows);
    }
    
    Window win = GPOINTER_TO_INT(window_node->data);
    
    /* Send activation event */
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = net_active_window_atom;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 2; /* Source indication (2 = pager) */
    xev.xclient.data.l[1] = CurrentTime;
    xev.xclient.data.l[2] = 0;

    XSendEvent(xdisplay, root_window, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(xdisplay);
}

/* Filter X events to update list */
GdkFilterReturn event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data) {
    (void)event; (void)data; /* Unused */
    XEvent *xev = (XEvent *)xevent;

    if (xev->type == PropertyNotify) {
        if (xev->xproperty.atom == net_client_list_atom) {
            update_window_list();
            pager_update(); /* Windows changed, update previews potentially */
        }
        else if (xev->xproperty.atom == net_active_window_atom ||
                 xev->xproperty.atom == XInternAtom(xdisplay, "_NET_CURRENT_DESKTOP", False)) {
             /* Also update on active window or desktop switch */
             pager_update();
        }
    }

    return GDK_FILTER_CONTINUE;
}

/* Context menu and pinned apps functions */

/* Load pinned apps from config file */
void load_pinned_apps() {
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "simple-panel", NULL);
    gchar *config_file = g_build_filename(config_dir, "pinned-apps", NULL);
    
    g_mkdir_with_parents(config_dir, 0755);
    
    GError *error = NULL;
    gchar *contents = NULL;
    
    if (g_file_get_contents(config_file, &contents, NULL, &error)) {
        gchar **lines = g_strsplit(contents, "\n", -1);
        for (int i = 0; lines[i] != NULL; i++) {
            g_strstrip(lines[i]);
            if (strlen(lines[i]) > 0) {
                pinned_apps = g_list_append(pinned_apps, g_strdup(lines[i]));
            }
        }
        g_strfreev(lines);
        g_free(contents);
    }
    
    g_free(config_file);
    g_free(config_dir);
}

/* Save pinned apps to config file */
void save_pinned_apps() {
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "simple-panel", NULL);
    gchar *config_file = g_build_filename(config_dir, "pinned-apps", NULL);
    
    g_mkdir_with_parents(config_dir, 0755);
    
    GString *contents = g_string_new("");
    for (GList *l = pinned_apps; l != NULL; l = l->next) {
        g_string_append(contents, (gchar *)l->data);
        g_string_append_c(contents, '\n');
    }
    
    GError *error = NULL;
    if (!g_file_set_contents(config_file, contents->str, -1, &error)) {
        g_warning("Failed to save pinned apps: %s", error->message);
        g_error_free(error);
    }
    
    g_string_free(contents, TRUE);
    g_free(config_file);
    g_free(config_dir);
}

/* Pin/Unpin clicked */
void on_pin_clicked(GtkWidget *menuitem, gpointer data) {
    (void)menuitem;
    WindowGroup *group = (WindowGroup *)data;
    
    if (group->is_pinned) {
        /* Unpin */
        group->is_pinned = FALSE;
        
        /* Find and remove from pinned list */
        GList *found = g_list_find_custom(pinned_apps, group->wm_class, (GCompareFunc)g_strcmp0);
        if (found != NULL) {
            g_free(found->data);
            pinned_apps = g_list_delete_link(pinned_apps, found);
        }
    } else {
        /* Pin */
        group->is_pinned = TRUE;
        pinned_apps = g_list_append(pinned_apps, g_strdup(group->wm_class));
    }
    
    save_pinned_apps();
    update_window_list();
}

/* New Window clicked */
void on_new_window_clicked(GtkWidget *menuitem, gpointer data) {
    (void)menuitem;
    WindowGroup *group = (WindowGroup *)data;
    
    if (group->desktop_file_path != NULL) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(group->desktop_file_path);
        if (app_info != NULL) {
            GError *error = NULL;
            GdkAppLaunchContext *context = gdk_display_get_app_launch_context(gdk_display_get_default());
            
            if (!g_app_info_launch(G_APP_INFO(app_info), NULL, G_APP_LAUNCH_CONTEXT(context), &error)) {
                g_warning("Failed to launch app: %s", error->message);
                g_error_free(error);
            }
            
            g_object_unref(context);
            g_object_unref(app_info);
        }
    }
}

/* Close All Windows clicked */
void on_close_all_clicked(GtkWidget *menuitem, gpointer data) {
    (void)menuitem;
    WindowGroup *group = (WindowGroup *)data;
    Atom net_close_window = XInternAtom(xdisplay, "_NET_CLOSE_WINDOW", False);
    
    for (GList *l = group->windows; l != NULL; l = l->next) {
        Window win = GPOINTER_TO_INT(l->data);
        
        XEvent xev;
        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.xclient.window = win;
        xev.xclient.message_type = net_close_window;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = CurrentTime;
        xev.xclient.data.l[1] = 2; /* Source indication */
        
        XSendEvent(xdisplay, root_window, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    }
    
    XFlush(xdisplay);
}

/* Run with GPU clicked */
void on_run_with_gpu_clicked(GtkWidget *menuitem, gpointer data) {
    (void)menuitem;
    WindowGroup *group = (WindowGroup *)data;
    
    if (group->desktop_file_path != NULL) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(group->desktop_file_path);
        if (app_info != NULL) {
            /* Get the Exec line from desktop file */
            gchar *exec = g_desktop_app_info_get_string(app_info, "Exec");
            
            if (exec != NULL) {
                /* Remove field codes like %U, %F, etc. */
                gchar *clean_exec = g_strdup(exec);
                gchar *percent = g_strstr_len(clean_exec, -1, " %");
                if (percent != NULL) {
                    *percent = '\0';
                }
                
                /* Build command with GPU environment variables (no & needed - async by default) */
                gchar *gpu_command = g_strdup_printf("env DRI_PRIME=1 __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia %s", clean_exec);
                
                GError *error = NULL;
                if (!g_spawn_command_line_async(gpu_command, &error)) {
                    g_warning("Failed to launch with GPU: %s", error->message);
                    g_error_free(error);
                }
                
                g_free(gpu_command);
                g_free(clean_exec);
                g_free(exec);
            }
            
            g_object_unref(app_info);
        }
    }
}

/* Create context menu */
void create_context_menu(WindowGroup *group, GdkEventButton *event) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;
    
    /* Pin/Unpin */
    if (group->is_pinned) {
        item = gtk_menu_item_new_with_label("Unpin from Dock");
    } else {
        item = gtk_menu_item_new_with_label("Pin to Dock");
    }
    g_signal_connect(item, "activate", G_CALLBACK(on_pin_clicked), group);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    
    /* Separator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    
    /* New Window */
    if (group->desktop_file_path != NULL) {
        item = gtk_menu_item_new_with_label("New Window");
        g_signal_connect(item, "activate", G_CALLBACK(on_new_window_clicked), group);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    
    /* Close All Windows */
    if (group->windows != NULL && g_list_length(group->windows) > 0) {
        gchar *label = g_strdup_printf("Close All (%d)", g_list_length(group->windows));
        item = gtk_menu_item_new_with_label(label);
        g_free(label);
        g_signal_connect(item, "activate", G_CALLBACK(on_close_all_clicked), group);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    
    /* Separator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    
    /* Run with GPU */
    if (group->desktop_file_path != NULL) {
        item = gtk_menu_item_new_with_label("Run with Dedicated GPU");
        g_signal_connect(item, "activate", G_CALLBACK(on_run_with_gpu_clicked), group);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
}

/* Button press event handler */
gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)data;
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) { /* Right click */
        WindowGroup *group = (WindowGroup *)g_object_get_data(G_OBJECT(widget), "group");
        if (group != NULL) {
            create_context_menu(group, event);
            return TRUE;
        }
    }
    return FALSE;
}
