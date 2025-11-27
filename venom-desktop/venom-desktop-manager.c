/*
 * venom-desktop-manager.c
 * Pro Desktop: Free Positioning, Smart Grid (No Stacking), Recursive Copy, System DnD.
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
static GtkWidget *main_window = NULL;
static GtkWidget *icon_layout = NULL;
static int screen_w = 0;
static int screen_h = 0;

/* Selection State */
static double start_x = 0, start_y = 0;
static double current_x = 0, current_y = 0;
static gboolean is_selecting = FALSE;
static GList *selected_items = NULL;

/* Drag State (System DnD) */
static double drag_start_x_root = 0;
static double drag_start_y_root = 0;
static GHashTable *drag_initial_positions = NULL; 

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

/* --- Recursive File Operations --- */

static gboolean recursive_copy_move(GFile *src, GFile *dest, gboolean is_move, GError **error) {
    GFileInfo *info = g_file_query_info(src, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!info) return FALSE;
    
    GFileType type = g_file_info_get_file_type(info);
    g_object_unref(info);

    if (type == G_FILE_TYPE_DIRECTORY) {
        if (!g_file_make_directory(dest, NULL, error)) {
            /* Ignore error if directory exists */
            if (!g_error_matches(*error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
                return FALSE;
            }
            g_clear_error(error);
        }

        GFileEnumerator *enumerator = g_file_enumerate_children(src, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE, NULL, error);
        if (!enumerator) return FALSE;

        GFileInfo *child_info;
        while ((child_info = g_file_enumerator_next_file(enumerator, NULL, NULL))) {
            const char *name = g_file_info_get_name(child_info);
            GFile *child_src = g_file_get_child(src, name);
            GFile *child_dest = g_file_get_child(dest, name);
            
            if (!recursive_copy_move(child_src, child_dest, is_move, error)) {
                g_object_unref(child_src);
                g_object_unref(child_dest);
                g_object_unref(child_info);
                g_object_unref(enumerator);
                return FALSE;
            }
            
            g_object_unref(child_src);
            g_object_unref(child_dest);
            g_object_unref(child_info);
        }
        g_object_unref(enumerator);

        if (is_move) {
            return g_file_delete(src, NULL, error);
        }
    } else {
        if (is_move) {
            return g_file_move(src, dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error);
        } else {
            return g_file_copy(src, dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error);
        }
    }
    return TRUE;
}

/* --- Selection Logic --- */

static gboolean is_selected(GtkWidget *item) {
    return g_list_find(selected_items, item) != NULL;
}

static void select_item(GtkWidget *item) {
    if (!is_selected(item)) {
        selected_items = g_list_append(selected_items, item);
        GtkStyleContext *context = gtk_widget_get_style_context(item);
        gtk_style_context_add_class(context, "selected");
        gtk_widget_queue_draw(item);
    }
}

static void deselect_item(GtkWidget *item) {
    GList *l = g_list_find(selected_items, item);
    if (l) {
        selected_items = g_list_delete_link(selected_items, l);
        GtkStyleContext *context = gtk_widget_get_style_context(item);
        gtk_style_context_remove_class(context, "selected");
        gtk_widget_queue_draw(item);
    }
}

static void deselect_all() {
    for (GList *l = selected_items; l != NULL; l = l->next) {
        GtkWidget *item = GTK_WIDGET(l->data);
        if (GTK_IS_WIDGET(item)) {
            GtkStyleContext *context = gtk_widget_get_style_context(item);
            gtk_style_context_remove_class(context, "selected");
            gtk_widget_queue_draw(item);
        }
    }
    g_list_free(selected_items);
    selected_items = NULL;
}

/* --- File Operations --- */

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
    char *uri_list = (char *)user_data_or_owner;
    if (info == 1) { /* text/uri-list */
        gtk_selection_data_set(selection_data, gtk_selection_data_get_target(selection_data), 8, (guchar *)uri_list, strlen(uri_list));
    } else if (info == 2) { /* x-special/gnome-copied-files */
        char *data = g_strdup_printf("copy\n%s", uri_list);
        gtk_selection_data_set(selection_data, gtk_selection_data_get_target(selection_data), 8, (guchar *)data, strlen(data));
        g_free(data);
    }
}

