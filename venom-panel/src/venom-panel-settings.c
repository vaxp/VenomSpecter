#include <gtk/gtk.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include "venom-panel-plugin-api.h"

#define PANEL_CONFIG_FILE "/home/x/.config/venom/panel.conf"
#define PANEL_PLUGINS_DIR "/home/x/.config/venom/panel-plugins"

typedef struct {
    char type[32];
    char name_or_file[128];
    gboolean expand;
    int padding;
} ConfigItem;

static GList *config_items = NULL;
static GtkWidget *listbox;

/* --- Parser & Writer --- */
static void load_config(void) {
    g_list_free_full(config_items, g_free);
    config_items = NULL;

    char *contents = NULL;
    if (!g_file_get_contents(PANEL_CONFIG_FILE, &contents, NULL, NULL)) return;

    ConfigItem *cur = NULL;
    char **lines = g_strsplit(contents, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        char *line = g_strstrip(lines[i]);
        if (!line[0] || line[0] == '#') continue;
        if (!g_strcmp0(line, "[item]")) {
            if (cur) config_items = g_list_append(config_items, cur);
            cur = g_new0(ConfigItem, 1);
            cur->expand = FALSE; cur->padding = 2;
            continue;
        }
        if (!cur) continue;
        char **kv = g_strsplit(line, "=", 2);
        if (kv[0] && kv[1]) {
            char *k = g_strstrip(kv[0]); char *v = g_strstrip(kv[1]);
            if (!g_strcmp0(k, "type")) strncpy(cur->type, v, 31);
            else if (!g_strcmp0(k, "file") || !g_strcmp0(k, "name")) strncpy(cur->name_or_file, v, 127);
            else if (!g_strcmp0(k, "expand")) cur->expand = !g_strcmp0(v, "true");
            else if (!g_strcmp0(k, "padding")) cur->padding = atoi(v);
        }
        g_strfreev(kv);
    }
    if (cur) config_items = g_list_append(config_items, cur);
    g_strfreev(lines);
    g_free(contents);
}

static void save_config(void) {
    GString *out = g_string_new("# venom-panel layout config\n\n");
    for (GList *l = config_items; l; l = l->next) {
        ConfigItem *item = l->data;
        g_string_append(out, "[item]\n");
        g_string_append_printf(out, "type=%s\n", item->type);
        if (!g_strcmp0(item->type, "plugin"))
            g_string_append_printf(out, "file=%s\n", item->name_or_file);
        else if (!g_strcmp0(item->type, "builtin"))
            g_string_append_printf(out, "name=%s\n", item->name_or_file);
        
        if (g_strcmp0(item->type, "separator") != 0 && g_strcmp0(item->type, "spacer") != 0) {
            g_string_append_printf(out, "expand=%s\n", item->expand ? "true" : "false");
            g_string_append_printf(out, "padding=%d\n", item->padding);
        } else if (!g_strcmp0(item->type, "spacer")) {
            g_string_append_printf(out, "expand=%s\n", item->expand ? "true" : "false");
        }
        g_string_append(out, "\n");
    }
    g_file_set_contents(PANEL_CONFIG_FILE, out->str, -1, NULL);
    g_string_free(out, TRUE);
    
    /* Reload panel */
    g_spawn_command_line_async("killall -SIGUSR1 venom-panel", NULL);
}

/* --- UI Helpers --- */
static void on_remove_clicked(GtkButton *btn, gpointer data) {
    GtkWidget *row = GTK_WIDGET(data);
    int index = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row));
    ConfigItem *item = g_list_nth_data(config_items, index);
    config_items = g_list_remove(config_items, item);
    g_free(item);
    gtk_widget_destroy(row);
    save_config();
}

static void rebuild_listbox(void);

static void on_move_up(GtkButton *btn, gpointer data) {
    int idx = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(data));
    if (idx <= 0) return;
    GList *l = g_list_nth(config_items, idx);
    ConfigItem *cur = l->data;
    l->data = l->prev->data;
    l->prev->data = cur;
    save_config();
    rebuild_listbox();
}

static void on_move_down(GtkButton *btn, gpointer data) {
    int idx = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(data));
    GList *l = g_list_nth(config_items, idx);
    if (!l || !l->next) return;
    ConfigItem *cur = l->data;
    l->data = l->next->data;
    l->next->data = cur;
    save_config();
    rebuild_listbox();
}

