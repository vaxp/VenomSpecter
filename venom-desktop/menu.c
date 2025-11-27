/*
 * menu.c
 * Context menus for items and background
 */

#include "menu.h"
#include "wallpaper.h"
#include "selection.h"
#include "filesystem.h"
#include "icons.h"
#include <gio/gdesktopappinfo.h>
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

/* --- Item Menu Callbacks --- */

void on_item_rename(GtkWidget *menuitem, gpointer data) {
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

void on_item_cut(GtkWidget *menuitem, gpointer data) { 
    copy_selection_to_clipboard(TRUE); 
}

void on_item_copy(GtkWidget *menuitem, gpointer data) { 
    copy_selection_to_clipboard(FALSE); 
}

void on_item_open(GtkWidget *menuitem, gpointer data) { 
    open_file_uri((char*)data); 
}

void on_item_delete(GtkWidget *menuitem, gpointer data) { 
    GList *uris_to_delete = NULL;
    
    /* Collect URIs first to avoid modifying the list while iterating */
    for (GList *l = selected_items; l != NULL; l = l->next) {
        char *uri = (char *)g_object_get_data(G_OBJECT(l->data), "uri");
        if (uri) {
            uris_to_delete = g_list_prepend(uris_to_delete, g_strdup(uri));
        }
    }
    
    /* Perform deletion */
    for (GList *l = uris_to_delete; l != NULL; l = l->next) {
        delete_file((char *)l->data);
        g_free(l->data);
    }
    g_list_free(uris_to_delete);
    
    refresh_icons();
}

gboolean on_item_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
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
        
        if (g_list_length(selected_items) > 1) {
            /* Multi-Selection Menu */
            GtkWidget *i_cut = gtk_menu_item_new_with_label("Cut");
            GtkWidget *i_copy = gtk_menu_item_new_with_label("Copy");
            GtkWidget *i_del = gtk_menu_item_new_with_label("Move to Trash");
            
            g_signal_connect(i_cut, "activate", G_CALLBACK(on_item_cut), NULL);
            g_signal_connect(i_copy, "activate", G_CALLBACK(on_item_copy), NULL);
            g_signal_connect(i_del, "activate", G_CALLBACK(on_item_delete), NULL);
            
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_cut);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_copy);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_del);
        } else {
            /* Single Item Menu */
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
        }
        
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

/* --- Background Menu Callbacks --- */

void on_bg_paste(GtkWidget *item, gpointer data) { 
    paste_from_clipboard(); 
}

void on_create_folder(GtkWidget *item, gpointer data) {
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

void on_open_terminal(GtkWidget *item, gpointer data) {
    char *cmd = g_strdup_printf("exo-open --launch TerminalEmulator --working-directory=%s/Desktop", g_get_home_dir());
    g_spawn_command_line_async(cmd, NULL);
    g_free(cmd);
}

void on_refresh_clicked(GtkWidget *item, gpointer data) {
    refresh_icons();
}
