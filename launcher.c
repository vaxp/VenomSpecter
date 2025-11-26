#include "launcher.h"
#include "search.h"
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Global variables */
GtkWidget *launcher_button = NULL;
GtkWidget *launcher_window = NULL;
GtkWidget *app_stack = NULL;
GtkWidget *search_entry = NULL;
GtkWidget *search_results_view = NULL;
GtkWidget *prev_button = NULL;
GtkWidget *next_button = NULL;
int total_pages = 0;

/* Function prototypes */
void on_launcher_clicked(GtkWidget *widget, gpointer data);
void on_launcher_app_clicked(GtkWidget *widget, gpointer data);
void populate_applications_grid(GtkWidget *stack);
gboolean on_launcher_window_button_press(GtkWidget *window, GdkEventButton *event, gpointer data);
gboolean on_launcher_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);
gboolean on_launcher_key_press(GtkWidget *window, GdkEventKey *event, gpointer data);
void on_search_activate(GtkEntry *entry, gpointer data);
void update_navigation_buttons(void);

/* Helper to update button visibility */
void update_navigation_buttons(void) {
    if (!app_stack || !prev_button || !next_button) return;
    
    GtkWidget *visible_child = gtk_stack_get_visible_child(GTK_STACK(app_stack));
    if (!visible_child) return;
    
    const gchar *name = gtk_widget_get_name(visible_child);
    
    /* Hide buttons if showing search results */
    if (g_strcmp0(name, "search_results_scroll") == 0) {
        gtk_widget_hide(prev_button);
        gtk_widget_hide(next_button);
        return;
    }
    
    /* Parse page number */
    /* Page names are "page0", "page1", etc. (set in populate_applications_grid) */
    /* Wait, in populate_applications_grid I used gtk_stack_add_named(..., page_name) */
    /* But gtk_widget_get_name might return the type name if not explicitly set? */
    /* Actually gtk_stack_add_named sets the child property 'name', not the widget name. */
    /* To get the name used in stack, we need gtk_stack_get_visible_child_name */
    
    const gchar *child_name = gtk_stack_get_visible_child_name(GTK_STACK(app_stack));
    if (child_name && g_str_has_prefix(child_name, "page")) {
        int page_num = atoi(child_name + 4);
        
        if (page_num > 0) {
            gtk_widget_show(prev_button);
        } else {
            gtk_widget_hide(prev_button);
        }
        
        if (page_num < total_pages - 1) {
            gtk_widget_show(next_button);
        } else {
            gtk_widget_hide(next_button);
        }
    } else {
        /* Fallback or unknown state */
        gtk_widget_hide(prev_button);
        gtk_widget_hide(next_button);
    }
}

/* Stack visible child changed callback */
void on_stack_visible_child_notify(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    (void)gobject; (void)pspec; (void)user_data;
    update_navigation_buttons();
}

/* Search Entry Activate Handler (Enter Key) */
void on_search_activate(GtkEntry *entry, gpointer data) {
    (void)data;
    const gchar *text = gtk_entry_get_text(entry);
    if (!text || strlen(text) == 0) return;
    
    if (g_str_has_prefix(text, "vater:")) {
        execute_vater(text + 6, launcher_window);
        return;
    }
    if (g_str_has_prefix(text, "!:")) {
        execute_math(text + 2, launcher_window);
        return;
    }
    if (g_str_has_prefix(text, "vafile:")) {
        execute_file_search(text + 7, launcher_window);
        return;
    }
    if (g_str_has_prefix(text, "g:")) {
        execute_web_search(text + 2, "github", launcher_window, &app_stack, &search_entry, &search_results_view);
        return;
    }
    if (g_str_has_prefix(text, "s:")) {
        execute_web_search(text + 2, "google", launcher_window, &app_stack, &search_entry, &search_results_view);
        return;
    }
    
    /* If normal search, launch first app result */
    GList *children = gtk_container_get_children(GTK_CONTAINER(search_results_view));
    if (children) {
        GtkWidget *first_btn = GTK_WIDGET(children->data);
        gtk_widget_activate(first_btn);
        g_list_free(children);
    }
}

void on_search_changed(GtkSearchEntry *entry, gpointer data) {
    (void)data;
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
    perform_search(text, app_stack, search_results_view, launcher_window);
}

/* Launcher app clicked - launch application */
void on_launcher_app_clicked(GtkWidget *widget, gpointer data) {
    const gchar *type = (const gchar *)data;
    
    if (g_strcmp0(type, "app") == 0) {
        const gchar *desktop_file = g_object_get_data(G_OBJECT(widget), "desktop-file");
        if (desktop_file != NULL) {
            GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(desktop_file);
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
            
            /* Close launcher */
            if (launcher_window != NULL) {
                gtk_widget_destroy(launcher_window);
                launcher_window = NULL;
                app_stack = NULL;
                search_entry = NULL;
                search_results_view = NULL;
                prev_button = NULL;
                next_button = NULL;
            }
        }
    }
}

