#include "launcher.h"
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

/* Function prototypes */
void on_launcher_clicked(GtkWidget *widget, gpointer data);
void on_launcher_app_clicked(GtkWidget *widget, gpointer data);
void populate_applications_grid(GtkWidget *stack);
void perform_search(const gchar *text);
gboolean on_launcher_window_button_press(GtkWidget *window, GdkEventButton *event, gpointer data);
gboolean on_launcher_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);
gboolean on_launcher_key_press(GtkWidget *window, GdkEventKey *event, gpointer data);
void on_search_activate(GtkEntry *entry, gpointer data);

/* --- Dialog Implementations --- */

/* Password Dialog */
gchar *show_password_dialog(GtkWidget *parent) {
    GtkWidget *dialog, *content_area;
    GtkWidget *entry;
    gchar *password = NULL;
    gint result;

    dialog = gtk_dialog_new_with_buttons("Authentication Required",
                                         GTK_WINDOW(parent),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_OK", GTK_RESPONSE_ACCEPT,
                                         NULL);
    
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 20);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content_area), 10);

    GtkWidget *icon = gtk_image_new_from_icon_name("dialog-password", GTK_ICON_SIZE_DIALOG);
    gtk_container_add(GTK_CONTAINER(content_area), icon);

    GtkWidget *label = gtk_label_new("Enter your password to run this command:");
    gtk_container_add(GTK_CONTAINER(content_area), label);

    entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    gtk_widget_show_all(content_area);
    
    /* Set default button */
    GtkWidget *ok_btn = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_widget_grab_default(ok_btn);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        password = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    }

    gtk_widget_destroy(dialog);
    return password;
}

/* Math Result Dialog */
void show_math_result_dialog(GtkWidget *parent, const gchar *expression, const gchar *result) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_CLOSE,
                                               "%s\n= %s", expression, result);
    gtk_window_set_title(GTK_WINDOW(dialog), "Calculation Result");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/* File Results Dialog */
void on_file_result_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    (void)data;
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(row));
    const gchar *path = g_object_get_data(G_OBJECT(child), "file-path");
    
    if (path) {
        gchar *uri = g_filename_to_uri(path, NULL, NULL);
        if (uri) {
            gtk_show_uri_on_window(GTK_WINDOW(launcher_window), uri, GDK_CURRENT_TIME, NULL);
            g_free(uri);
            /* Close launcher */
            if (launcher_window) {
                gtk_widget_destroy(launcher_window);
                launcher_window = NULL;
                app_stack = NULL;
                search_entry = NULL;
                search_results_view = NULL;
            }
        }
    }
}

void show_file_results_dialog(GtkWidget *parent, const gchar *term, GList *files) {
    (void)term;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("File Search Results",
                                                    GTK_WINDOW(parent),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_Close", GTK_RESPONSE_CLOSE,
                                                    NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    if (files == NULL) {
        GtkWidget *label = gtk_label_new("No files found.");
        gtk_container_add(GTK_CONTAINER(content_area), label);
    } else {
        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_add(GTK_CONTAINER(content_area), scrolled);
        
        GtkWidget *listbox = gtk_list_box_new();
        gtk_container_add(GTK_CONTAINER(scrolled), listbox);
        g_signal_connect(listbox, "row-activated", G_CALLBACK(on_file_result_activated), NULL);
        
        for (GList *l = files; l != NULL; l = l->next) {
            gchar *path = (gchar *)l->data;
            gchar *basename = g_path_get_basename(path);
            
            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            g_object_set_data_full(G_OBJECT(row_box), "file-path", g_strdup(path), g_free);
            
            GtkWidget *name_label = gtk_label_new(basename);
            gtk_widget_set_halign(name_label, GTK_ALIGN_START);
            gchar *markup = g_strdup_printf("<b>%s</b>", basename);
            gtk_label_set_markup(GTK_LABEL(name_label), markup);
            g_free(markup);
            
            GtkWidget *path_label = gtk_label_new(path);
            gtk_widget_set_halign(path_label, GTK_ALIGN_START);
            gtk_label_set_ellipsize(GTK_LABEL(path_label), PANGO_ELLIPSIZE_START);
            GtkStyleContext *context = gtk_widget_get_style_context(path_label);
            gtk_style_context_add_class(context, "dim-label");
            
            gtk_box_pack_start(GTK_BOX(row_box), name_label, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(row_box), path_label, FALSE, FALSE, 0);
            
            gtk_container_set_border_width(GTK_CONTAINER(row_box), 10);
            gtk_container_add(GTK_CONTAINER(listbox), row_box);
            
            g_free(basename);
        }
    }
    
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/* Command Console Dialog */
typedef struct {
    GtkWidget *dialog;
    GtkWidget *text_view;
    GtkWidget *entry;
    GPid pid;
    gint stdin_fd;
    guint stdout_watch;
    guint stderr_watch;
} ConsoleData;

static gboolean on_console_output(GIOChannel *source, GIOCondition condition, gpointer data) {
    ConsoleData *console = (ConsoleData *)data;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->text_view));
    GtkTextIter end;
    gchar *str;
    gsize len;
    GIOStatus status;

    if (condition & G_IO_IN) {
        status = g_io_channel_read_line(source, &str, &len, NULL, NULL);
        if (status == G_IO_STATUS_NORMAL) {
            gtk_text_buffer_get_end_iter(buffer, &end);
            gtk_text_buffer_insert(buffer, &end, str, len);
            
            /* Scroll to bottom */
            GtkTextMark *mark = gtk_text_buffer_create_mark(buffer, NULL, &end, FALSE);
            gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(console->text_view), mark, 0.0, TRUE, 0.0, 1.0);
            gtk_text_buffer_delete_mark(buffer, mark);
            
            g_free(str);
        }
    }
    
    if (condition & (G_IO_HUP | G_IO_ERR)) {
        /* Mark as removed so we don't try to remove again in close */
        if (console->stdout_watch > 0 && g_source_remove_by_user_data(console)) {
             /* This logic is tricky because we don't know WHICH watch this is (stdout or stderr) easily without checking.
                Actually, simpler: just return FALSE and let GLib remove it. 
                BUT we must update our struct so on_console_close doesn't try to remove it.
             */
        }
        return FALSE; /* Remove source */
    }
    
    return TRUE;
}

