#include <gtk/gtk.h>
#include <gio/gio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <time.h>

// --- إعدادات التصميم ---
#define NOTIFY_WIDTH 340
#define NOTIFY_HEIGHT 90
#define MARGIN_X 20
#define MARGIN_Y 50
#define SPACING 10
#define ICON_SIZE 48
#define PADDING 12
#define DEFAULT_TIMEOUT 5000
#define MAX_HISTORY 50 

// --- هيكل بيانات السجل (History Item) ---
typedef struct {
    guint32 id;
    char *app_name;
    char *icon_path;
    char *summary;
    char *body;
    gint64 timestamp;
} NotificationItem;

// --- هيكل النافذة النشطة ---
typedef struct {
    guint32 id;
    Window win;
    cairo_surface_t *surface;
    cairo_t *cr;
    int y_pos;
    guint timeout_source;
} VenomNotification;

Display *display;
int screen;
GList *active_notifications = NULL; // النوافذ الظاهرة حالياً
GList *history_list = NULL;         // سجل الإشعارات (للكونترول سنتر)
guint32 id_counter = 1;
gboolean do_not_disturb = FALSE;    // وضع عدم الإزعاج
GHashTable *app_notification_map = NULL; // خريطة لتتبع آخر إشعار لكل تطبيق (للاستبدال)
GDBusConnection *dbus_connection = NULL; // حفظ الاتصال لإرسال الإشارات

// --- تعريف الواجهات (Standard + Venom History) ---
static const gchar introspection_xml[] =
  "<node>"
  // 1. الواجهة القياسية (للتطبيقات)
  "  <interface name='org.freedesktop.Notifications'>"
  "    <method name='Notify'>"
  "      <arg type='s' name='app_name' direction='in'/>"
  "      <arg type='u' name='replaces_id' direction='in'/>"
  "      <arg type='s' name='app_icon' direction='in'/>"
  "      <arg type='s' name='summary' direction='in'/>"
  "      <arg type='s' name='body' direction='in'/>"
  "      <arg type='as' name='actions' direction='in'/>"
  "      <arg type='a{sv}' name='hints' direction='in'/>"
  "      <arg type='i' name='expire_timeout' direction='in'/>"
  "      <arg type='u' name='id' direction='out'/>"
  "    </method>"
  "    <method name='GetCapabilities'><arg type='as' name='caps' direction='out'/></method>"
  "    <method name='GetServerInformation'><arg type='s' name='name' direction='out'/><arg type='s' name='vendor' direction='out'/><arg type='s' name='ver' direction='out'/><arg type='s' name='spec' direction='out'/></method>"
  "    <method name='CloseNotification'><arg type='u' name='id' direction='in'/></method>"
  "  </interface>"
  
  // 2. واجهة فينوم الخاصة (للكونترول سنتر)
  "  <interface name='org.venom.NotificationHistory'>"
  "    <method name='GetHistory'>"
  "      <arg type='a(ussss)' name='notifications' direction='out'/>"
  "    </method>"
  "    <method name='ClearHistory'/>"
  "    <method name='RemoveNotification'><arg type='u' name='id' direction='in'/></method>"
  "    <method name='SetDoNotDisturb'><arg type='b' name='enabled' direction='in'/></method>"
  "    <method name='GetDoNotDisturb'><arg type='b' name='enabled' direction='out'/></method>"
  "    <signal name='HistoryUpdated'/>"
  "    <signal name='DoNotDisturbChanged'><arg type='b' name='enabled'/></signal>"
  "  </interface>"
  "</node>";

// --- Forward Declarations ---
void add_to_history(guint32 id, const char *app, const char *icon, const char *summary, const char *body);
void emit_history_updated_signal(GDBusConnection *connection);

// --- إدارة السجل (History Logic) ---