static void clipboard_clear_func(GtkClipboard *clipboard, gpointer user_data_or_owner) {
    g_free(user_data_or_owner);
}

static void copy_selection_to_clipboard(gboolean is_cut) {
    if (!selected_items) return;

    GString *uri_list = g_string_new("");
    for (GList *l = selected_items; l != NULL; l = l->next) {
        GtkWidget *item = GTK_WIDGET(l->data);
        char *uri = (char *)g_object_get_data(G_OBJECT(item), "uri");
        if (uri) {
            g_string_append_printf(uri_list, "%s\r\n", uri);
        }
    }

    if (uri_list->len == 0) {
        g_string_free(uri_list, TRUE);
        return;
    }

    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetEntry targets[] = {
        { "text/uri-list", 0, 1 },
        { "x-special/gnome-copied-files", 0, 2 }
    };
    
    char *final_list = g_string_free(uri_list, FALSE);
    gtk_clipboard_set_with_data(clipboard, targets, 2, clipboard_get_func, clipboard_clear_func, final_list);
}

static void on_paste_received(GtkClipboard *clipboard, GtkSelectionData *selection_data, gpointer data) {
    if (!selection_data || gtk_selection_data_get_length(selection_data) <= 0) return;

    gchar *content = (gchar *)gtk_selection_data_get_data(selection_data);
    gchar **lines = g_strsplit(content, "\n", -1);
    
    gboolean is_cut = FALSE;
    int start_idx = 0;

    if (lines[0] && g_strcmp0(lines[0], "cut") == 0) {
        is_cut = TRUE;
        start_idx = 1;
    } else if (lines[0] && g_strcmp0(lines[0], "copy") == 0) {
        start_idx = 1;
    }

    for (int i = start_idx; lines[i] != NULL; i++) {
        gchar *uri = g_strstrip(lines[i]);
        if (strlen(uri) == 0) continue;
        if (g_str_has_prefix(uri, "#")) continue;

        GFile *src = g_file_new_for_uri(uri);
        char *basename = g_file_get_basename(src);
        char *dest_path = g_strdup_printf("%s/Desktop/%s", g_get_home_dir(), basename);
        GFile *dest = g_file_new_for_path(dest_path);
        
        GError *err = NULL;
        recursive_copy_move(src, dest, is_cut, &err);

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
    if (gtk_clipboard_wait_is_target_available(clipboard, gdk_atom_intern("x-special/gnome-copied-files", FALSE))) {
        gtk_clipboard_request_contents(clipboard, gdk_atom_intern("x-special/gnome-copied-files", FALSE), on_paste_received, NULL);
    } else {
        gtk_clipboard_request_contents(clipboard, gdk_atom_intern("text/uri-list", FALSE), on_paste_received, NULL);
    }
}

/* --- Drag and Drop (Unified) --- */

static void on_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer data) {
    /* Find the image child to use as drag icon */
    GtkWidget *box = gtk_bin_get_child(GTK_BIN(widget));
    if (box) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(box));
        GtkWidget *image = NULL;
        for (GList *l = children; l != NULL; l = l->next) {
            if (GTK_IS_IMAGE(l->data)) {
                image = GTK_WIDGET(l->data);
                break;
            }
        }
        
        if (image) {
            switch (gtk_image_get_storage_type(GTK_IMAGE(image))) {
                case GTK_IMAGE_PIXBUF: {
                    GdkPixbuf *pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));
                    if (pixbuf) gtk_drag_set_icon_pixbuf(context, pixbuf, 24, 24);
                    break;
                }
                default:
                    gtk_drag_set_icon_default(context);
                    break;
            }
        } else {
            gtk_drag_set_icon_default(context);
        }
        if (children) g_list_free(children);
    } else {
        gtk_drag_set_icon_default(context);
    }
}

static void on_drag_data_get(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *data, guint info, guint time, gpointer user_data) {
    GString *uri_list = g_string_new("");
    
    if (is_selected(widget)) {
        for (GList *l = selected_items; l != NULL; l = l->next) {
            GtkWidget *item = GTK_WIDGET(l->data);
            char *uri = (char *)g_object_get_data(G_OBJECT(item), "uri");
            if (uri) g_string_append_printf(uri_list, "%s\r\n", uri);
        }
    } else {
        char *uri = (char *)g_object_get_data(G_OBJECT(widget), "uri");
        if (uri) g_string_append_printf(uri_list, "%s\r\n", uri);
    }

    gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8, (guchar *)uri_list->str, uri_list->len);
    g_string_free(uri_list, TRUE);
}

