// #include <gtk/gtk.h>
// #include <gdk/gdkx.h>
// #include <stdlib.h>

// /* نستخدم cairo_surface_t بدلاً من GdkPixbuf لأنه أخف وأسرع في الرسم */
// static cairo_surface_t *bg_surface = NULL;
// static GtkWidget *drawing_area = NULL;
// static int screen_w = 0;
// static int screen_h = 0;

// /* --- تحميل الصورة وتحويلها لسطح Cairo وحذف الأصل --- */
// static void load_wallpaper(const char *path, GtkWidget *widget) {
//     GError *error = NULL;
    
//     /* 1. تنظيف الذاكرة القديمة فوراً */
//     if (bg_surface) {
//         cairo_surface_destroy(bg_surface);
//         bg_surface = NULL;
//     }

//     /* 2. تحميل الصورة كـ Pixbuf مؤقت */
//     GdkPixbuf *temp_pixbuf = gdk_pixbuf_new_from_file_at_scale(path, screen_w, screen_h, FALSE, &error);

//     if (!temp_pixbuf) {
//         g_warning("Failed to load wallpaper: %s", error->message);
//         g_error_free(error);
//         return;
//     }

//     /* 3. تحويل Pixbuf إلى Cairo Surface (أسرع وأخف) */
//     GdkWindow *window = gtk_widget_get_window(widget);
//     /* ننشئ سطحاً مشابهاً لنافذتنا */
//     bg_surface = gdk_cairo_surface_create_from_pixbuf(temp_pixbuf, 0, window);

//     /* 4. ✅ الخطوة الحاسمة: حذف الـ Pixbuf من الرام فوراً */
//     g_object_unref(temp_pixbuf);

//     /* 5. إعادة الرسم */
//     if (drawing_area) gtk_widget_queue_draw(drawing_area);
    
//     /* محاولة تنظيف الذاكرة غير المستخدمة */
//     malloc_trim(0); 
// }

// /* --- دالة الرسم السريعة --- */
// static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
//     if (bg_surface) {
//         /* رسم السطح الجاهز مباشرة (Blit) - لا يحتاج معالجة */
//         cairo_set_source_surface(cr, bg_surface, 0, 0);
//         cairo_paint(cr);
//     } else {
//         cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
//         cairo_paint(cr);
//     }
//     return FALSE;
// }

// static void on_change_wallpaper_clicked(GtkWidget *item, gpointer window) {
//     GtkWidget *dialog;
//     dialog = gtk_file_chooser_dialog_new("Select Wallpaper", GTK_WINDOW(window),
//                                          GTK_FILE_CHOOSER_ACTION_OPEN,
//                                          "_Cancel", GTK_RESPONSE_CANCEL,
//                                          "_Set", GTK_RESPONSE_ACCEPT, NULL);

//     GtkFileFilter *filter = gtk_file_filter_new();
//     gtk_file_filter_set_name(filter, "Images");
//     gtk_file_filter_add_mime_type(filter, "image/png");
//     gtk_file_filter_add_mime_type(filter, "image/jpeg");
//     gtk_file_filter_add_mime_type(filter, "image/webp");
//     gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

//     if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
//         char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
//         /* نمرر النافذة لكي نستطيع إنشاء السطح المتوافق معها */
//         load_wallpaper(filename, GTK_WIDGET(window));
//         g_free(filename);
//     }
//     gtk_widget_destroy(dialog);
// }

// static void on_quit_clicked(GtkWidget *item, gpointer data) {
//     gtk_main_quit();
// }

// static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
//     if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
//         GtkWidget *menu = gtk_menu_new();
//         GtkWidget *change = gtk_menu_item_new_with_label("Change Wallpaper...");
//         GtkWidget *quit = gtk_menu_item_new_with_label("Quit Desktop");
        
//         g_signal_connect(change, "activate", G_CALLBACK(on_change_wallpaper_clicked), data);
//         g_signal_connect(quit, "activate", G_CALLBACK(on_quit_clicked), NULL);
        
//         gtk_menu_shell_append(GTK_MENU_SHELL(menu), change);
//         gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
//         gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit);
        
//         gtk_widget_show_all(menu);
//         gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
//         return TRUE;
//     }
//     return FALSE;
// }

// /* نحتاج تحميل الخلفية الأولية بعد ظهور النافذة لضمان وجود GdkWindow */
// static void on_realize(GtkWidget *widget, gpointer data) {
//     const char *default_bg = "/home/x/Pictures/anime.jpg"; 
//     if (access(default_bg, F_OK) != -1) {
//         load_wallpaper(default_bg, widget);
//     }
// }

// int main(int argc, char *argv[]) {
//     gtk_init(&argc, &argv);