static GtkWidget* create_row(ConfigItem *item) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);

    char label_text[256];
    const char *icon_name = "application-x-addon";
    if (!g_strcmp0(item->type, "separator")) {
        snprintf(label_text, sizeof(label_text), "<b>Separator</b>");
        icon_name = "list-remove-symbolic";
    } else if (!g_strcmp0(item->type, "spacer")) {
        snprintf(label_text, sizeof(label_text), "<b>Spacer</b> (expands: %s)", item->expand ? "yes" : "no");
        icon_name = "go-next-symbolic"; 
    } else {
        snprintf(label_text, sizeof(label_text), "<b>%s</b> <small>(%s)</small>", item->name_or_file, item->type);
    }

    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), label_text);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), lbl, TRUE, TRUE, 0);

    GtkWidget *btn_up = gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *btn_dn = gtk_button_new_from_icon_name("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *btn_rm = gtk_button_new_from_icon_name("user-trash-symbolic", GTK_ICON_SIZE_BUTTON);
    
    g_signal_connect(btn_up, "clicked", G_CALLBACK(on_move_up), row);
    g_signal_connect(btn_dn, "clicked", G_CALLBACK(on_move_down), row);
    g_signal_connect(btn_rm, "clicked", G_CALLBACK(on_remove_clicked), row);
    
    gtk_box_pack_end(GTK_BOX(box), btn_rm, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), btn_dn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), btn_up, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(row), box);
    gtk_widget_show_all(row);
    return row;
}

static void rebuild_listbox(void) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(listbox));
    for (GList *l = children; l; l = l->next) gtk_widget_destroy(l->data);
    g_list_free(children);

    for (GList *l = config_items; l; l = l->next) {
        gtk_list_box_insert(GTK_LIST_BOX(listbox), create_row((ConfigItem*)l->data), -1);
    }
}

static void on_add_item(GtkButton *btn, gpointer data) {
    (void)btn;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Add Item", GTK_WINDOW(data),
                                          GTK_DIALOG_MODAL,
                                          "Cancel", GTK_RESPONSE_CANCEL,
                                          "Add", GTK_RESPONSE_ACCEPT,
                                          NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *combo = gtk_combo_box_text_new();
    
    /* Dynamically load plugins from directory */
    DIR *dir = opendir(PANEL_PLUGINS_DIR);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (g_str_has_suffix(ent->d_name, ".so")) {
                char *full_path = g_build_filename(PANEL_PLUGINS_DIR, ent->d_name, NULL);
                void *handle = dlopen(full_path, RTLD_LAZY | RTLD_LOCAL);
                if (handle) {
                    VenomPanelPluginInitFn init_func = (VenomPanelPluginInitFn)dlsym(handle, "venom_panel_plugin_init");
                    if (init_func) {
                        VenomPanelPluginAPI *api = init_func();
                        char id_str[256];
                        char label_str[256];
                        snprintf(id_str, sizeof(id_str), "plugin:%s", ent->d_name);
                        snprintf(label_str, sizeof(label_str), "Plugin: %s", api->name ? api->name : ent->d_name);
                        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), id_str, label_str);
                    }
                    dlclose(handle);
                }
                g_free(full_path);
            }
        }
        closedir(dir);
    }
    
    /* Append static builtin items */
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "builtin:tray", "Builtin: System Tray");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "builtin:power", "Builtin: Power Menu");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "builtin:clock", "Builtin: Clock");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "builtin:system-icons", "Builtin: Wi-Fi / Sound");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "builtin:control-center", "Builtin: Control Center Button");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "spacer", "Spacer (Expands)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "separator", "Separator Line");
    
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    
    gtk_box_pack_start(GTK_BOX(content), combo, TRUE, TRUE, 8);
    gtk_widget_show_all(dialog);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
        if (id) {
            ConfigItem *it = g_new0(ConfigItem, 1);
            it->expand = FALSE; it->padding = 2;
            if (!g_strcmp0(id, "spacer")) { strcpy(it->type, "spacer"); it->expand = TRUE; }
            else if (!g_strcmp0(id, "separator")) { strcpy(it->type, "separator"); }
            else {
                char **parts = g_strsplit(id, ":", 2);
                strcpy(it->type, parts[0]);
                strcpy(it->name_or_file, parts[1]);
                g_strfreev(parts);
            }
            config_items = g_list_append(config_items, it);
            save_config();
            rebuild_listbox();
        }
    }
    gtk_widget_destroy(dialog);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    load_config();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Panel Preferences");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 500);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* Header */
    GtkWidget *header = gtk_label_new("<b>Panel Items</b>");
    gtk_label_set_use_markup(GTK_LABEL(header), TRUE);
    gtk_widget_set_margin_top(header, 12);
    gtk_widget_set_margin_bottom(header, 12);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    /* ListBox inside ScrolledWindow */
    listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_NONE);
    rebuild_listbox();

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), listbox);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    /* Toolbar */
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(toolbar, 8);
    gtk_widget_set_margin_end(toolbar, 8);
    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    
    GtkWidget *btn_add = gtk_button_new_with_label("+ Add Item");
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_item), window);
    gtk_box_pack_start(GTK_BOX(toolbar), btn_add, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