/* Drop on Folder (Move file into folder) */
static void on_folder_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data) {
    char *folder_uri = (char *)user_data;
    GFile *folder = g_file_new_for_uri(folder_uri);
    char *folder_path = g_file_get_path(folder);
    
    if (folder_path) {
        gchar **uris = g_uri_list_extract_uris((const gchar *)gtk_selection_data_get_data(data));
        if (uris) {
            for (int i = 0; uris[i] != NULL; i++) {
                GFile *src = g_file_new_for_uri(uris[i]);
                char *basename = g_file_get_basename(src);
                char *dest_path = g_strdup_printf("%s/%s", folder_path, basename);
                GFile *dest = g_file_new_for_path(dest_path);
                
                GError *err = NULL;
                recursive_copy_move(src, dest, TRUE, &err); /* Always move for DnD to folder? Or Copy? Standard is Move on same fs. */
                
                if (err) {
                    g_warning("DnD Move failed: %s", err->message);
                    g_error_free(err);
                }
                
                g_free(basename);
                g_free(dest_path);
                g_object_unref(src);
                g_object_unref(dest);
            }
            g_strfreev(uris);
            refresh_icons();
        }
        g_free(folder_path);
    }
    g_object_unref(folder);
    gtk_drag_finish(context, TRUE, FALSE, time);
}

/* Drop on Desktop Background (Move Icon or Import File) */
static void on_bg_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data) {
    GtkWidget *source_widget = gtk_drag_get_source_widget(context);
    
    /* Case 1: Internal Move (Moving icons on desktop) */
    if (source_widget && GTK_IS_WIDGET(source_widget) && 
        g_strcmp0(gtk_widget_get_name(source_widget), "desktop-item") == 0) {
        
        double delta_x = x - drag_start_x_root;
        double delta_y = y - drag_start_y_root;

        if (drag_initial_positions) {
            GHashTableIter iter;
            gpointer key, value;
            g_hash_table_iter_init(&iter, drag_initial_positions);
            while (g_hash_table_iter_next(&iter, &key, &value)) {
                GtkWidget *item = GTK_WIDGET(key);
                int *start_pos = (int *)value;
                int new_x = start_pos[0] + delta_x;
                int new_y = start_pos[1] + delta_y;
                
                gtk_layout_move(GTK_LAYOUT(icon_layout), item, new_x, new_y);
                
                /* Save new position */
                char *uri = (char *)g_object_get_data(G_OBJECT(item), "uri");
                if (uri) {
                    GFile *f = g_file_new_for_uri(uri);
                    char *filename = g_file_get_basename(f);
                    save_item_position(filename, new_x, new_y);
                    g_free(filename);
                    g_object_unref(f);
                }
            }
        }
        gtk_drag_finish(context, TRUE, FALSE, time);
        return;
    }

    /* Case 2: External Drop (Importing files) */
    gchar **uris = g_uri_list_extract_uris((const gchar *)gtk_selection_data_get_data(data));
    if (uris) {
        for (int i = 0; uris[i] != NULL; i++) {
            GFile *src = g_file_new_for_uri(uris[i]);
            char *basename = g_file_get_basename(src);
            char *dest_path = g_strdup_printf("%s/Desktop/%s", g_get_home_dir(), basename);
            GFile *dest = g_file_new_for_path(dest_path);
            
            GError *err = NULL;
            recursive_copy_move(src, dest, FALSE, &err); /* Copy by default for import */
            
            if (err) {
                g_warning("Import failed: %s", err->message);
                g_error_free(err);
            }
            
            g_free(basename);
            g_free(dest_path);
            g_object_unref(src);
            g_object_unref(dest);
        }
        g_strfreev(uris);
        refresh_icons();
    }
    gtk_drag_finish(context, TRUE, FALSE, time);
}

/* --- Context Menus --- */