//     GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
//     gtk_window_set_title(GTK_WINDOW(window), "Venom Optimized Desktop");
//     gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DESKTOP);

//     GdkDisplay *display = gdk_display_get_default();
//     GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
//     GdkRectangle geo;
//     gdk_monitor_get_geometry(monitor, &geo);
//     screen_w = geo.width;
//     screen_h = geo.height;

//     gtk_window_set_default_size(GTK_WINDOW(window), screen_w, screen_h);
//     gtk_window_move(GTK_WINDOW(window), 0, 0);

//     /* استخدام realize signal لتحميل الخلفية بعد إنشاء موارد X11 */
//     g_signal_connect(window, "realize", G_CALLBACK(on_realize), NULL);

//     GtkWidget *overlay = gtk_overlay_new();
//     gtk_container_add(GTK_CONTAINER(window), overlay);

//     drawing_area = gtk_drawing_area_new();
//     gtk_widget_set_hexpand(drawing_area, TRUE);
//     gtk_widget_set_vexpand(drawing_area, TRUE);
//     g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw), NULL);
//     gtk_container_add(GTK_CONTAINER(overlay), drawing_area);

//     GtkWidget *icon_layer = gtk_event_box_new();
//     gtk_widget_set_app_paintable(icon_layer, FALSE);
//     gtk_event_box_set_visible_window(GTK_EVENT_BOX(icon_layer), FALSE);
//     g_signal_connect(icon_layer, "button-press-event", G_CALLBACK(on_button_press), window);
    
//     gtk_overlay_add_overlay(GTK_OVERLAY(overlay), icon_layer);

//     g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
//     gtk_widget_show_all(window);
//     gtk_main();

//     if (bg_surface) cairo_surface_destroy(bg_surface);
//     return 0;
// }






/*
 * venom-setbg.c
 * A lightweight wallpaper setter using Xlib & Imlib2.
 * RAM Usage after running: 0MB (Process terminates).
 *
 * Compile:
 * gcc -o venom-setbg venom-setbg.c -lX11 -lImlib2
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <Imlib2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <image_path>\n", argv[0]);
        return 1;
    }

    char *image_path = argv[1];
    Display *disp = XOpenDisplay(NULL);
    
    if (!disp) {
        fprintf(stderr, "Cannot open X display!\n");
        return 1;
    }

    int scr = DefaultScreen(disp);
    Window root = RootWindow(disp, scr);
    
    /* إعداد Imlib2 للعمل مع شاشتنا */
    imlib_context_set_display(disp);
    imlib_context_set_visual(DefaultVisual(disp, scr));
    imlib_context_set_colormap(DefaultColormap(disp, scr));

    /* تحميل الصورة */
    Imlib_Image image = imlib_load_image(image_path);
    if (!image) {
        fprintf(stderr, "Failed to load image: %s\n", image_path);
        XCloseDisplay(disp);
        return 1;
    }

    imlib_context_set_image(image);

    /* الحصول على أبعاد الشاشة */
    int width = DisplayWidth(disp, scr);
    int height = DisplayHeight(disp, scr);

    /* إنشاء Pixmap (مساحة في ذاكرة X Server) */
    Pixmap pixmap = XCreatePixmap(disp, root, width, height, DefaultDepth(disp, scr));

    /* رسم الصورة على الـ Pixmap مع تغيير الحجم لملء الشاشة */
    imlib_context_set_drawable(pixmap);
    imlib_render_image_on_drawable_at_size(0, 0, width, height);

    /* تعيين الـ Pixmap كخلفية للـ Root Window */
    XSetWindowBackgroundPixmap(disp, root, pixmap);
    XClearWindow(disp, root);

    /* --- الخطوات السحرية للتوافق مع الشفافية --- */
    /* هذه الخصائص تخبر البرامج الأخرى (مثل التيرمينال الشفاف) أن هناك خلفية */
    
    Atom prop_root = XInternAtom(disp, "_XROOTPMAP_ID", False);
    Atom prop_eset = XInternAtom(disp, "ESETROOT_PMAP_ID", False);

    /* تعيين الخصائص */
    XChangeProperty(disp, root, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pixmap, 1);
    XChangeProperty(disp, root, prop_eset, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pixmap, 1);

    /* تنظيف وإغلاق */
    /* ملاحظة: الـ Pixmap يبقى في X Server حتى نغيره، لكن برنامجنا ينتهي */
    imlib_free_image();
    XSetCloseDownMode(disp, RetainPermanent); /* مهم: أخبر X أن يحتفظ بالمصادر بعد خروجنا */
    XCloseDisplay(disp);

    return 0;
}