/*
 * venom-desktop-manager.c
 * Pro Desktop: Free Positioning, System Clipboard, Transparent Background.
 *
 * Compile:
 * gcc -o venom-desktop-manager venom-desktop-manager.c $(pkg-config --cflags --libs gtk+-3.0 gio-2.0 gdesktop-app-info-1.0) -lm
 */

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <unistd.h>
#include <glib/gstdio.h>

#define ICON_SIZE 48
#define ITEM_WIDTH 80
#define ITEM_HEIGHT 100
#define GRID_X 20
#define GRID_Y 20
#define CONFIG_FILE "/home/x/.config/venom/desktop-items.ini"

/* --- Global Variables --- */
static double start_x = 0, start_y = 0;
static double current_x = 0, current_y = 0;
static gboolean is_selecting = FALSE;
static GtkWidget *selection_layer = NULL;
static GtkWidget *icon_layout = NULL;
static int screen_w = 0;
static int screen_h = 0;

/* Dragging State */
static GtkWidget *dragged_item = NULL;
static double drag_start_x = 0, drag_start_y = 0;
static int item_start_x = 0, item_start_y = 0;
static gboolean is_dragging_icon = FALSE;

/* --- Helper Functions --- */

static void ensure_config_dir() {
    g_mkdir_with_parents("/home/x/.config/venom", 0755);
}

static void save_item_position(const char *filename, int x, int y) {
    ensure_config_dir();
    GKeyFile *key_file = g_key_file_new();
    g_key_file_load_from_file(key_file, CONFIG_FILE, G_KEY_FILE_NONE, NULL);
    
    g_key_file_set_integer(key_file, "Positions", filename, x);
    g_key_file_set_integer(key_file, "Positions", g_strdup_printf("%s_y", filename), y);
    
    g_key_file_save_to_file(key_file, CONFIG_FILE, NULL);
    g_key_file_free(key_file);
}

static gboolean get_item_position(const char *filename, int *x, int *y) {
    GKeyFile *key_file = g_key_file_new();
    if (!g_key_file_load_from_file(key_file, CONFIG_FILE, G_KEY_FILE_NONE, NULL)) {
        g_key_file_free(key_file);
        return FALSE;
    }
    
    GError *err = NULL;
    int px = g_key_file_get_integer(key_file, "Positions", filename, &err);
    if (err) { g_error_free(err); g_key_file_free(key_file); return FALSE; }
    
    int py = g_key_file_get_integer(key_file, "Positions", g_strdup_printf("%s_y", filename), &err);
    if (err) { g_error_free(err); g_key_file_free(key_file); return FALSE; }
    
    *x = px;
    *y = py;
    g_key_file_free(key_file);
    return TRUE;
}

static void open_file_uri(const char *uri) {
    GError *err = NULL;
    GFile *file = g_file_new_for_uri(uri);
    char *path = g_file_get_path(file);
    
    if (path && g_str_has_suffix(path, ".desktop")) {
        GDesktopAppInfo *app = g_desktop_app_info_new_from_filename(path);
        if (app) {
            g_app_info_launch(G_APP_INFO(app), NULL, NULL, &err);
            g_object_unref(app);
        } else {
             g_app_info_launch_default_for_uri(uri, NULL, &err);
        }
    } else {
        g_app_info_launch_default_for_uri(uri, NULL, &err);
    }
    
    if (err) {
        g_printerr("Error launching: %s\n", err->message);
        g_error_free(err);
    }
    g_free(path);
    g_object_unref(file);
}

static void refresh_icons();

static void delete_file(const char *uri) {
    GFile *file = g_file_new_for_uri(uri);
    GError *err = NULL;
    if (g_file_trash(file, NULL, &err)) {
        refresh_icons();
    } else {
        g_warning("Trash failed: %s", err->message);
        g_error_free(err);
    }
    g_object_unref(file);
}

/* --- System Clipboard --- */