static void on_item_rename(GtkWidget *menuitem, gpointer data) {
    char *uri = (char *)data;
    GFile *file = g_file_new_for_uri(uri);
    char *name = g_file_get_basename(file);
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Rename", GTK_WINDOW(main_window), GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL, "_Rename", GTK_RESPONSE_ACCEPT, NULL);
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

static void on_item_cut(GtkWidget *menuitem, gpointer data) { copy_selection_to_clipboard(TRUE); }
static void on_item_copy(GtkWidget *menuitem, gpointer data) { copy_selection_to_clipboard(FALSE); }
static void on_item_open(GtkWidget *menuitem, gpointer data) { open_file_uri((char*)data); }
static void on_item_delete(GtkWidget *menuitem, gpointer data) { 
    for (GList *l = selected_items; l != NULL; l = l->next) {
        char *uri = (char *)g_object_get_data(G_OBJECT(l->data), "uri");
        delete_file(uri);
    }
}

static gboolean on_item_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    char *uri = (char *)data;

    /* Left Click */
    if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
        if (!is_selected(widget)) {
            if (!(event->state & GDK_CONTROL_MASK)) {
                deselect_all();
            }
            select_item(widget);
        }
        
        /* Capture Start Position for DnD Move Logic */
        drag_start_x_root = event->x_root;
        drag_start_y_root = event->y_root;
        
        if (drag_initial_positions) {
            GHashTableIter iter;
            gpointer key, value;
            g_hash_table_iter_init(&iter, drag_initial_positions);
            while (g_hash_table_iter_next(&iter, &key, &value)) g_free(value);
            g_hash_table_destroy(drag_initial_positions);
        }
        drag_initial_positions = g_hash_table_new(g_direct_hash, g_direct_equal);
        
        for (GList *l = selected_items; l != NULL; l = l->next) {
            GtkWidget *item = GTK_WIDGET(l->data);
            int x, y;
            gtk_container_child_get(GTK_CONTAINER(icon_layout), item, "x", &x, "y", &y, NULL);
            int *pos = g_new(int, 2);
            pos[0] = x;
            pos[1] = y;
            g_hash_table_insert(drag_initial_positions, item, pos);
        }
        
        return FALSE; /* Propagate to allow GTK DnD to start */
    }

    /* Right Click */
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        if (!is_selected(widget)) {
            deselect_all();
            select_item(widget);
        }

        GtkWidget *menu = gtk_menu_new();
        GtkWidget *i_open = gtk_menu_item_new_with_label("Open");
        GtkWidget *i_cut = gtk_menu_item_new_with_label("Cut");
        GtkWidget *i_copy = gtk_menu_item_new_with_label("Copy");
        GtkWidget *i_rename = gtk_menu_item_new_with_label("Rename");
        GtkWidget *i_del = gtk_menu_item_new_with_label("Move to Trash");
        
        g_signal_connect(i_open, "activate", G_CALLBACK(on_item_open), uri);
        g_signal_connect(i_cut, "activate", G_CALLBACK(on_item_cut), NULL);
        g_signal_connect(i_copy, "activate", G_CALLBACK(on_item_copy), NULL);
        g_signal_connect(i_rename, "activate", G_CALLBACK(on_item_rename), uri);
        g_signal_connect(i_del, "activate", G_CALLBACK(on_item_delete), NULL);
        
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
    
    /* Double Click */
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
        open_file_uri(uri);
        return TRUE;
    }

    return FALSE;
}

/* Desktop Background Context Menu */

static void on_bg_paste(GtkWidget *item, gpointer data) { paste_from_clipboard(); }

