/*
 * venom-desktop-manager.c
 * Pro Desktop: Smart .desktop parsing, File Context Menus, Rubber Band Selection.
 *
 * Compile:
 * gcc -o venom-desktop-manager venom-desktop-manager.c $(pkg-config --cflags --libs gtk+-3.0 gio-2.0 gdesktop-app-info-1.0) -lm
 */

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h> /* ضروري لقراءة ملفات التطبيقات */
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>

#define ICON_SIZE 48

/* --- متغيرات التحديد (Rubber Band) --- */
static double start_x = 0, start_y = 0;
static double current_x = 0, current_y = 0;
static gboolean is_selecting = FALSE;
static GtkWidget *selection_layer = NULL; /* طبقة رسم المربع الأزرق */

/* --- دوال مساعدة --- */
static void open_file_uri(const char *uri) {
    GError *err = NULL;
    g_app_info_launch_default_for_uri(uri, NULL, &err);
    if (err) {
        g_printerr("Error opening: %s\n", err->message);
        g_error_free(err);
    }
}

static void delete_file(const char *uri) {
    GFile *file = g_file_new_for_uri(uri);
    g_file_trash(file, NULL, NULL); /* نقل للمهملات وليس حذف نهائي */
    g_object_unref(file);
    /* ملاحظة: لتحديث العرض يجب إعادة مسح المجلد، سنكتفي بالحذف الآن */
}

/* --- قائمة الملفات (Item Context Menu) --- */
static void on_item_open(GtkWidget *menuitem, gpointer data) { open_file_uri((char*)data); }
static void on_item_delete(GtkWidget *menuitem, gpointer data) { delete_file((char*)data); }

static gboolean on_item_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    char *uri = (char *)data;

    /* كليك يمين: قائمة الملف */
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkWidget *menu = gtk_menu_new();
        
        GtkWidget *i_open = gtk_menu_item_new_with_label("Open");
        GtkWidget *i_del = gtk_menu_item_new_with_label("Move to Trash");
        
        g_signal_connect(i_open, "activate", G_CALLBACK(on_item_open), uri);
        g_signal_connect(i_del, "activate", G_CALLBACK(on_item_delete), uri);
        
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_open);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_del);
        
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
        return TRUE; /* تم التعامل مع الحدث */
    }
    
    /* Double Click: فتح الملف */
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
        open_file_uri(uri);
        return TRUE;
    }
    
    return FALSE; /* تمرير الحدث (للتحديد مثلاً) */
}

/* --- إنشاء الأيقونة بذكاء (.desktop parser) --- */
static GtkWidget* create_desktop_item(GFileInfo *info, const char *full_path) {
    const char *filename = g_file_info_get_name(info);
    char *display_name = g_strdup(g_file_info_get_display_name(info));
    GIcon *gicon = g_object_ref(g_file_info_get_icon(info));
    
    /* ✅ الحل لمشكلة واتساب وتطبيقات .desktop */
    if (g_str_has_suffix(filename, ".desktop")) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(full_path);
        if (app_info) {
            /* استبدال الاسم والايقونة بمعلومات التطبيق الحقيقية */
            g_free(display_name);
            display_name = g_strdup(g_app_info_get_name(G_APP_INFO(app_info)));
            
            if (gicon) g_object_unref(gicon);
            gicon = g_app_info_get_icon(G_APP_INFO(app_info));
            if (gicon) g_object_ref(gicon);
            
            g_object_unref(app_info);
        }
    }

    /* إنشاء الزر */
    GtkWidget *btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_widget_set_name(btn, "desktop-item");
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(btn), box);

    GtkWidget *image = gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(image), ICON_SIZE);
    gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);

    GtkWidget *label = gtk_label_new(display_name);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 14);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

    /* نحفظ الـ URI مع الزر لاستخدامه لاحقاً */
    GFile *f = g_file_new_for_path(full_path);
    char *uri = g_file_get_uri(f);
    g_signal_connect_data(btn, "button-press-event", G_CALLBACK(on_item_button_press), 
                          uri, (GClosureNotify)g_free, 0);
    
    g_object_unref(f);
    g_free(display_name);
    if (gicon) g_object_unref(gicon);

    gtk_widget_show_all(btn);
    return btn;
}