static gboolean on_console_stdout(GIOChannel *source, GIOCondition condition, gpointer data) {
    ConsoleData *console = (ConsoleData *)data;
    gboolean result = on_console_output(source, condition, data);
    if (result == FALSE) console->stdout_watch = 0;
    return result;
}

static gboolean on_console_stderr(GIOChannel *source, GIOCondition condition, gpointer data) {
    ConsoleData *console = (ConsoleData *)data;
    gboolean result = on_console_output(source, condition, data);
    if (result == FALSE) console->stderr_watch = 0;
    return result;
}

static void on_console_entry_activate(GtkEntry *entry, gpointer data) {
    ConsoleData *console = (ConsoleData *)data;
    const gchar *text = gtk_entry_get_text(entry);
    
    if (console->stdin_fd != -1) {
        gchar *input = g_strdup_printf("%s\n", text);
        write(console->stdin_fd, input, strlen(input));
        g_free(input);
        gtk_entry_set_text(entry, "");
    }
}

static void on_console_close(GtkWidget *dialog, gint response, gpointer data) {
    (void)dialog; (void)response;
    ConsoleData *console = (ConsoleData *)data;
    
    if (console->stdout_watch > 0) {
        g_source_remove(console->stdout_watch);
        console->stdout_watch = 0;
    }
    if (console->stderr_watch > 0) {
        g_source_remove(console->stderr_watch);
        console->stderr_watch = 0;
    }
    
    g_free(console);
}