static void on_create_folder(GtkWidget *item, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Create Folder", GTK_WINDOW(main_window),
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

/* --- Icon Loading & Placement --- */

static GtkWidget* create_desktop_item(GFileInfo *info, const char *full_path) {
    const char *filename = g_file_info_get_name(info);
    char *display_name = g_strdup(g_file_info_get_display_name(info));
    GIcon *gicon = g_object_ref(g_file_info_get_icon(info));
    gboolean is_dir = (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY);
    
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
    
    /* Store URI */
    g_object_set_data_full(G_OBJECT(btn), "uri", g_strdup(uri), g_free);
    
    /* Setup Drag Source (System DnD) */
    GtkTargetEntry targets[] = { { "text/uri-list", 0, 0 } };
    gtk_drag_source_set(btn, GDK_BUTTON1_MASK, targets, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    g_signal_connect(btn, "drag-begin", G_CALLBACK(on_drag_begin), NULL);
    g_signal_connect(btn, "drag-data-get", G_CALLBACK(on_drag_data_get), NULL);
    
    /* Setup Drag Dest (if directory) */
    if (is_dir) {
        gtk_drag_dest_set(btn, GTK_DEST_DEFAULT_ALL, targets, 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);
        g_signal_connect(btn, "drag-data-received", G_CALLBACK(on_folder_drag_data_received), g_strdup(uri));
    }

    /* Connect Events */
    gtk_widget_add_events(btn, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect_data(btn, "button-press-event", G_CALLBACK(on_item_button_press), g_strdup(uri), (GClosureNotify)g_free, 0);
    
    g_object_unref(f);
    g_free(display_name);
    if (gicon) g_object_unref(gicon);
    g_free(uri);

    gtk_widget_show_all(btn);
    return btn;
}

static gint sort_file_info(gconstpointer a, gconstpointer b) {
    GFileInfo *ia = (GFileInfo *)a;
    GFileInfo *ib = (GFileInfo *)b;
    return g_ascii_strcasecmp(g_file_info_get_display_name(ia), g_file_info_get_display_name(ib));
}

static void refresh_icons() {
    /* Clear existing children */
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(icon_layout));
    deselect_all();
    for (iter = children; iter != NULL; iter = g_list_next(iter))
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    g_list_free(children);
    
    const char *home = g_get_home_dir();
    char *desktop_path = g_strdup_printf("%s/Desktop", home);
    GFile *dir = g_file_new_for_path(desktop_path);
    
    GFileEnumerator *enumerator = g_file_enumerate_children(dir,
        "standard::*", G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (enumerator) {
        GList *file_list = NULL;
        GFileInfo *info;
        while ((info = g_file_enumerator_next_file(enumerator, NULL, NULL))) {
            const char *fname = g_file_info_get_name(info);
            if (fname[0] != '.') {
                file_list = g_list_prepend(file_list, info);
            } else {
                g_object_unref(info);
            }
        }
        g_object_unref(enumerator);
        
        /* Sort files alphabetically */
        file_list = g_list_sort(file_list, sort_file_info);
        
        /* Placement Logic:
           1. Place items with saved positions. Mark spots as occupied.
           2. Place remaining items in first available grid spot.
        */
        
        GList *pending_items = NULL;
        GList *occupied_rects = NULL; /* List of GdkRectangle* */
        
        for (GList *l = file_list; l != NULL; l = l->next) {
            info = (GFileInfo *)l->data;
            const char *fname = g_file_info_get_name(info);
            char *full_path = g_strdup_printf("%s/%s", desktop_path, fname);
            GtkWidget *item = create_desktop_item(info, full_path);
            g_free(full_path);
            
            int x, y;
            if (get_item_position(fname, &x, &y)) {
                gtk_layout_put(GTK_LAYOUT(icon_layout), item, x, y);
                GdkRectangle *r = g_new(GdkRectangle, 1);
                r->x = x; r->y = y; r->width = ITEM_WIDTH; r->height = ITEM_HEIGHT;
                occupied_rects = g_list_prepend(occupied_rects, r);
            } else {
                /* Store for pass 2 */
                pending_items = g_list_append(pending_items, item);
            }
        }
        
        /* Pass 2: Place pending items */
        int grid_x = GRID_X;
        int grid_y = GRID_Y;
        
        for (GList *l = pending_items; l != NULL; l = l->next) {
            GtkWidget *item = GTK_WIDGET(l->data);
            
            /* Find next free spot */
            while (TRUE) {
                gboolean collision = FALSE;
                GdkRectangle candidate = { grid_x, grid_y, ITEM_WIDTH, ITEM_HEIGHT };
                
                for (GList *r_node = occupied_rects; r_node != NULL; r_node = r_node->next) {
                    GdkRectangle *occ = (GdkRectangle *)r_node->data;
                    if (gdk_rectangle_intersect(&candidate, occ, NULL)) {
                        collision = TRUE;
                        break;
                    }
                }
                
                if (!collision) {
                    break; /* Found spot */
                }
                
                /* Move to next grid slot */
                grid_y += ITEM_HEIGHT + 10;
                if (grid_y > screen_h - ITEM_HEIGHT) {
                    grid_y = GRID_Y;
                    grid_x += ITEM_WIDTH + 10;
                }
            }
            
            gtk_layout_put(GTK_LAYOUT(icon_layout), item, grid_x, grid_y);
            
            /* Mark as occupied */
            GdkRectangle *r = g_new(GdkRectangle, 1);
            r->x = grid_x; r->y = grid_y; r->width = ITEM_WIDTH; r->height = ITEM_HEIGHT;
            occupied_rects = g_list_prepend(occupied_rects, r);
        }
        
        /* Cleanup */
        g_list_free(pending_items); // Widgets are owned by container now
        g_list_free_full(file_list, g_object_unref);
        g_list_free_full(occupied_rects, g_free);
    }
    g_object_unref(dir);
    g_free(desktop_path);
}

/* --- Drawing & Events --- */

static gboolean on_layout_draw_bg(GtkWidget *widget, cairo_t *cr, gpointer data) {
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    return FALSE; 
}

static gboolean on_layout_draw_fg(GtkWidget *widget, cairo_t *cr, gpointer data) {
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
        deselect_all();
        is_selecting = TRUE;
        start_x = event->x;
        start_y = event->y;
        current_x = event->x;
        current_y = event->y;
        gtk_widget_queue_draw(icon_layout);
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
        gtk_widget_queue_draw(icon_layout);
        
        double x = MIN(start_x, current_x);
        double y = MIN(start_y, current_y);
        double w = fabs(current_x - start_x);
        double h = fabs(current_y - start_y);
        
        GList *children = gtk_container_get_children(GTK_CONTAINER(icon_layout));
        for (GList *l = children; l != NULL; l = l->next) {
            GtkWidget *item = GTK_WIDGET(l->data);
            GtkAllocation alloc;
            gtk_widget_get_allocation(item, &alloc);
            
            if (alloc.x < x + w && alloc.x + alloc.width > x &&
                alloc.y < y + h && alloc.y + alloc.height > y) {
                select_item(item);
            } else {
                deselect_item(item);
            }
        }
        g_list_free(children);
    }
    return FALSE;
}

static gboolean on_bg_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1 && is_selecting) {
        is_selecting = FALSE;
        gtk_widget_queue_draw(icon_layout);
    }
    return FALSE;
}