// استبدال إشعار في السجل بدلاً من إضافة واحد جديد
void replace_in_history(const char *app, const char *icon, const char *summary, const char *body) {
    // البحث عن آخر إشعار من نفس التطبيق
    for (GList *l = history_list; l != NULL; l = l->next) {
        NotificationItem *item = (NotificationItem *)l->data;
        if (g_strcmp0(item->app_name, app) == 0) {
            // تحديث الإشعار بدلاً من حذفه
            g_free(item->summary);
            g_free(item->body);
            item->summary = g_strdup(summary);
            item->body = g_strdup(body);
            item->timestamp = time(NULL);
            return; // خرج بعد التحديث
        }
    }
    
    // إذا لم نجد واحداً، أضف واحداً جديداً
    add_to_history(0, app, icon, summary, body);
}

void add_to_history(guint32 id, const char *app, const char *icon, const char *summary, const char *body) {
    NotificationItem *item = g_new0(NotificationItem, 1);
    item->id = id;
    item->app_name = g_strdup(app);
    item->icon_path = g_strdup(icon);
    item->summary = g_strdup(summary);
    item->body = g_strdup(body);
    item->timestamp = time(NULL);

    // إضافة لأول القائمة (الأحدث أولاً)
    history_list = g_list_prepend(history_list, item);

    // تنظيف القديم إذا تجاوزنا الحد المسموح
    if (g_list_length(history_list) > MAX_HISTORY) {
        GList *last = g_list_last(history_list);
        NotificationItem *old_item = (NotificationItem *)last->data;
        g_free(old_item->app_name);
        g_free(old_item->icon_path);
        g_free(old_item->summary);
        g_free(old_item->body);
        g_free(old_item);
        history_list = g_list_delete_link(history_list, last);
    }
}

void clear_history() {
    GList *l;
    for (l = history_list; l != NULL; l = l->next) {
        NotificationItem *item = (NotificationItem *)l->data;
        g_free(item->app_name);
        g_free(item->icon_path);
        g_free(item->summary);
        g_free(item->body);
        g_free(item);
    }
    g_list_free(history_list);
    history_list = NULL;
    
    // تنظيف الخريطة أيضاً
    if (app_notification_map) {
        g_hash_table_destroy(app_notification_map);
        app_notification_map = NULL;
    }
}

// --- دوال الرسم (كما هي مع تحسين Flush) ---
void draw_notification_content(VenomNotification *n, const char *title, const char *body, const char *icon_path) {
    cairo_t *cr = n->cr;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // خلفية مشطوفة
    double x = 0, y = 0, w = NOTIFY_WIDTH, h = NOTIFY_HEIGHT, cut = 15.0;
    cairo_new_path(cr);
    cairo_move_to(cr, x + cut, y);
    cairo_line_to(cr, x + w, y);
    cairo_line_to(cr, x + w, y + h);
    cairo_line_to(cr, x, y + h);
    cairo_line_to(cr, x, y + cut);
    cairo_close_path(cr);

    cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, 0.95);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.8);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    // أيقونة
    if (icon_path && strlen(icon_path) > 0) {
        GError *err = NULL;
        GdkPixbuf *pixbuf = NULL;
        if (g_file_test(icon_path, G_FILE_TEST_EXISTS)) {
            pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_path, ICON_SIZE, ICON_SIZE, TRUE, &err);
        } else {
            pixbuf = gdk_pixbuf_new_from_file_at_scale("/usr/share/icons/Adwaita/48x48/legacy/preferences-system-details.png", ICON_SIZE, ICON_SIZE, TRUE, NULL);
        }
        if (pixbuf) {
            gdk_cairo_set_source_pixbuf(cr, pixbuf, PADDING, (NOTIFY_HEIGHT - ICON_SIZE) / 2);
            cairo_paint(cr);
            g_object_unref(pixbuf);
        }
    }

    // نص
    int text_x = PADDING + ICON_SIZE + PADDING;
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_width(layout, (NOTIFY_WIDTH - text_x - PADDING) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    char *safe_title = g_markup_escape_text(title ? title : "Notification", -1);
    char *safe_body = g_markup_escape_text(body ? body : "", -1);
    char *markup = g_strdup_printf("<span font_desc='Sans Bold 10' foreground='#00FFFF'>%s</span>\n<span font_desc='Sans 9' foreground='#EEEEEE'>%s</span>", safe_title, safe_body);
    
    pango_layout_set_markup(layout, markup, -1);
    int text_h;
    pango_layout_get_pixel_size(layout, NULL, &text_h);
    cairo_move_to(cr, text_x, (NOTIFY_HEIGHT - text_h) / 2.0);
    pango_cairo_show_layout(cr, layout);
    
    g_free(safe_title); g_free(safe_body); g_free(markup);
    g_object_unref(layout);
    
    cairo_surface_flush(n->surface);
}