void show_command_console(GtkWidget *parent, const gchar *command, const gchar *password) {
    ConsoleData *console = g_new0(ConsoleData, 1);
    gint stdout_pipe, stderr_pipe;
    GError *error = NULL;
    gchar **argv;
    
    if (!g_shell_parse_argv(command, NULL, &argv, &error)) {
        g_warning("Parse error: %s", error->message);
        g_error_free(error);
        g_free(console);
        return;
    }
    
    /* Handle sudo -S injection if password provided */
    if (password != NULL) {
        /* This is a simplified injection. In a real app, we'd reconstruct the argv properly */
        /* For now, we assume the command starts with sudo */
        /* Actually, let's just run: echo password | sudo -S command */
        /* But that's hard with execvp. */
        /* Better: spawn 'sudo -S ...' and write password to stdin immediately */
        
        /* Check if command contains sudo */
        gboolean has_sudo = FALSE;
        for (int i = 0; argv[i] != NULL; i++) {
            if (strcmp(argv[i], "sudo") == 0) {
                has_sudo = TRUE;
                /* Inject -S if not present? Too complex for this snippet. 
                   Let's assume user or caller handles 'sudo -S' or we just write to stdin anyway. */
            }
        }
        
        if (has_sudo) {
             /* We will write password to stdin below */
        }
    }

    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &console->pid, &console->stdin_fd, &stdout_pipe, &stderr_pipe, &error)) {
        GtkWidget *err_dlg = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Failed to spawn command: %s", error->message);
        gtk_dialog_run(GTK_DIALOG(err_dlg));
        gtk_widget_destroy(err_dlg);
        g_error_free(error);
        g_strfreev(argv);
        g_free(console);
        return;
    }
    g_strfreev(argv);

    /* Create UI */
    console->dialog = gtk_dialog_new_with_buttons("Command Console", GTK_WINDOW(parent), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "_Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(console->dialog), 700, 500);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(console->dialog));
    
    /* Command label */
    GtkWidget *cmd_label = gtk_label_new(command);
    gtk_widget_set_halign(cmd_label, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(content), cmd_label);
    
    /* Text View */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    console->text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(console->text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(console->text_view), TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled), console->text_view);
    gtk_box_pack_start(GTK_BOX(content), scrolled, TRUE, TRUE, 0);
    
    /* Input Entry */
    console->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(console->entry), "Type input...");
    g_signal_connect(console->entry, "activate", G_CALLBACK(on_console_entry_activate), console);
    gtk_box_pack_start(GTK_BOX(content), console->entry, FALSE, FALSE, 0);
    
    gtk_widget_show_all(console->dialog);
    
    /* Setup IO watches */
    GIOChannel *out_ch = g_io_channel_unix_new(stdout_pipe);
    console->stdout_watch = g_io_add_watch(out_ch, G_IO_IN | G_IO_HUP | G_IO_ERR, on_console_stdout, console);
    g_io_channel_unref(out_ch);
    
    GIOChannel *err_ch = g_io_channel_unix_new(stderr_pipe);
    console->stderr_watch = g_io_add_watch(err_ch, G_IO_IN | G_IO_HUP | G_IO_ERR, on_console_stderr, console);
    g_io_channel_unref(err_ch);
    
    /* Write password if needed */
    if (password) {
        gchar *pwd_nl = g_strdup_printf("%s\n", password);
        write(console->stdin_fd, pwd_nl, strlen(pwd_nl));
        g_free(pwd_nl);
    }
    
    g_signal_connect(console->dialog, "response", G_CALLBACK(on_console_close), console);
    gtk_dialog_run(GTK_DIALOG(console->dialog));
    gtk_widget_destroy(console->dialog);
}

/* --- Search Logic Refactoring --- */

/* Helper to execute shell command and return output (Synchronous - for simple things) */
gchar *execute_command_sync(const gchar *cmd) {
    FILE *fp;
    char path[1035];
    GString *output = g_string_new("");
    fp = popen(cmd, "r");
    if (fp == NULL) {
        g_string_free(output, TRUE);
        return g_strdup("Failed to run command");
    }
    while (fgets(path, sizeof(path), fp) != NULL) {
        g_string_append(output, path);
    }
    pclose(fp);
    return g_string_free(output, FALSE);
}

/* Execution Handlers */

void execute_vater(const gchar *cmd) {
    if (cmd && strlen(cmd) > 0) {
        gchar *password = NULL;
        /* Check if sudo is needed */
        if (strstr(cmd, "sudo")) {
            password = show_password_dialog(launcher_window);
            if (!password) return; /* Cancelled */
            
            /* Inject -S for sudo if not present */
            gchar *final_cmd;
            if (!strstr(cmd, "-S")) {
                 GRegex *regex = g_regex_new("\\bsudo\\b", 0, 0, NULL);
                 final_cmd = g_regex_replace_literal(regex, cmd, -1, 0, "sudo -S", 0, NULL);
                 g_regex_unref(regex);
            } else {
                final_cmd = g_strdup(cmd);
            }
            
            show_command_console(launcher_window, final_cmd, password);
            g_free(final_cmd);
            g_free(password);
        } else {
            show_command_console(launcher_window, cmd, NULL);
        }
    }
}

void execute_math(const gchar *expr) {
    if (expr && strlen(expr) > 0) {
        gchar *cmd = g_strdup_printf("echo \"%s\" | bc -l", expr);
        gchar *output = execute_command_sync(cmd);
        g_strstrip(output);
        show_math_result_dialog(launcher_window, expr, output);
        g_free(output);
        g_free(cmd);
    }
}