static void clipboard_get_func(GtkClipboard *clipboard, GtkSelectionData *selection_data, guint info, gpointer user_data_or_owner) {
    char *uri = (char *)user_data_or_owner;
    if (info == 1) { /* text/uri-list */
        gtk_selection_data_set(selection_data, gtk_selection_data_get_target(selection_data), 8, (guchar *)uri, strlen(uri));
    } else if (info == 2) { /* x-special/gnome-copied-files */
        /* Format: action\nuri (action: copy or cut) */
        /* We'll default to copy for now as we don't track cut state globally in this simple implementation for the callback */
        /* To support cut properly, we'd need to pass a struct with action and uri */
        char *data = g_strdup_printf("copy\n%s", uri);
        gtk_selection_data_set(selection_data, gtk_selection_data_get_target(selection_data), 8, (guchar *)data, strlen(data));
        g_free(data);
    }
}

static void clipboard_clear_func(GtkClipboard *clipboard, gpointer user_data_or_owner) {
    g_free(user_data_or_owner);
}

static void copy_file_to_clipboard(const char *uri, gboolean is_cut) {
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetEntry targets[] = {
        { "text/uri-list", 0, 1 },
        { "x-special/gnome-copied-files", 0, 2 }
    };
    
    /* Note: For full cut support we need to pass the state. For now, we simplify. */
    char *uri_copy = g_strdup(uri); /* Passed to clear_func */
    gtk_clipboard_set_with_data(clipboard, targets, 2, clipboard_get_func, clipboard_clear_func, uri_copy);
}

static void on_paste_received(GtkClipboard *clipboard, GtkSelectionData *selection_data, gpointer data) {
    if (!selection_data || gtk_selection_data_get_length(selection_data) <= 0) return;

    gchar *content = (gchar *)gtk_selection_data_get_data(selection_data);
    gchar **lines = g_strsplit(content, "\n", -1);
    
    /* Handle x-special/gnome-copied-files (action\nuri) or text/uri-list (uri\nuri...) */
    gboolean is_cut = FALSE;
    int start_idx = 0;

    if (g_strcmp0(lines[0], "cut") == 0) {
        is_cut = TRUE;
        start_idx = 1;
    } else if (g_strcmp0(lines[0], "copy") == 0) {
        start_idx = 1;
    }

    for (int i = start_idx; lines[i] != NULL; i++) {
        gchar *uri = g_strstrip(lines[i]);
        if (strlen(uri) == 0) continue;
        if (g_str_has_prefix(uri, "#")) continue; /* Skip comments */

        GFile *src = g_file_new_for_uri(uri);
        char *basename = g_file_get_basename(src);
        char *dest_path = g_strdup_printf("%s/Desktop/%s", g_get_home_dir(), basename);
        GFile *dest = g_file_new_for_path(dest_path);
        
        GError *err = NULL;
        if (is_cut) {
            g_file_move(src, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, &err);
        } else {
            g_file_copy(src, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, &err);
        }

        if (err) {
            g_warning("Paste error: %s", err->message);
            g_error_free(err);
        }
        
        g_free(basename);
        g_free(dest_path);
        g_object_unref(src);
        g_object_unref(dest);
    }
    g_strfreev(lines);
    refresh_icons();
}

static void paste_from_clipboard() {
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    /* Try gnome format first for cut/copy distinction, then uri-list */
    if (gtk_clipboard_wait_is_target_available(clipboard, gdk_atom_intern("x-special/gnome-copied-files", FALSE))) {
        gtk_clipboard_request_contents(clipboard, gdk_atom_intern("x-special/gnome-copied-files", FALSE), on_paste_received, NULL);
    } else {
        gtk_clipboard_request_contents(clipboard, gdk_atom_intern("text/uri-list", FALSE), on_paste_received, NULL);
    }
}

/* --- Context Menus --- */

static void on_item_rename(GtkWidget *menuitem, gpointer data) {
    char *uri = (char *)data;
    GFile *file = g_file_new_for_uri(uri);
    char *name = g_file_get_basename(file);
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Rename", NULL, GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL, "_Rename", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), name);
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dialog);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (strlen(new_name) > 0) {
            GFile *new_file = g_file_set_display_name(file, new_name, NULL, NULL);
            if (new_file) {
                g_object_unref(new_file);
                refresh_icons();
            }
        }
    }
    gtk_widget_destroy(dialog);
    g_free(name);
    g_object_unref(file);
}