/* --- منطق التحديد (Rubber Band Drawing) --- */
static gboolean on_selection_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    if (is_selecting) {
        double x = MIN(start_x, current_x);
        double y = MIN(start_y, current_y);
        double w = fabs(current_x - start_x);
        double h = fabs(current_y - start_y);

        /* رسم المربع الأزرق الشفاف */
        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.3); /* تعبئة زرقاء شفافة */
        cairo_rectangle(cr, x, y, w, h);
        cairo_fill(cr);

        /* رسم الإطار */
        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.8); /* إطار أزرق غامق */
        cairo_set_line_width(cr, 1.0);
        cairo_rectangle(cr, x, y, w, h);
        cairo_stroke(cr);
    }
    return FALSE;
}

/* أحداث الماوس للخلفية (بدء التحديد) */
static gboolean on_bg_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1) {
        /* بدء التحديد */
        is_selecting = TRUE;
        start_x = event->x;
        start_y = event->y;
        current_x = event->x;
        current_y = event->y;
        gtk_widget_queue_draw(selection_layer);
        return TRUE;
    }
    /* كليك يمين للخلفية (القائمة العامة) */
    if (event->button == 3) {
        // ... (كود القائمة العامة السابق) ...
        return TRUE;
    }
    return FALSE;
}

static gboolean on_bg_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (is_selecting) {
        current_x = event->x;
        current_y = event->y;
        gtk_widget_queue_draw(selection_layer); /* إعادة رسم المربع */
    }
    return FALSE;
}

static gboolean on_bg_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1 && is_selecting) {
        is_selecting = FALSE;
        gtk_widget_queue_draw(selection_layer); /* مسح المربع */
        /* هنا يمكنك إضافة منطق لاختيار الأيقونات التي تقع داخل المربع */
    }
    return FALSE;
}

/* --- تحميل الأيقونات --- */
static void load_desktop_icons(GtkWidget *flowbox) {
    const char *home = getenv("HOME");
    char *desktop_path = g_strdup_printf("%s/Desktop", home);
    GFile *dir = g_file_new_for_path(desktop_path);
    
    GFileEnumerator *enumerator = g_file_enumerate_children(dir,
        "standard::*", G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (enumerator) {
        GFileInfo *info;
        while ((info = g_file_enumerator_next_file(enumerator, NULL, NULL))) {
            const char *fname = g_file_info_get_name(info);
            if (fname[0] != '.') {
                char *full_path = g_strdup_printf("%s/%s", desktop_path, fname);
                GtkWidget *item = create_desktop_item(info, full_path);
                gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), item, -1);
                g_free(full_path);
            }
            g_object_unref(info);
        }
        g_object_unref(enumerator);
    }
    g_object_unref(dir);
    g_free(desktop_path);
}

/* --- MAIN --- */
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
    gtk_window_set_default_size(GTK_WINDOW(window), r.width, r.height);
    gtk_window_move(GTK_WINDOW(window), 0, 0);

    /* Overlay Layers */
    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(window), overlay);

    /* 1. Background Event Area (For Rubber Band & Right Click) */
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

    /* 2. Icons Layer */
    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    gtk_widget_set_halign(flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 20);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_MULTIPLE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 20);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 20);
    gtk_widget_set_margin_top(flowbox, 40);
    gtk_widget_set_margin_start(flowbox, 20);

    /* CSS */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, 
        "#desktop-item { background: transparent; border-radius: 5px; padding: 8px; transition: all 0.1s; }"
        "#desktop-item:hover { background: rgba(255,255,255,0.15); }"
        "#desktop-item:selected { background: rgba(52, 152, 219, 0.4); }" /* تحديد العنصر */
        "label { color: white; text-shadow: 1px 1px 2px black; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css), 800);

    /* Make flowbox transparent to events where there are no icons */
    /* ملاحظة: في GTK3 صعب جعل الـ Container يمرر الأحداث للخلفية، لذلك نضعه فوق */
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), flowbox);

    load_desktop_icons(flowbox);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
    malloc_trim(0);
    gtk_main();

    return 0;
}