// --- البحث عن إشعار نشط بواسطة التطبيق ---
VenomNotification* find_notification_by_app(const char *app_name) {
    (void)app_name; // لا نحتاجه الآن لأننا نستخدم الخريطة
    return NULL;
}

// --- إدارة النوافذ (مع إصلاح الـ XMap قبل الرسم) ---
void close_notification(guint32 id);

gboolean on_timeout(gpointer data) {
    close_notification(GPOINTER_TO_UINT(data));
    return FALSE; 
}

void close_notification(guint32 id) {
    GList *l;
    for (l = active_notifications; l != NULL; l = l->next) {
        VenomNotification *n = (VenomNotification *)l->data;
        if (n->id == id) {
            if (n->timeout_source > 0) g_source_remove(n->timeout_source);
            cairo_destroy(n->cr);
            cairo_surface_destroy(n->surface);
            XUnmapWindow(display, n->win);
            XDestroyWindow(display, n->win);
            XFlush(display);
            active_notifications = g_list_delete_link(active_notifications, l);
            
            // حذف من الخريطة
            GHashTableIter iter;
            gpointer key, value;
            if (app_notification_map) {
                g_hash_table_iter_init(&iter, app_notification_map);
                while (g_hash_table_iter_next(&iter, &key, &value)) {
                    if (GPOINTER_TO_UINT(value) == id) {
                        g_hash_table_iter_remove(&iter);
                        g_free(key);
                        break;
                    }
                }
            }
            
            g_free(n);
            
            // Shift Up
            int i = 0;
            for (GList *curr = active_notifications; curr != NULL; curr = curr->next) {
                VenomNotification *vn = (VenomNotification *)curr->data;
                int new_y = MARGIN_Y + (i * (NOTIFY_HEIGHT + SPACING));
                XMoveWindow(display, vn->win, DisplayWidth(display, screen) - NOTIFY_WIDTH - MARGIN_X, new_y);
                i++;
            }
            XFlush(display);
            break;
        }
    }
}