/* --- Main --- */

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

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
    
    /* Setup Background as Drag Dest */
    GtkTargetEntry targets[] = { { "text/uri-list", 0, 0 } };
    gtk_drag_dest_set(icon_layout, GTK_DEST_DEFAULT_ALL, targets, 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);
    g_signal_connect(icon_layout, "drag-data-received", G_CALLBACK(on_bg_drag_data_received), NULL);
    
    gtk_widget_add_events(icon_layout, 
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    
    g_signal_connect(icon_layout, "draw", G_CALLBACK(on_layout_draw_bg), NULL);
    g_signal_connect_after(icon_layout, "draw", G_CALLBACK(on_layout_draw_fg), NULL);
    
    g_signal_connect(icon_layout, "button-press-event", G_CALLBACK(on_bg_button_press), NULL);
    g_signal_connect(icon_layout, "motion-notify-event", G_CALLBACK(on_bg_motion), NULL);
    g_signal_connect(icon_layout, "button-release-event", G_CALLBACK(on_bg_button_release), NULL);

    gtk_container_add(GTK_CONTAINER(main_window), icon_layout);

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, 
        "window { background-color: transparent; }"
        "#desktop-item { background: transparent; border-radius: 5px; padding: 8px; transition: all 0.1s; }"
        "#desktop-item:hover { background: rgba(255,255,255,0.15); }"
        "#desktop-item.selected { background: rgba(52, 152, 219, 0.4); border: 1px solid rgba(52, 152, 219, 0.8); }"
        "label { color: white; text-shadow: 1px 1px 2px black; font-weight: bold; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css), 800);

    refresh_icons();

    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(main_window);
    malloc_trim(0);
    gtk_main();

    return 0;
}