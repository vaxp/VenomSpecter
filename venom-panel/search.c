#include "search.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gio/gdesktopappinfo.h>

/* --- Dialog Implementations --- */

/* Password Dialog */
static gchar *show_password_dialog(GtkWidget *parent) {
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
    
    /* Remove window decoration */
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    
    /* Apply CSS styling */
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data = 
        "dialog { background: rgba(0, 0, 0, 0.541); }"
        "dialog label { color: white; }"
        "dialog button { color: white; }";
    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);
    GtkStyleContext *style_ctx = gtk_widget_get_style_context(dialog);
    gtk_style_context_add_provider(style_ctx, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
    
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 20);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content_area), 5);

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
static void show_math_result_dialog(GtkWidget *parent, const gchar *expression, const gchar *result) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_CLOSE,
                                               "%s\n= %s", expression, result);
    gtk_window_set_title(GTK_WINDOW(dialog), "Calculation Result");
    
    /* Remove window decoration */
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    
    /* Apply CSS styling */
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data = 
        "dialog { background: rgba(0, 0, 0, 0.541); }"
        "dialog label { color: white; }"
        "dialog button { color: white; }";
    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);
    GtkStyleContext *style_ctx = gtk_widget_get_style_context(dialog);
    gtk_style_context_add_provider(style_ctx, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/* File Results Dialog */
static void on_file_result_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    GtkWidget *dialog = GTK_WIDGET(data);  /* This is the dialog itself, not the parent */
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(row));
    const gchar *path = g_object_get_data(G_OBJECT(child), "file-path");
    
    if (path) {
        /* Open file in the default file manager */
        GError *error = NULL;
        
        /* Get the directory of the file */
        gchar *directory = g_path_get_dirname(path);
        gchar *uri = g_filename_to_uri(directory, NULL, NULL);
        
        if (uri) {
            /* Try to open with file manager */
            g_app_info_launch_default_for_uri(uri, NULL, &error);
            
            if (error) {
                g_warning("Failed to open file manager: %s", error->message);
                g_error_free(error);
            }
            g_free(uri);
        }
        g_free(directory);
        
        /* Close only the dialog window, not the parent */
        gtk_widget_destroy(dialog);
    }
}