void create_new_notification(const char *app_name, const char *summary, const char *body, const char *icon, gint timeout) {
    // تهيئة الخريطة إذا لم تكن موجودة (نحتاجها في كلا الوضعين)
    if (!app_notification_map) {
        app_notification_map = g_hash_table_new(g_str_hash, g_str_equal);
    }
    
    // إذا كان وضع عدم الإزعاج مفعل، أضف للسجل بشكل صامت فقط (مع الاستبدال)
    if (do_not_disturb) {
        // ابحث عن إشعار سابق من نفس التطبيق في السجل
        gboolean found = FALSE;
        for (GList *l = history_list; l != NULL; l = l->next) {
            NotificationItem *item = (NotificationItem *)l->data;
            if (g_strcmp0(item->app_name, app_name) == 0) {
                // استبدل الإشعار بدلاً من إضافة واحد جديد
                g_free(item->summary);
                g_free(item->body);
                item->summary = g_strdup(summary);
                item->body = g_strdup(body);
                item->timestamp = time(NULL);
                emit_history_updated_signal(NULL);
                found = TRUE;
                break;
            }
        }
        // إذا لم نجد واحداً، أضف واحداً جديداً
        if (!found) {
            add_to_history(id_counter++, app_name, icon, summary, body);
            emit_history_updated_signal(NULL);
        }
        return;
    }
    
    // البحث عن إشعار سابق من نفس التطبيق
    gpointer existing_id_ptr = g_hash_table_lookup(app_notification_map, app_name);
    guint32 existing_id = 0;
    VenomNotification *existing_notification = NULL;
    
    if (existing_id_ptr) {
        existing_id = GPOINTER_TO_UINT(existing_id_ptr);
        // ابحث عن الإشعار الموجود لتحديثه
        for (GList *l = active_notifications; l != NULL; l = l->next) {
            VenomNotification *n = (VenomNotification *)l->data;
            if (n->id == existing_id) {
                existing_notification = n;
                break;
            }
        }
    }
    
    // إذا وجدنا إشعار سابق، حدثه بدلاً من إنشاء واحد جديد
    if (existing_notification) {
        // استبدل في السجل
        replace_in_history(app_name, icon, summary, body);
        // حدث محتوى الإشعار على الشاشة
        draw_notification_content(existing_notification, summary, body, icon);
        cairo_surface_flush(existing_notification->surface);
        XFlush(display);
        // أعد ضبط المؤقت
        if (existing_notification->timeout_source > 0) {
            g_source_remove(existing_notification->timeout_source);
        }
        if (timeout <= 0) timeout = DEFAULT_TIMEOUT;
        existing_notification->timeout_source = g_timeout_add(timeout, on_timeout, GUINT_TO_POINTER(existing_id));
        // أرسل إشارة التحديث
        emit_history_updated_signal(NULL);
        return;
    }
    
    int count = g_list_length(active_notifications);
    int y_pos = MARGIN_Y + (count * (NOTIFY_HEIGHT + SPACING));
    int screen_w = DisplayWidth(display, screen);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo)) 
        XMatchVisualInfo(display, screen, DefaultDepth(display, screen), TrueColor, &vinfo);

    XSetWindowAttributes attrs;
    attrs.colormap = XCreateColormap(display, DefaultRootWindow(display), vinfo.visual, AllocNone);
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    attrs.override_redirect = True; 

    Window win = XCreateWindow(display, DefaultRootWindow(display),
                               screen_w - NOTIFY_WIDTH - MARGIN_X, y_pos,
                               NOTIFY_WIDTH, NOTIFY_HEIGHT, 0, vinfo.depth, InputOutput,
                               vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect, &attrs);

    Atom state_above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    XChangeProperty(display, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char *)&state_above, 1);

    // ⚡ إظهار النافذة فوراً قبل الرسم (حل مشكلة عدم الرسم)
    XMapWindow(display, win);

    cairo_surface_t *surf = cairo_xlib_surface_create(display, win, vinfo.visual, NOTIFY_WIDTH, NOTIFY_HEIGHT);
    cairo_t *cr = cairo_create(surf);

    VenomNotification *n = g_new0(VenomNotification, 1);
    n->id = id_counter++;
    n->win = win;
    n->surface = surf;
    n->cr = cr;
    n->y_pos = y_pos;

    // 📝 الأرشفة هنا: إضافة الإشعار للسجل
    add_to_history(n->id, app_name, icon, summary, body);

    // حفظ ID الإشعار لهذا التطبيق (لاستبدال الإشعارات المستقبلية)
    g_hash_table_insert(app_notification_map, g_strdup(app_name), GUINT_TO_POINTER(n->id));

    draw_notification_content(n, summary, body, icon);
    cairo_surface_flush(surf);
    XFlush(display);

    active_notifications = g_list_append(active_notifications, n);

    if (timeout <= 0) timeout = DEFAULT_TIMEOUT;
    n->timeout_source = g_timeout_add(timeout, on_timeout, GUINT_TO_POINTER(n->id));
}

// --- معالجة D-Bus ---

// دالة مساعدة لإرسال إشارة "تحديث السجل"
void emit_history_updated_signal(GDBusConnection *connection) {
    // استخدم المتغير العام إذا لم نحصل على connection
    if (!connection) connection = dbus_connection;
    if (!connection) return;
    
    g_dbus_connection_emit_signal(connection,
                                  NULL,
                                  "/org/freedesktop/Notifications",
                                  "org.venom.NotificationHistory",
                                  "HistoryUpdated",
                                  NULL, NULL);
}

