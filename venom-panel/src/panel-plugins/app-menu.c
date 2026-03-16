#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>
#include <string.h>
#include "venom-panel-plugin-api.h"

/* The main popup menu */
static GtkWidget *main_menu = NULL;
static gboolean menu_populated = FALSE;

/* Known categories */
typedef struct {
    const char *id;
    const char *label;
    const char *icon_name;
    GtkWidget *submenu;
    GtkWidget *menu_item;
    int count;
} AppCategory;

static AppCategory categories[] = {
    {"Settings", "Settings", "preferences-desktop", NULL, NULL, 0},
    {"Utility", "Accessories", "applications-accessories", NULL, NULL, 0},
    {"Development", "Development", "applications-development", NULL, NULL, 0},
    {"Graphics", "Graphics", "applications-graphics", NULL, NULL, 0},
    {"Network", "Internet", "applications-internet", NULL, NULL, 0},
    {"AudioVideo", "Multimedia", "applications-multimedia", NULL, NULL, 0},
    {"Office", "Office", "applications-office", NULL, NULL, 0},
    {"System", "System", "applications-system", NULL, NULL, 0},
    {"Other", "Other", "applications-other", NULL, NULL, 0}
};

static const int NUM_CATEGORIES = sizeof(categories) / sizeof(categories[0]);

/* Helper to launch apps */
static void launch_app(GtkMenuItem *item, gpointer data) {
    (void)item;
    GDesktopAppInfo *app_info = G_DESKTOP_APP_INFO(data);
    GError *error = NULL;
    
    if (!g_app_info_launch(G_APP_INFO(app_info), NULL, NULL, &error)) {
        g_warning("[AppMenu] Failed to launch: %s", error->message);
        g_error_free(error);
    }
}

/* Helper to launch simple commands */
static void launch_cmd(GtkMenuItem *item, gpointer data) {
    (void)item;
    const char *cmd = (const char *)data;
    g_spawn_command_line_async(cmd, NULL);
}

/* Helper to create a menu item with an icon and label */
static GtkWidget* create_icon_menu_item(GIcon *icon, const char *icon_name, const char *label_text) {
    GtkWidget *item = gtk_menu_item_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    
    GtkWidget *img = NULL;
    if (icon) {
        img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
    } else if (icon_name) {
        img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    } else {
        img = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_MENU);
    }
    
    gtk_image_set_pixel_size(GTK_IMAGE(img), 16); /* Standard menu icon size */
    gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);
    
    GtkWidget *lbl = gtk_label_new(label_text);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), lbl, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(item), box);
    gtk_widget_show_all(item);
    return item;
}

