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
    WindowGroup *group = (WindowGroup *)data;
    
    if (group->is_pinned) {
        /* Unpin */
        group->is_pinned = FALSE;
        pinned_apps = g_list_remove(pinned_apps, g_list_find_custom(pinned_apps, group->wm_class, (GCompareFunc)g_strcmp0)->data);
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
    WindowGroup *group = (WindowGroup *)data;
    
    if (group->desktop_file_path != NULL) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(group->desktop_file_path);
        if (app_info != NULL) {
            const gchar *exec = g_app_info_get_commandline(G_APP_INFO(app_info));
            
            /* Build command with GPU environment variables */
            gchar *gpu_command = g_strdup_printf("env DRI_PRIME=1 __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia %s", exec);
            
            GError *error = NULL;
            if (!g_spawn_command_line_async(gpu_command, &error)) {
                g_warning("Failed to launch with GPU: %s", error->message);
                g_error_free(error);
            }
            
            g_free(gpu_command);
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
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) { /* Right click */
        WindowGroup *group = (WindowGroup *)g_object_get_data(G_OBJECT(widget), "group");
        if (group != NULL) {
            create_context_menu(group, event);
            return TRUE;
        }
    }
    return FALSE;
}