static void on_item_cut(GtkWidget *menuitem, gpointer data) { copy_file_to_clipboard((char*)data, TRUE); }
static void on_item_copy(GtkWidget *menuitem, gpointer data) { copy_file_to_clipboard((char*)data, FALSE); }
static void on_item_open(GtkWidget *menuitem, gpointer data) { open_file_uri((char*)data); }
static void on_item_delete(GtkWidget *menuitem, gpointer data) { delete_file((char*)data); }

static gboolean on_item_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    char *uri = (char *)data;

    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkWidget *menu = gtk_menu_new();
        
        GtkWidget *i_open = gtk_menu_item_new_with_label("Open");
        GtkWidget *i_cut = gtk_menu_item_new_with_label("Cut");
        GtkWidget *i_copy = gtk_menu_item_new_with_label("Copy");
        GtkWidget *i_rename = gtk_menu_item_new_with_label("Rename");
        GtkWidget *i_del = gtk_menu_item_new_with_label("Move to Trash");
        
        g_signal_connect(i_open, "activate", G_CALLBACK(on_item_open), uri);
        g_signal_connect(i_cut, "activate", G_CALLBACK(on_item_cut), uri);
        g_signal_connect(i_copy, "activate", G_CALLBACK(on_item_copy), uri);
        g_signal_connect(i_rename, "activate", G_CALLBACK(on_item_rename), uri);
        g_signal_connect(i_del, "activate", G_CALLBACK(on_item_delete), uri);
        
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_open);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_cut);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_copy);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_rename);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_del);
        
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
        return TRUE;
    }
    
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
        open_file_uri(uri);
        return TRUE;
    }
    
    /* Start Dragging */
    if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
        is_dragging_icon = TRUE;
        dragged_item = widget;
        drag_start_x = event->x_root;
        drag_start_y = event->y_root;
        gtk_container_child_get(GTK_CONTAINER(icon_layout), widget, "x", &item_start_x, "y", &item_start_y, NULL);
        return TRUE; /* Consume event to prevent propagation issues */
    }

    return FALSE;
}

static gboolean on_item_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (is_dragging_icon && dragged_item == widget) {
        int dx = event->x_root - drag_start_x;
        int dy = event->y_root - drag_start_y;
        int new_x = item_start_x + dx;
        int new_y = item_start_y + dy;
        
        gtk_layout_move(GTK_LAYOUT(icon_layout), widget, new_x, new_y);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_item_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (is_dragging_icon && event->button == 1) {
        is_dragging_icon = FALSE;
        dragged_item = NULL;
        
        int x, y;
        gtk_container_child_get(GTK_CONTAINER(icon_layout), widget, "x", &x, "y", &y, NULL);
        
        /* Snap to grid (optional, simple snapping here) */
        // x = (x / 10) * 10;
        // y = (y / 10) * 10;
        // gtk_layout_move(GTK_LAYOUT(icon_layout), widget, x, y);
        
        /* Save position */
        GFile *f = g_file_new_for_uri((char*)data);
        char *filename = g_file_get_basename(f);
        save_item_position(filename, x, y);
        g_free(filename);
        g_object_unref(f);
        
        return TRUE;
    }
    return FALSE;
}

/* Desktop Background Context Menu */

static void on_bg_paste(GtkWidget *item, gpointer data) { paste_from_clipboard(); }