/* Build the app menu structure */
static void build_app_menu(void) {
    if (menu_populated) return;
    
    /* 1. Static Top Items */
    GtkWidget *term_item = create_icon_menu_item(NULL, "utilities-terminal", "Terminal Emulator");
    g_signal_connect(term_item, "activate", G_CALLBACK(launch_cmd), "x-terminal-emulator");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), term_item);

    GtkWidget *fm_item = create_icon_menu_item(NULL, "system-file-manager", "File Manager");
    g_signal_connect(fm_item, "activate", G_CALLBACK(launch_cmd), "xdg-open /home/x"); /* or Thunar */
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), fm_item);

    GtkWidget *web_item = create_icon_menu_item(NULL, "web-browser", "Web Browser");
    g_signal_connect(web_item, "activate", G_CALLBACK(launch_cmd), "xdg-open http://");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), web_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), gtk_separator_menu_item_new());

    /* Initialize Category Submenus */
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        categories[i].submenu = gtk_menu_new();
        categories[i].menu_item = create_icon_menu_item(NULL, categories[i].icon_name, categories[i].label);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(categories[i].menu_item), categories[i].submenu);
        categories[i].count = 0;
    }

    /* 2. Populate Categories from .desktop files */
    GList *apps = g_app_info_get_all();
    for (GList *l = apps; l; l = l->next) {
        GAppInfo *app = G_APP_INFO(l->data);
        if (!g_app_info_should_show(app)) continue;

        const char *name = g_app_info_get_name(app);
        if (!name) continue;

        /* Find Category */
        AppCategory *target_cat = &categories[NUM_CATEGORIES - 1]; /* Default: Other */
        if (G_IS_DESKTOP_APP_INFO(app)) {
            const char *cats = g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(app));
            if (cats) {
                for (int i = 0; i < NUM_CATEGORIES - 1; i++) {
                    if (strstr(cats, categories[i].id)) {
                        target_cat = &categories[i];
                        break;
                    }
                }
            }
        }

        /* Create App Item */
        GIcon *icon = g_app_info_get_icon(app);
        GtkWidget *app_item = create_icon_menu_item(icon, NULL, name);
        
        g_object_ref(app);
        g_signal_connect(app_item, "activate", G_CALLBACK(launch_app), app);
        g_object_set_data_full(G_OBJECT(app_item), "app-info", app, g_object_unref);

        gtk_menu_shell_append(GTK_MENU_SHELL(target_cat->submenu), app_item);
        target_cat->count++;
    }
    g_list_free_full(apps, g_object_unref);

    /* Attach populated category submenus to main menu */
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        if (categories[i].count > 0) {
            gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), categories[i].menu_item);
        }
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), gtk_separator_menu_item_new());

    /* 3. Static Bottom Items */
    GtkWidget *about_item = create_icon_menu_item(NULL, "help-about", "About Venom...");
    /* g_signal_connect(about_item, "activate", G_CALLBACK(launch_cmd), "venom-about"); */
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), about_item);

    GtkWidget *logout_item = create_icon_menu_item(NULL, "system-log-out", "Log Out");
    g_signal_connect(logout_item, "activate", G_CALLBACK(launch_cmd), "venom-logout"); /* Or similar */
    gtk_menu_shell_append(GTK_MENU_SHELL(main_menu), logout_item);

    gtk_widget_show_all(main_menu);
    menu_populated = TRUE;
}

static void on_menu_button_toggled(GtkToggleButton *btn, gpointer data) {
    (void)data;
    if (gtk_toggle_button_get_active(btn)) {
        build_app_menu();
        /* Popup the main_menu exactly under the button */
        gtk_menu_popup_at_widget(GTK_MENU(main_menu), GTK_WIDGET(btn), 
                                 GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
    }
}

static void on_menu_deactivate(GtkMenuShell *shell, gpointer data) {
    (void)shell;
    GtkToggleButton *btn = GTK_TOGGLE_BUTTON(data);
    gtk_toggle_button_set_active(btn, FALSE);
}

static GtkWidget* create_app_menu_widget(void) {
    /* We use a generic toggle button to mimic the start menu trigger */
    GtkWidget *menu_btn = gtk_toggle_button_new();
    gtk_button_set_relief(GTK_BUTTON(menu_btn), GTK_RELIEF_NONE);
    
    /* We use start-here icon for the menu */
    GtkWidget *menu_icon = gtk_image_new_from_icon_name("start-here-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(menu_btn), menu_icon);
    
    /* Create the empty main menu attached to nothing initially */
    main_menu = gtk_menu_new();
    
    /* When the menu closes, un-toggle the button */
    g_signal_connect(main_menu, "deactivate", G_CALLBACK(on_menu_deactivate), menu_btn);
    
    /* When the button is toggled, show the menu */
    g_signal_connect(menu_btn, "toggled", G_CALLBACK(on_menu_button_toggled), NULL);

    gtk_widget_show_all(menu_btn);
    return menu_btn;
}

VenomPanelPluginAPI* venom_panel_plugin_init(void) {
    static VenomPanelPluginAPI api;
    api.name          = "App Menu";
    api.description   = "Traditional hierarchical application launcher.";
    api.author        = "Venom";
    api.expand        = FALSE;
    api.padding       = 2;
    api.create_widget = create_app_menu_widget;
    return &api;
}

VenomPanelPluginAPIv2* venom_panel_plugin_init_v2(void) {
    static VenomPanelPluginAPIv2 api = {
        .api_version = VENOM_PANEL_PLUGIN_API_VERSION,
        .struct_size = sizeof(VenomPanelPluginAPIv2),
        .name = "App Menu",
        .description = "Traditional hierarchical application launcher.",
        .author = "Venom",
        .zone = VENOM_PLUGIN_ZONE_LEFT,
        .priority = 0,
        .expand = FALSE,
        .padding = 2,
        .create_widget = create_app_menu_widget,
        .destroy_widget = NULL,
    };
    return &api;
}