static void show_file_results_dialog(GtkWidget *parent, const gchar *term, GList *files) {
    (void)term;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("File Search Results",
                                                    GTK_WINDOW(parent),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_Close", GTK_RESPONSE_CLOSE,
                                                    NULL);
    
    /* Remove window decoration (title bar) */
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    
    /* زيادة حجم النافذة لعرض النتائج بشكل أفضل */
    gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 600);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    
    /* Apply CSS styling */
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data = 
        "dialog { background: rgba(0, 0, 0, 0.541); }"
        "dialog > .titlebar { background: rgba(0, 0, 0, 0.541); border: none; }"
        "dialog label, dialog button { color: white; }";
    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);
    GtkStyleContext *style_ctx = gtk_widget_get_style_context(dialog);
    gtk_style_context_add_provider(style_ctx, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 10);
    
    if (files == NULL) {
        GtkWidget *label = gtk_label_new("No files found.");
        gtk_container_add(GTK_CONTAINER(content_area), label);
    } else {
        /* Scrolled window that expands to fill the dialog */
        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_widget_set_vexpand(scrolled, TRUE);
        gtk_widget_set_hexpand(scrolled, TRUE);
        gtk_container_add(GTK_CONTAINER(content_area), scrolled);
        
        GtkWidget *listbox = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_SINGLE);
        gtk_container_add(GTK_CONTAINER(scrolled), listbox);
        /* Pass the dialog itself, not the parent, so we can close only the dialog */
        g_signal_connect(listbox, "row-activated", G_CALLBACK(on_file_result_activated), dialog);
        
        for (GList *l = files; l != NULL; l = l->next) {
            gchar *path = (gchar *)l->data;
            gchar *basename = g_path_get_basename(path);
            
            /* Create a more prominent row */
            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
            gtk_widget_set_margin_start(row_box, 15);
            gtk_widget_set_margin_end(row_box, 15);
            gtk_widget_set_margin_top(row_box, 10);
            gtk_widget_set_margin_bottom(row_box, 10);
            g_object_set_data_full(G_OBJECT(row_box), "file-path", g_strdup(path), g_free);
            
            /* File name - bold and larger */
            GtkWidget *name_label = gtk_label_new(basename);
            gtk_widget_set_halign(name_label, GTK_ALIGN_START);
            gchar *markup = g_strdup_printf("<b><span size='large'>%s</span></b>", basename);
            gtk_label_set_markup(GTK_LABEL(name_label), markup);
            gtk_label_set_line_wrap(GTK_LABEL(name_label), TRUE);
            g_free(markup);
            
            /* Full path - smaller and dimmed */
            GtkWidget *path_label = gtk_label_new(path);
            gtk_widget_set_halign(path_label, GTK_ALIGN_START);
            gtk_label_set_ellipsize(GTK_LABEL(path_label), PANGO_ELLIPSIZE_MIDDLE);
            gtk_label_set_line_wrap(GTK_LABEL(path_label), TRUE);
            GtkStyleContext *context = gtk_widget_get_style_context(path_label);
            gtk_style_context_add_class(context, "dim-label");
            gchar *path_markup = g_strdup_printf("<span size='small' foreground='#888888'>%s</span>", path);
            gtk_label_set_markup(GTK_LABEL(path_label), path_markup);
            g_free(path_markup);
            
            gtk_box_pack_start(GTK_BOX(row_box), name_label, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(row_box), path_label, FALSE, FALSE, 0);
            
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
        if (console->stdout_watch > 0 && g_source_remove_by_user_data(console)) {
             /* Handled by helper functions below */
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
    (void)response;
    ConsoleData *console = (ConsoleData *)data;
    
    if (console->stdout_watch > 0) {
        g_source_remove(console->stdout_watch);
        console->stdout_watch = 0;
    }
    if (console->stderr_watch > 0) {
        g_source_remove(console->stderr_watch);
        console->stderr_watch = 0;
    }
    
    /* Destroy the dialog */
    gtk_widget_destroy(dialog);
    
    g_free(console);
}

static void show_command_console(GtkWidget *parent, const gchar *command, const gchar *password) {
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
    
    /* Remove window decoration */
    gtk_window_set_decorated(GTK_WINDOW(console->dialog), FALSE);
    
    /* Apply CSS styling */
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data = 
        "dialog { background: rgba(0, 0, 0, 0.541); }"
        "dialog label { color: white; }"
        "dialog button { color: white; }"
        "dialog textview { background: rgba(0, 0, 0, 0.8); color: #00ff00; }";
    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);
    GtkStyleContext *style_ctx = gtk_widget_get_style_context(console->dialog);
    gtk_style_context_add_provider(style_ctx, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
    
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
    gtk_widget_show_all(console->dialog);
}

/* --- Execution Handlers --- */

static gchar *execute_command_sync(const gchar *cmd) {
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

void execute_vater(const gchar *cmd, GtkWidget *window) {
    if (cmd && strlen(cmd) > 0) {
        gchar *password = NULL;
        /* Check if sudo is needed */
        if (strstr(cmd, "sudo")) {
            password = show_password_dialog(window);
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
            
            show_command_console(window, final_cmd, password);
            g_free(final_cmd);
            g_free(password);
        } else {
            show_command_console(window, cmd, NULL);
        }
    }
}

void execute_math(const gchar *expr, GtkWidget *window) {
    if (expr && strlen(expr) > 0) {
        gchar *cmd = g_strdup_printf("echo \"%s\" | bc -l", expr);
        gchar *output = execute_command_sync(cmd);
        g_strstrip(output);
        show_math_result_dialog(window, expr, output);
        g_free(output);
        g_free(cmd);
    }
}

void execute_file_search(const gchar *term, GtkWidget *window) {
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
        show_file_results_dialog(window, term, files);
        g_list_free_full(files, g_free);
    }
}

void execute_web_search(const gchar *term, const gchar *engine, GtkWidget *window, GtkWidget **stack_ptr, GtkWidget **entry_ptr, GtkWidget **results_ptr) {
    if (term && strlen(term) > 0) {
        gchar *url;
        if (g_strcmp0(engine, "github") == 0) {
            url = g_strdup_printf("https://github.com/search?q=%s", term);
        } else {
            url = g_strdup_printf("https://www.google.com/search?q=%s", term);
        }
        gtk_show_uri_on_window(GTK_WINDOW(window), url, GDK_CURRENT_TIME, NULL);
        g_free(url);
        
        /* Close launcher */
        if (window) {
            gtk_widget_destroy(window);
            /* Note: The caller needs to handle nullifying their pointers if they are global */
            /* We can't do it here easily unless we pass pointers to pointers */
            /* Passed pointers to allow nullifying */
            if (stack_ptr) *stack_ptr = NULL;
            if (entry_ptr) *entry_ptr = NULL;
            if (results_ptr) *results_ptr = NULL;
            /* window pointer itself needs to be nullified by caller or we pass GtkWidget** */
        }
    }
}

/* --- Main Search Logic --- */

void perform_search(const gchar *text, GtkWidget *stack, GtkWidget *results_view, GtkWidget *window) {
    (void)window;
    if (results_view == NULL) return;
    
    /* Clear previous results */
    GList *children = gtk_container_get_children(GTK_CONTAINER(results_view));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
    
    if (text == NULL || strlen(text) == 0) {
        GtkWidget *first_page = gtk_stack_get_child_by_name(GTK_STACK(stack), "page0");
        if (first_page) gtk_stack_set_visible_child(GTK_STACK(stack), first_page);
        return;
    }
    
    /* Switch to search results view */
    GtkWidget *results_scroll = gtk_stack_get_child_by_name(GTK_STACK(stack), "search_results");
    if (results_scroll) gtk_stack_set_visible_child(GTK_STACK(stack), results_scroll);
    
    /* Special Commands Handling - Show Info Labels */
    
    if (g_str_has_prefix(text, "vater:")) {
        GtkWidget *label = gtk_label_new("Press Enter to execute command...");
        gtk_box_pack_start(GTK_BOX(results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(results_view);
        return;
    }
    
    if (g_str_has_prefix(text, "!:")) {
        GtkWidget *label = gtk_label_new("Press Enter to calculate...");
        gtk_box_pack_start(GTK_BOX(results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(results_view);
        return;
    }
    
    if (g_str_has_prefix(text, "vafile:")) {
        GtkWidget *label = gtk_label_new("Press Enter to search files...");
        gtk_box_pack_start(GTK_BOX(results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(results_view);
        return;
    }
    
    if (g_str_has_prefix(text, "g:")) {
        GtkWidget *label = gtk_label_new("Press Enter to search GitHub...");
        gtk_box_pack_start(GTK_BOX(results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(results_view);
        return;
    }
    
    if (g_str_has_prefix(text, "s:")) {
        GtkWidget *label = gtk_label_new("Press Enter to search Google...");
        gtk_box_pack_start(GTK_BOX(results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(results_view);
        return;
    }

    if (g_str_has_prefix(text, "ai:")) {
        GtkWidget *label = gtk_label_new("Press Enter to ask Admiral AI...");
        gtk_box_pack_start(GTK_BOX(results_view), label, FALSE, FALSE, 0);
        gtk_widget_show_all(results_view);
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
                            
                            gtk_box_pack_start(GTK_BOX(results_view), button, FALSE, FALSE, 0);
                            
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
    
    gtk_widget_show_all(results_view);
}

/* --- AI Chat Implementation --- */

typedef struct {
    GtkWidget *window;
    GtkWidget *text_view;
    GtkWidget *entry;
    GtkWidget *status_label;
    GtkWidget *spinner;
    gchar *full_response;
    gint current_index;
    guint typewriter_timer;
    GPid pid;
    gint stdout_fd;
    guint stdout_watch;
} AiChatData;

static void ai_chat_cleanup(AiChatData *data) {
    if (data->typewriter_timer > 0) {
        g_source_remove(data->typewriter_timer);
        data->typewriter_timer = 0;
    }
    if (data->stdout_watch > 0) {
        g_source_remove(data->stdout_watch);
        data->stdout_watch = 0;
    }
    if (data->full_response) {
        g_free(data->full_response);
        data->full_response = NULL;
    }
    g_free(data);
}

static gboolean typewriter_step(gpointer user_data) {
    AiChatData *data = (AiChatData *)user_data;
    
    if (!data->full_response || data->full_response[data->current_index] == '\0') {
        data->typewriter_timer = 0;
        gtk_label_set_text(GTK_LABEL(data->status_label), "Done.");
        return FALSE; /* Stop timer */
    }
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->text_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    
    /* Append one character (handle UTF-8 properly) */
    gchar *p = data->full_response + data->current_index;
    gchar *next = g_utf8_next_char(p);
    gint len = next - p;
    
    gtk_text_buffer_insert(buffer, &end, p, len);
    data->current_index += len;
    
    /* Scroll to bottom */
    GtkTextMark *mark = gtk_text_buffer_create_mark(buffer, NULL, &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(data->text_view), mark, 0.0, TRUE, 0.0, 1.0);
    gtk_text_buffer_delete_mark(buffer, mark);
    
    return TRUE;
}

static void start_typewriter(AiChatData *data) {
    if (data->typewriter_timer > 0) {
        g_source_remove(data->typewriter_timer);
    }
    data->current_index = 0;
    gtk_label_set_text(GTK_LABEL(data->status_label), "Typing...");
    data->typewriter_timer = g_timeout_add(10, typewriter_step, data);
}

static gboolean on_ai_stdout(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    AiChatData *data = (AiChatData *)user_data;
    GIOStatus status;
    gchar *str;
    gsize len;
    
    if (condition & G_IO_IN) {
        status = g_io_channel_read_to_end(source, &str, &len, NULL);
        if (status == G_IO_STATUS_NORMAL) {
            if (data->full_response) {
                gchar *tmp = g_strconcat(data->full_response, str, NULL);
                g_free(data->full_response);
                data->full_response = tmp;
            } else {
                data->full_response = g_strdup(str);
            }
            g_free(str);
        }
    }
    
    if (condition & (G_IO_HUP | G_IO_ERR)) {
        gtk_spinner_stop(GTK_SPINNER(data->spinner));
        if (data->full_response) {
            start_typewriter(data);
        } else {
            gtk_label_set_text(GTK_LABEL(data->status_label), "No response.");
        }
        data->stdout_watch = 0;
        return FALSE; /* Remove source */
    }
    
    return TRUE;
}

static void fetch_ai_response(AiChatData *data, const gchar *query) {
    if (!query || strlen(query) == 0) return;
    
    /* Reset state */
    if (data->full_response) {
        g_free(data->full_response);
        data->full_response = NULL;
    }
    if (data->typewriter_timer > 0) {
        g_source_remove(data->typewriter_timer);
        data->typewriter_timer = 0;
    }
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->text_view)), "", -1);
    gtk_label_set_text(GTK_LABEL(data->status_label), "Admiral is analyzing...");
    gtk_spinner_start(GTK_SPINNER(data->spinner));
    
    /* Prepare curl command */
    gchar *encoded_query = g_uri_escape_string(query, NULL, TRUE);
    gchar *url = g_strdup_printf("https://text.pollinations.ai/%s", encoded_query);
    g_free(encoded_query);
    
    gchar *argv[] = {"curl", "-s", url, NULL};
    GError *error = NULL;
    gint stdout_pipe;
    
    if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &data->pid, NULL, &stdout_pipe, NULL, &error)) {
        GIOChannel *ch = g_io_channel_unix_new(stdout_pipe);
        data->stdout_watch = g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR, on_ai_stdout, data);
        g_io_channel_unref(ch);
    } else {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Failed to connect.");
        gtk_spinner_stop(GTK_SPINNER(data->spinner));
        g_warning("AI Spawn Error: %s", error->message);
        g_error_free(error);
    }
    
    g_free(url);
}

static void on_ai_entry_activate(GtkEntry *entry, gpointer user_data) {
    AiChatData *data = (AiChatData *)user_data;
    const gchar *text = gtk_entry_get_text(entry);
    
    if (g_str_has_prefix(text, "ai:")) {
        fetch_ai_response(data, text + 3);
    } else {
        fetch_ai_response(data, text);
    }
}

static gboolean on_ai_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget; (void)event;
    AiChatData *data = (AiChatData *)user_data;
    ai_chat_cleanup(data);
    return FALSE; /* Propagate destroy */
}

void execute_ai_chat(const gchar *initial_query, GtkWidget *parent) {
    AiChatData *data = g_new0(AiChatData, 1);
    
    /* Window Setup */
    data->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(data->window), "Admiral AI");
    gtk_window_set_default_size(GTK_WINDOW(data->window), 800, 550);
    gtk_window_set_position(GTK_WINDOW(data->window), GTK_WIN_POS_CENTER);
    gtk_window_set_transient_for(GTK_WINDOW(data->window), GTK_WINDOW(parent));
    gtk_window_set_modal(GTK_WINDOW(data->window), TRUE);
    gtk_widget_set_name(data->window, "ai-window");
    
    /* Visual Transparency */
    GdkScreen *screen = gtk_widget_get_screen(data->window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(data->window, visual);
    }
    
    /* Layout */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 20);
    gtk_container_add(GTK_CONTAINER(data->window), main_box);
    
    /* Header */
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name("dialog-information", GTK_ICON_SIZE_MENU);
    GtkWidget *title = gtk_label_new("Admiral AI");
    GtkStyleContext *title_ctx = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(title_ctx, "h1");
    
    GtkWidget *beta_label = gtk_label_new("BETA");
    GtkStyleContext *beta_ctx = gtk_widget_get_style_context(beta_label);
    gtk_style_context_add_class(beta_ctx, "badge");
    
    gtk_box_pack_start(GTK_BOX(header_box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), title, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header_box), beta_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), header_box, FALSE, FALSE, 0);
    
    /* Input */
    data->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->entry), "Type 'ai: ...'");
    if (initial_query) {
        gchar *full_text = g_strdup_printf("ai: %s", initial_query);
        gtk_entry_set_text(GTK_ENTRY(data->entry), full_text);
        g_free(full_text);
    }
    g_signal_connect(data->entry, "activate", G_CALLBACK(on_ai_entry_activate), data);
    gtk_box_pack_start(GTK_BOX(main_box), data->entry, FALSE, FALSE, 0);
    
    /* Response Area */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    data->text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(data->text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(data->text_view), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(data->text_view), 10);
    gtk_container_add(GTK_CONTAINER(scrolled), data->text_view);
    gtk_box_pack_start(GTK_BOX(main_box), scrolled, TRUE, TRUE, 0);
    
    /* Status Footer */
    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    data->spinner = gtk_spinner_new();
    data->status_label = gtk_label_new("Ready.");
    gtk_box_pack_start(GTK_BOX(footer_box), data->spinner, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(footer_box), data->status_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), footer_box, FALSE, FALSE, 0);
    
    g_signal_connect(data->window, "delete-event", G_CALLBACK(on_ai_window_delete), data);
    
    gtk_widget_show_all(data->window);
    
    if (initial_query) {
        fetch_ai_response(data, initial_query);
    }
}