static void on_create_folder(GtkWidget *item, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Create Folder", GTK_WINDOW(data),
                                                    GTK_DIALOG_MODAL,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Create", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (strlen(name) > 0) {
            char *path = g_strdup_printf("%s/Desktop/%s", g_get_home_dir(), name);
            g_mkdir(path, 0755);
            g_free(path);
            refresh_icons();
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_open_terminal(GtkWidget *item, gpointer data) {
    char *cmd = g_strdup_printf("exo-open --launch TerminalEmulator --working-directory=%s/Desktop", g_get_home_dir());
    g_spawn_command_line_async(cmd, NULL);
    g_free(cmd);
}

static void on_refresh_clicked(GtkWidget *item, gpointer data) {
    refresh_icons();
}

/* --- Icon Loading --- */

static GtkWidget* create_desktop_item(GFileInfo *info, const char *full_path) {
    const char *filename = g_file_info_get_name(info);
    char *display_name = g_strdup(g_file_info_get_display_name(info));
    GIcon *gicon = g_object_ref(g_file_info_get_icon(info));
    
    if (g_str_has_suffix(filename, ".desktop")) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(full_path);
        if (app_info) {
            g_free(display_name);
            display_name = g_strdup(g_app_info_get_name(G_APP_INFO(app_info)));
            if (gicon) g_object_unref(gicon);
            gicon = g_app_info_get_icon(G_APP_INFO(app_info));
            if (gicon) g_object_ref(gicon);
            g_object_unref(app_info);
        }
    }

    GtkWidget *btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_widget_set_name(btn, "desktop-item");
    gtk_widget_set_size_request(btn, ITEM_WIDTH, ITEM_HEIGHT);
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(btn), box);

    GtkWidget *image = gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(image), ICON_SIZE);
    gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);

    GtkWidget *label = gtk_label_new(display_name);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 12);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

    GFile *f = g_file_new_for_path(full_path);
    char *uri = g_file_get_uri(f);
    
    /* Connect Events */
    gtk_widget_add_events(btn, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect_data(btn, "button-press-event", G_CALLBACK(on_item_button_press), uri, NULL, 0);
    g_signal_connect(btn, "motion-notify-event", G_CALLBACK(on_item_motion), NULL);
    g_signal_connect_data(btn, "button-release-event", G_CALLBACK(on_item_button_release), uri, (GClosureNotify)g_free, 0);
    
    g_object_unref(f);
    g_free(display_name);
    if (gicon) g_object_unref(gicon);

    gtk_widget_show_all(btn);
    return btn;
}

static void refresh_icons() {
    /* Clear existing children */
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(icon_layout));
    for (iter = children; iter != NULL; iter = g_list_next(iter))
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    g_list_free(children);

    const char *home = g_get_home_dir();
    char *desktop_path = g_strdup_printf("%s/Desktop", home);
    GFile *dir = g_file_new_for_path(desktop_path);
    
    GFileEnumerator *enumerator = g_file_enumerate_children(dir,
        "standard::*", G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (enumerator) {
        GFileInfo *info;
        int grid_x = GRID_X;
        int grid_y = GRID_Y;
        
        while ((info = g_file_enumerator_next_file(enumerator, NULL, NULL))) {
            const char *fname = g_file_info_get_name(info);
            if (fname[0] != '.') {
                char *full_path = g_strdup_printf("%s/%s", desktop_path, fname);
                GtkWidget *item = create_desktop_item(info, full_path);
                
                int x, y;
                if (get_item_position(fname, &x, &y)) {
                    gtk_layout_put(GTK_LAYOUT(icon_layout), item, x, y);
                } else {
                    /* Auto-placement */
                    gtk_layout_put(GTK_LAYOUT(icon_layout), item, grid_x, grid_y);
                    grid_y += ITEM_HEIGHT + 10;
                    if (grid_y > screen_h - ITEM_HEIGHT) {
                        grid_y = GRID_Y;
                        grid_x += ITEM_WIDTH + 10;
                    }
                }
                
                g_free(full_path);
            }
            g_object_unref(info);
        }
        g_object_unref(enumerator);
    }
    g_object_unref(dir);
    g_free(desktop_path);
}

/* --- Drawing & Events --- */

static gboolean on_selection_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    /* Clear background to make it transparent */
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* Draw Selection Box */
    if (is_selecting) {
        double x = MIN(start_x, current_x);
        double y = MIN(start_y, current_y);
        double w = fabs(current_x - start_x);
        double h = fabs(current_y - start_y);

        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.3);
        cairo_rectangle(cr, x, y, w, h);
        cairo_fill(cr);

        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.8);
        cairo_set_line_width(cr, 1.0);
        cairo_rectangle(cr, x, y, w, h);
        cairo_stroke(cr);
    }
    return FALSE;
}