/* Navigation callbacks */
void on_prev_page_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    GtkStack *stack = GTK_STACK(data);
    GtkWidget *visible = gtk_stack_get_visible_child(stack);
    if (visible) {
        if (g_strcmp0(gtk_widget_get_name(visible), "search_results_scroll") == 0) return;
        GList *children = gtk_container_get_children(GTK_CONTAINER(stack));
        GList *l = g_list_find(children, visible);
        if (l && l->prev) gtk_stack_set_visible_child(stack, GTK_WIDGET(l->prev->data));
        g_list_free(children);
    }
}

void on_next_page_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    GtkStack *stack = GTK_STACK(data);
    GtkWidget *visible = gtk_stack_get_visible_child(stack);
    if (visible) {
        if (g_strcmp0(gtk_widget_get_name(visible), "search_results_scroll") == 0) return;
        GList *children = gtk_container_get_children(GTK_CONTAINER(stack));
        GList *l = g_list_find(children, visible);
        if (l && l->next) gtk_stack_set_visible_child(stack, GTK_WIDGET(l->next->data));
        g_list_free(children);
    }
}

/* Launcher button clicked - show applications grid */
void on_launcher_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    if (launcher_window != NULL) {
        gtk_widget_destroy(launcher_window);
        launcher_window = NULL;
        app_stack = NULL;
        search_entry = NULL;
        search_results_view = NULL;
        prev_button = NULL;
        next_button = NULL;
        return;
    }
    
    launcher_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(launcher_window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(launcher_window), GDK_WINDOW_TYPE_HINT_NORMAL);
    gtk_widget_set_name(launcher_window, "launcher-window");
    gtk_widget_set_app_paintable(launcher_window, TRUE);
    GdkScreen *l_screen = gtk_window_get_screen(GTK_WINDOW(launcher_window));
    GdkVisual *l_visual = gdk_screen_get_rgba_visual(l_screen);
    if (l_visual != NULL && gdk_screen_is_composited(l_screen)) {
        gtk_widget_set_visual(launcher_window, l_visual);
    }
    GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(launcher_window));
    gint screen_width = gdk_screen_get_width(screen);
    gint screen_height = gdk_screen_get_height(screen);
    gtk_window_set_default_size(GTK_WINDOW(launcher_window), screen_width, screen_height);
    gtk_window_move(GTK_WINDOW(launcher_window), 0, 0);
    
    GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(root_box), 40);
    gtk_container_add(GTK_CONTAINER(launcher_window), root_box);
    
    search_entry = gtk_search_entry_new();
    gtk_widget_set_size_request(search_entry, 400, -1);
    gtk_widget_set_halign(search_entry, GTK_ALIGN_CENTER);
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed), NULL);
    g_signal_connect(search_entry, "activate", G_CALLBACK(on_search_activate), NULL);
    gtk_box_pack_start(GTK_BOX(root_box), search_entry, FALSE, FALSE, 0);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(root_box), main_box, TRUE, TRUE, 0);
    
    app_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app_stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(app_stack), 300);
    g_signal_connect(app_stack, "notify::visible-child", G_CALLBACK(on_stack_visible_child_notify), NULL);
    
    GtkWidget *results_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(results_scroll, "search_results_scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(results_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    search_results_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(search_results_view, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(results_scroll), search_results_view);
    
    gtk_stack_add_named(GTK_STACK(app_stack), results_scroll, "search_results");
    
    populate_applications_grid(app_stack);
    
    prev_button = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_name(prev_button, "nav-button");
    gtk_widget_set_valign(prev_button, GTK_ALIGN_CENTER);
    g_signal_connect(prev_button, "clicked", G_CALLBACK(on_prev_page_clicked), app_stack);
    gtk_box_pack_start(GTK_BOX(main_box), prev_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), app_stack, TRUE, TRUE, 0);
    
    next_button = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_name(next_button, "nav-button");
    gtk_widget_set_valign(next_button, GTK_ALIGN_CENTER);
    g_signal_connect(next_button, "clicked", G_CALLBACK(on_next_page_clicked), app_stack);
    gtk_box_pack_start(GTK_BOX(main_box), next_button, FALSE, FALSE, 0);
    
    gtk_window_set_modal(GTK_WINDOW(launcher_window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(launcher_window), TRUE);
    
    g_signal_connect(launcher_window, "key-press-event", G_CALLBACK(on_launcher_key_press), NULL);
    g_signal_connect(launcher_window, "delete-event", G_CALLBACK(on_launcher_delete_event), NULL);
    
    gtk_widget_show_all(launcher_window);
    
    /* Initial button state */
    update_navigation_buttons();
    
    GtkWidget *first_page = gtk_stack_get_child_by_name(GTK_STACK(app_stack), "page0");
    if (first_page) gtk_stack_set_visible_child(GTK_STACK(app_stack), first_page);
    
    gtk_window_present(GTK_WINDOW(launcher_window));
    
    /* Force focus on search entry */
    gtk_widget_grab_focus(search_entry);
    gtk_window_set_focus(GTK_WINDOW(launcher_window), search_entry);
}