static void handle_method_call(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path, const gchar *interface_name,
                               const gchar *method_name, GVariant *parameters,
                               GDBusMethodInvocation *invocation, gpointer user_data) {
    
    // 1. استقبال الإشعارات (Standard)
    if (g_strcmp0(method_name, "Notify") == 0) {
        gchar *app_name, *app_icon, *summary, *body;
        guint32 replaces_id;
        GVariant *actions = NULL;
        GVariant *hints = NULL;
        gint32 expire_timeout;

        g_variant_get(parameters, "(susss@as@a{sv}i)", 
                      &app_name, &replaces_id, &app_icon, 
                      &summary, &body, &actions, &hints, &expire_timeout);

        create_new_notification(app_name, summary, body, app_icon, expire_timeout);
        
        // 🚨 إخبار الكونترول سنتر أن هناك إشعار جديد
        emit_history_updated_signal(connection);

        g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", id_counter - 1));

        g_free(app_name); g_free(app_icon); g_free(summary); g_free(body);
        if (actions) g_variant_unref(actions);
        if (hints) g_variant_unref(hints);
    }
    // 2. طلب السجل (للكونترول سنتر)
    else if (g_strcmp0(method_name, "GetHistory") == 0) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a(ussss)"));
        
        for (GList *l = history_list; l != NULL; l = l->next) {
            NotificationItem *item = (NotificationItem *)l->data;
            g_variant_builder_add(builder, "(ussss)", 
                                  item->id, 
                                  item->app_name ? item->app_name : "",
                                  item->icon_path ? item->icon_path : "",
                                  item->summary ? item->summary : "",
                                  item->body ? item->body : "");
        }
        
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(ussss))", builder));
        g_variant_builder_unref(builder);
    }
    // 3. مسح السجل
    else if (g_strcmp0(method_name, "ClearHistory") == 0) {
        clear_history();
        emit_history_updated_signal(connection); // تحديث الواجهة لتصبح فارغة
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    // 4. تفعيل/تعطيل وضع عدم الإزعاج
    else if (g_strcmp0(method_name, "SetDoNotDisturb") == 0) {
        gboolean enabled;
        g_variant_get(parameters, "(b)", &enabled);
        do_not_disturb = enabled;
        
        // إرسال إشارة التغيير
        g_dbus_connection_emit_signal(connection,
                                      NULL,
                                      "/org/freedesktop/Notifications",
                                      "org.venom.NotificationHistory",
                                      "DoNotDisturbChanged",
                                      g_variant_new("(b)", do_not_disturb), NULL);
        
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    // 5. الحصول على حالة وضع عدم الإزعاج
    else if (g_strcmp0(method_name, "GetDoNotDisturb") == 0) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", do_not_disturb));
    }
    // 4. بقية الدوال القياسية
    else if (g_strcmp0(method_name, "GetCapabilities") == 0) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        g_variant_builder_add(builder, "s", "body");
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(as)", builder));
        g_variant_builder_unref(builder);
    }
    else if (g_strcmp0(method_name, "GetServerInformation") == 0) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(ssss)", "Venom", "Vaxp", "1.0", "1.2"));
    }
    else if (g_strcmp0(method_name, "CloseNotification") == 0) {
        guint32 id;
        g_variant_get(parameters, "(u)", &id);
        close_notification(id);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

static const GDBusInterfaceVTable interface_vtable = { handle_method_call, NULL, NULL };

static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    dbus_connection = connection; // احفظ الاتصال للاستخدام لاحقاً
    
    GDBusNodeInfo *node_info = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    
    // تسجيل الواجهة القياسية (للإشعارات)
    g_dbus_connection_register_object(connection, "/org/freedesktop/Notifications",
                                      node_info->interfaces[0], &interface_vtable,
                                      NULL, NULL, NULL);
    
    // تسجيل واجهة فينوم (للسجل والتحديثات)
    g_dbus_connection_register_object(connection, "/org/freedesktop/Notifications",
                                      node_info->interfaces[1], &interface_vtable,
                                      NULL, NULL, NULL);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    display = XOpenDisplay(NULL);
    if (!display) return 1;
    screen = DefaultScreen(display);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_bus_own_name(G_BUS_TYPE_SESSION, "org.freedesktop.Notifications",
                   G_BUS_NAME_OWNER_FLAGS_REPLACE, on_bus_acquired, NULL, NULL, NULL, NULL);
    g_main_loop_run(loop);
    return 0;
}