static gboolean on_bg_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1) {
        is_selecting = TRUE;
        start_x = event->x;
        start_y = event->y;
        current_x = event->x;
        current_y = event->y;
        gtk_widget_queue_draw(selection_layer);
        return TRUE;
    }
    if (event->button == 3) {
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *new_folder = gtk_menu_item_new_with_label("Create Folder");
        GtkWidget *term = gtk_menu_item_new_with_label("Open Terminal Here");
        GtkWidget *paste = gtk_menu_item_new_with_label("Paste");
        GtkWidget *sep1 = gtk_separator_menu_item_new();
        GtkWidget *refresh = gtk_menu_item_new_with_label("Refresh");
        GtkWidget *sep2 = gtk_separator_menu_item_new();
        GtkWidget *quit = gtk_menu_item_new_with_label("Quit Desktop");

        g_signal_connect(new_folder, "activate", G_CALLBACK(on_create_folder), widget);
        g_signal_connect(term, "activate", G_CALLBACK(on_open_terminal), NULL);
        g_signal_connect(paste, "activate", G_CALLBACK(on_bg_paste), NULL);
        g_signal_connect(refresh, "activate", G_CALLBACK(on_refresh_clicked), NULL);
        g_signal_connect(quit, "activate", G_CALLBACK(gtk_main_quit), NULL);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_folder);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), term);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep1);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), refresh);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep2);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_bg_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (is_selecting) {
        current_x = event->x;
        current_y = event->y;
        gtk_widget_queue_draw(selection_layer);
    }
    return FALSE;
}

static gboolean on_bg_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1 && is_selecting) {
        is_selecting = FALSE;
        gtk_widget_queue_draw(selection_layer);
    }
    return FALSE;
}

/* --- Main --- */

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Venom Pro Desktop");
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DESKTOP);
    
    GdkScreen *screen = gtk_widget_get_screen(window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(window, visual);
        gtk_widget_set_app_paintable(window, TRUE);
    }

    GdkRectangle r;
    gdk_monitor_get_geometry(gdk_display_get_primary_monitor(gdk_display_get_default()), &r);
    screen_w = r.width;
    screen_h = r.height;
    gtk_window_set_default_size(GTK_WINDOW(window), screen_w, screen_h);
    gtk_window_move(GTK_WINDOW(window), 0, 0);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(window), overlay);

    /* 1. Background Event Area */
    selection_layer = gtk_drawing_area_new();
    gtk_widget_set_hexpand(selection_layer, TRUE);
    gtk_widget_set_vexpand(selection_layer, TRUE);
    gtk_widget_add_events(selection_layer, 
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    
    g_signal_connect(selection_layer, "draw", G_CALLBACK(on_selection_draw), NULL);
    g_signal_connect(selection_layer, "button-press-event", G_CALLBACK(on_bg_button_press), NULL);
    g_signal_connect(selection_layer, "motion-notify-event", G_CALLBACK(on_bg_motion), NULL);
    g_signal_connect(selection_layer, "button-release-event", G_CALLBACK(on_bg_button_release), NULL);

    gtk_container_add(GTK_CONTAINER(overlay), selection_layer);

    /* 2. Icons Layer (GtkLayout for free positioning) */
    icon_layout = gtk_layout_new(NULL, NULL);
    gtk_widget_set_size_request(icon_layout, screen_w, screen_h);
    
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, 
        "window { background-color: transparent; }"
        "#desktop-item { background: transparent; border-radius: 5px; padding: 8px; transition: all 0.1s; }"
        "#desktop-item:hover { background: rgba(255,255,255,0.15); }"
        "#desktop-item:selected { background: rgba(52, 152, 219, 0.4); }"
        "label { color: white; text-shadow: 1px 1px 2px black; font-weight: bold; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css), 800);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), icon_layout);

    refresh_icons();

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
    malloc_trim(0);
    gtk_main();

    return 0;
}