void create_launcher_button(GtkWidget *box) {
    launcher_button = gtk_button_new_from_icon_name("start-here-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_name(launcher_button, "launcher-button");
    g_signal_connect(launcher_button, "clicked", G_CALLBACK(on_launcher_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box), launcher_button, FALSE, FALSE, 0);
    gtk_widget_show(launcher_button);
}

gboolean on_launcher_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    if (launcher_window) {
        gtk_widget_destroy(launcher_window);
        launcher_window = NULL;
        app_stack = NULL;
        search_entry = NULL;
        search_results_view = NULL;
        prev_button = NULL;
        next_button = NULL;
    }
    return TRUE; /* Stop propagation */
}

gboolean on_launcher_key_press(GtkWidget *window, GdkEventKey *event, gpointer data) {
    (void)window; (void)data;
    if (event->keyval == GDK_KEY_Escape) {
        if (launcher_window) {
            gtk_widget_destroy(launcher_window);
            launcher_window = NULL;
            app_stack = NULL;
            search_entry = NULL;
            search_results_view = NULL;
            prev_button = NULL;
            next_button = NULL;
        }
        return TRUE;
    }
    return FALSE;
}

void populate_applications_grid(GtkWidget *stack) {
    GDir *dir;
    const gchar *filename;
    GError *error = NULL;
    GList *apps = NULL;
    
    dir = g_dir_open("/usr/share/applications", 0, &error);
    if (dir) {
        while ((filename = g_dir_read_name(dir)) != NULL) {
            if (!g_str_has_suffix(filename, ".desktop")) continue;
            gchar *path = g_build_filename("/usr/share/applications", filename, NULL);
            apps = g_list_prepend(apps, path);
        }
        g_dir_close(dir);
    }
    
    int app_count = 0;
    int page_count = 0;
    GtkWidget *current_grid = NULL;
    
    for (GList *l = apps; l != NULL; l = l->next) {
        gchar *path = (gchar *)l->data;
        GKeyFile *keyfile = g_key_file_new();
        
        if (g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, NULL)) {
            gboolean no_display = g_key_file_get_boolean(keyfile, "Desktop Entry", "NoDisplay", NULL);
            gboolean hidden = g_key_file_get_boolean(keyfile, "Desktop Entry", "Hidden", NULL);
            
            if (!no_display && !hidden) {
                if (app_count % 24 == 0) {
                    current_grid = gtk_grid_new();
                    gtk_grid_set_row_spacing(GTK_GRID(current_grid), 20);
                    gtk_grid_set_column_spacing(GTK_GRID(current_grid), 20);
                    gtk_widget_set_halign(current_grid, GTK_ALIGN_CENTER);
                    gtk_widget_set_valign(current_grid, GTK_ALIGN_CENTER);
                    
                    gchar *page_name = g_strdup_printf("page%d", page_count++);
                    gtk_stack_add_named(GTK_STACK(stack), current_grid, page_name);
                    g_free(page_name);
                }
                
                gchar *name = g_key_file_get_locale_string(keyfile, "Desktop Entry", "Name", NULL, NULL);
                gchar *icon = g_key_file_get_string(keyfile, "Desktop Entry", "Icon", NULL);
                
                GtkWidget *app_btn = gtk_button_new();
                gtk_widget_set_size_request(app_btn, 120, 120);
                GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
                
                GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
                GdkPixbuf *pixbuf = NULL;
                if (icon) {
                    if (g_path_is_absolute(icon)) {
                        pixbuf = gdk_pixbuf_new_from_file_at_scale(icon, 48, 48, TRUE, NULL);
                    } else {
                        pixbuf = gtk_icon_theme_load_icon(icon_theme, icon, 48, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
                    }
                }
                
                GtkWidget *image;
                if (pixbuf) {
                    image = gtk_image_new_from_pixbuf(pixbuf);
                    g_object_unref(pixbuf);
                } else {
                    image = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DND);
                }
                gtk_box_pack_start(GTK_BOX(box), image, TRUE, TRUE, 0);
                
                GtkWidget *label = gtk_label_new(name);
                gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
                gtk_label_set_max_width_chars(GTK_LABEL(label), 10);
                gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
                
                gtk_container_add(GTK_CONTAINER(app_btn), box);
                
                g_object_set_data_full(G_OBJECT(app_btn), "desktop-file", g_strdup(path), g_free);
                g_signal_connect(app_btn, "clicked", G_CALLBACK(on_launcher_app_clicked), (gpointer)"app");
                
                int pos_in_page = app_count % 24;
                gtk_grid_attach(GTK_GRID(current_grid), app_btn, pos_in_page % 6, pos_in_page / 6, 1, 1);
                
                g_free(name);
                g_free(icon);
                app_count++;
            }
        }
        g_key_file_free(keyfile);
    }
    g_list_free_full(apps, g_free);
    
    total_pages = page_count;
}