void execute_file_search(const gchar *term) {
    if (term && strlen(term) > 0) {
        gchar *cmd = g_strdup_printf("find ~ -name \"*%s*\" 2>/dev/null | head -n 20", term);
        FILE *fp = popen(cmd, "r");
        GList *files = NULL;
        if (fp) {
            char path[1035];
            while (fgets(path, sizeof(path), fp) != NULL) {
                path[strcspn(path, "\n")] = 0;
                if (strlen(path) > 0) files = g_list_append(files, g_strdup(path));
            }
            pclose(fp);
        }
        g_free(cmd);
        show_file_results_dialog(launcher_window, term, files);
        g_list_free_full(files, g_free);
    }
}

void execute_web_search(const gchar *term, const gchar *engine) {
    if (term && strlen(term) > 0) {
        gchar *url;
        if (g_strcmp0(engine, "github") == 0) {
            url = g_strdup_printf("https://github.com/search?q=%s", term);
        } else {
            url = g_strdup_printf("https://www.google.com/search?q=%s", term);
        }
        gtk_show_uri_on_window(GTK_WINDOW(launcher_window), url, GDK_CURRENT_TIME, NULL);
        g_free(url);
        
        /* Close launcher */
        if (launcher_window) {
            gtk_widget_destroy(launcher_window);
            launcher_window = NULL;
            app_stack = NULL;
            search_entry = NULL;
            search_results_view = NULL;
        }
    }
}

/* Search Entry Activate Handler (Enter Key) */
void on_search_activate(GtkEntry *entry, gpointer data) {
    (void)data;
    const gchar *text = gtk_entry_get_text(entry);
    if (!text || strlen(text) == 0) return;
    
    if (g_str_has_prefix(text, "vater:")) {
        execute_vater(text + 6);
        return;
    }
    if (g_str_has_prefix(text, "!:")) {
        execute_math(text + 2);
        return;
    }
    if (g_str_has_prefix(text, "vafile:")) {
        execute_file_search(text + 7);
        return;
    }
    if (g_str_has_prefix(text, "g:")) {
        execute_web_search(text + 2, "github");
        return;
    }
    if (g_str_has_prefix(text, "s:")) {
        execute_web_search(text + 2, "google");
        return;
    }
    
    /* If normal search, maybe launch the first result? 
       For now, do nothing or launch first app if available.
       Let's try to launch first app in results. */
    GList *children = gtk_container_get_children(GTK_CONTAINER(search_results_view));
    if (children) {
        GtkWidget *first_btn = GTK_WIDGET(children->data);
        /* Check if it's an app button */
        /* const gchar *type = g_object_get_data(G_OBJECT(first_btn), "type"); - Unused */
        /* Actually, we can just simulate a click on the first button */
        gtk_widget_activate(first_btn); // This emits 'clicked'
        g_list_free(children);
    }
}

void perform_search(const gchar *text) {
    if (search_results_view == NULL) return;
    
    /* Clear previous results */
    GList *children = gtk_container_get_children(GTK_CONTAINER(search_results_view));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
    
    if (text == NULL || strlen(text) == 0) {
        GtkWidget *first_page = gtk_stack_get_child_by_name(GTK_STACK(app_stack), "page0");
        if (first_page) gtk_stack_set_visible_child(GTK_STACK(app_stack), first_page);
        return;
    }
    
    /* Switch to search results view */
    GtkWidget *results_scroll = gtk_stack_get_child_by_name(GTK_STACK(app_stack), "search_results");
    if (results_scroll) gtk_stack_set_visible_child(GTK_STACK(app_stack), results_scroll);
    
    /* Special Commands Handling - Show Info Labels */
    
    /* vater: Shell Command */
    if (g_str_has_prefix(text, "vater:")) {
        GtkWidget *label = gtk_label_new("Press Enter to execute command...");
        gtk_box_pack_start(GTK_BOX(search_results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(search_results_view);
        return;
    }
    
    /* !: Math */
    if (g_str_has_prefix(text, "!:")) {
        GtkWidget *label = gtk_label_new("Press Enter to calculate...");
        gtk_box_pack_start(GTK_BOX(search_results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(search_results_view);
        return;
    }
    
    /* vafile: File Search */
    if (g_str_has_prefix(text, "vafile:")) {
        GtkWidget *label = gtk_label_new("Press Enter to search files...");
        gtk_box_pack_start(GTK_BOX(search_results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(search_results_view);
        return;
    }
    
    /* g: GitHub Search */
    if (g_str_has_prefix(text, "g:")) {
        GtkWidget *label = gtk_label_new("Press Enter to search GitHub...");
        gtk_box_pack_start(GTK_BOX(search_results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(search_results_view);
        return;
    }
    
    /* s: Google Search */
    if (g_str_has_prefix(text, "s:")) {
        GtkWidget *label = gtk_label_new("Press Enter to search Google...");
        gtk_box_pack_start(GTK_BOX(search_results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(search_results_view);
        return;
    }

    /* Normal App Search */
    GDir *dir;
    const gchar *filename;
    GError *error = NULL;
    gchar *lower_text = g_utf8_strdown(text, -1);
    
    dir = g_dir_open("/usr/share/applications", 0, &error);
    if (dir) {
        while ((filename = g_dir_read_name(dir)) != NULL) {
            if (!g_str_has_suffix(filename, ".desktop")) continue;
            
            gchar *filepath = g_build_filename("/usr/share/applications", filename, NULL);
            GKeyFile *keyfile = g_key_file_new();
            
            if (g_key_file_load_from_file(keyfile, filepath, G_KEY_FILE_NONE, NULL)) {
                gboolean no_display = g_key_file_get_boolean(keyfile, "Desktop Entry", "NoDisplay", NULL);
                gboolean hidden = g_key_file_get_boolean(keyfile, "Desktop Entry", "Hidden", NULL);
                
                if (!no_display && !hidden) {
                    gchar *name = g_key_file_get_locale_string(keyfile, "Desktop Entry", "Name", NULL, NULL);
                    if (name) {
                        gchar *lower_name = g_utf8_strdown(name, -1);
                        if (strstr(lower_name, lower_text)) {
                            gchar *icon = g_key_file_get_string(keyfile, "Desktop Entry", "Icon", NULL);
                            
                            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                            GtkWidget *button = gtk_button_new();
                            
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
                            gtk_box_pack_start(GTK_BOX(row), image, FALSE, FALSE, 0);
                            
                            GtkWidget *label = gtk_label_new(name);
                            gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0);
                            
                            gtk_container_add(GTK_CONTAINER(button), row);
                            
                            g_object_set_data_full(G_OBJECT(button), "desktop-file", g_strdup(filepath), g_free);
                            g_signal_connect(button, "clicked", G_CALLBACK(on_launcher_app_clicked), (gpointer)"app");
                            
                            gtk_box_pack_start(GTK_BOX(search_results_view), button, FALSE, FALSE, 0);
                            
                            g_free(icon);
                        }
                        g_free(lower_name);
                        g_free(name);
                    }
                }
            }
            g_key_file_free(keyfile);
            g_free(filepath);
        }
        g_dir_close(dir);
    }
    g_free(lower_text);
    
    gtk_widget_show_all(search_results_view);
}

void on_search_changed(GtkSearchEntry *entry, gpointer data) {
    (void)data;
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
    perform_search(text);
}

/* Launcher app clicked - launch application */
void on_launcher_app_clicked(GtkWidget *widget, gpointer data) {
    const gchar *type = (const gchar *)data;
    
    /* Only handle apps here now, special commands are handled by Enter key */
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
    
    GtkWidget *results_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(results_scroll, "search_results_scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(results_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    search_results_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(search_results_view, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(results_scroll), search_results_view);
    
    gtk_stack_add_named(GTK_STACK(app_stack), results_scroll, "search_results");
    
    populate_applications_grid(app_stack);
    
    GtkWidget *prev_button = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_name(prev_button, "nav-button");
    gtk_widget_set_valign(prev_button, GTK_ALIGN_CENTER);
    g_signal_connect(prev_button, "clicked", G_CALLBACK(on_prev_page_clicked), app_stack);
    gtk_box_pack_start(GTK_BOX(main_box), prev_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), app_stack, TRUE, TRUE, 0);
    
    GtkWidget *next_button = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_name(next_button, "nav-button");
    gtk_widget_set_valign(next_button, GTK_ALIGN_CENTER);
    g_signal_connect(next_button, "clicked", G_CALLBACK(on_next_page_clicked), app_stack);
    gtk_box_pack_start(GTK_BOX(main_box), next_button, FALSE, FALSE, 0);
    
    gtk_window_set_modal(GTK_WINDOW(launcher_window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(launcher_window), TRUE);
    
    g_signal_connect(launcher_window, "key-press-event", G_CALLBACK(on_launcher_key_press), NULL);
    g_signal_connect(launcher_window, "delete-event", G_CALLBACK(on_launcher_delete_event), NULL);
    
    gtk_widget_show_all(launcher_window);
    
    GtkWidget *first_page = gtk_stack_get_child_by_name(GTK_STACK(app_stack), "page0");
    if (first_page) gtk_stack_set_visible_child(GTK_STACK(app_stack), first_page);
    
    gtk_window_present(GTK_WINDOW(launcher_window));
    gtk_widget_grab_focus(search_entry);
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
}
