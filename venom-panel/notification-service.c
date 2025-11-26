/*
 * notification-service.c
 * Integration of working D-Bus notification server
 * Based on tested venom-notification-server.c logic
 */

#include "notification-service.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

/* =====================================================================
 * D-Bus Introspection XML
 * ===================================================================== */
static const gchar introspection_xml[] =
  "<node>"
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
  "    <method name='GetCapabilities'>"
  "      <arg type='as' name='caps' direction='out'/>"
  "    </method>"
  "    <method name='GetServerInformation'>"
  "      <arg type='s' name='name' direction='out'/>"
  "      <arg type='s' name='vendor' direction='out'/>"
  "      <arg type='s' name='version' direction='out'/>"
  "      <arg type='s' name='spec_version' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static guint32 next_notification_id = 1;
static guint name_owner_id = 0;
static NotificationCallback _callback = NULL;
static gpointer _callback_data = NULL;

/* =====================================================================
 * Notification UI Display
 * ===================================================================== */

static gboolean auto_close_notification(gpointer window)
{
    if (GTK_IS_WIDGET(window)) {
        gtk_widget_destroy(GTK_WIDGET(window));
    }
    return FALSE;
}

static void show_notification_ui(const char *summary, const char *body, const char *icon_name)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 80);
    gtk_container_set_border_width(GTK_CONTAINER(window), 0);
    
    /* Position: top-right corner */
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle workarea;
    gdk_monitor_get_workarea(monitor, &workarea);
    
    int x = workarea.x + workarea.width - 320;
    int y = workarea.y + 40;
    gtk_window_move(GTK_WINDOW(window), x, y);

    /* Layout */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 16);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    /* Icon */
    if (!icon_name || strlen(icon_name) == 0)
        icon_name = "dialog-information-symbolic";
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_valign(icon, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), icon, FALSE, FALSE, 0);

    /* Text box */
    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(main_box), text_box, TRUE, TRUE, 0);

    /* Summary (bold) */
    GtkWidget *lbl_summary = gtk_label_new(summary);
    gtk_widget_set_halign(lbl_summary, GTK_ALIGN_START);
    char *markup = g_strdup_printf("<b>%s</b>", summary);
    gtk_label_set_markup(GTK_LABEL(lbl_summary), markup);
    g_free(markup);
    gtk_box_pack_start(GTK_BOX(text_box), lbl_summary, FALSE, FALSE, 0);

    /* Body */
    GtkWidget *lbl_body = gtk_label_new(body);
    gtk_widget_set_halign(lbl_body, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(lbl_body), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(lbl_body), 30);
    gtk_box_pack_start(GTK_BOX(text_box), lbl_body, FALSE, FALSE, 0);

    /* CSS Styling */
    GtkStyleContext *ctx = gtk_widget_get_style_context(window);
    gtk_style_context_add_class(ctx, "venom-notification");

    /* Show and auto-close */
    gtk_widget_show_all(window);
    g_timeout_add(5000, auto_close_notification, window);
}

/* =====================================================================
 * D-Bus Method Handlers
 * ===================================================================== */

static void handle_method_call(GDBusConnection *conn,
                               const gchar *sender,
                               const gchar *obj_path,
                               const gchar *intf_name,
                               const gchar *method_name,
                               GVariant *params,
                               GDBusMethodInvocation *invoc,
                               gpointer user_data)
{
    (void)sender;
    (void)obj_path;
    (void)intf_name;
    (void)user_data;

    if (g_strcmp0(method_name, "Notify") == 0) {
        const gchar *app, *icon, *sum, *body;
        guint32 rep_id;
        GVariant *acts, *hints;
        gint32 timeout;

        g_variant_get(params, "(susssasa{sv}i)", &app, &rep_id, &icon, &sum, &body, &acts, &hints, &timeout);

        if (!sum) sum = "Notification";
        if (!body) body = "";

        /* Create notification struct */
        VenomNotification notif = {
            .id = next_notification_id,
            .app_name = g_strdup(app ? app : "System"),
            .summary = g_strdup(sum),
            .body = g_strdup(body),
            .app_icon = g_strdup(icon ? icon : ""),
            .timestamp = time(NULL)
        };

        /* Call registered callback */
        if (_callback) {
            _callback(&notif, _callback_data);
        }

        /* Display notification popup */
        show_notification_ui(sum, body, icon);

        guint32 id = next_notification_id++;
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(u)", id));
        
        g_free(notif.app_name);
        g_free(notif.summary);
        g_free(notif.body);
        g_free(notif.app_icon);
        g_variant_unref(acts);
        g_variant_unref(hints);
    }
    else if (g_strcmp0(method_name, "GetCapabilities") == 0) {
        GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("as"));
        g_variant_builder_add(b, "s", "body");
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(as)", b));
        g_variant_builder_unref(b);
    }
    else if (g_strcmp0(method_name, "GetServerInformation") == 0) {
        g_dbus_method_invocation_return_value(invoc,
            g_variant_new("(ssss)", "Venom Notification Server", "VenomDE", "1.0", "1.2"));
    }
}

static const GDBusInterfaceVTable interface_vtable = {
    handle_method_call,
    NULL,
    NULL
};

static void on_bus_acquired(GDBusConnection *conn,
                            const gchar *name,
                            gpointer user_data)
{
    (void)user_data;

    GError *error = NULL;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(introspection_xml, &error);

    if (!info) {
        g_warning("Failed to parse introspection XML: %s", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return;
    }

    g_dbus_connection_register_object(conn,
                                      "/org/freedesktop/Notifications",
                                      info->interfaces[0],
                                      &interface_vtable,
                                      NULL,
                                      NULL,
                                      &error);

    if (error) {
        g_warning("Failed to register notification object: %s", error->message);
        g_error_free(error);
    }

    g_dbus_node_info_unref(info);
    g_print("[NotificationService] D-Bus service registered successfully\n");
}

static void on_name_lost(GDBusConnection *conn,
                         const gchar *name,
                         gpointer user_data)
{
    (void)conn;
    (void)user_data;

    g_warning("[NotificationService] Lost D-Bus name: %s", name);
}

/* =====================================================================
 * Public API
 * ===================================================================== */

void notification_service_init(NotificationCallback callback, gpointer user_data)
{
    _callback = callback;
    _callback_data = user_data;

    name_owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                    "org.freedesktop.Notifications",
                                    G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                    on_bus_acquired,
                                    NULL,
                                    on_name_lost,
                                    NULL,
                                    NULL);

    g_print("[NotificationService] Initializing...\n");
}

void notification_service_cleanup(void)
{
    if (name_owner_id > 0) {
        g_bus_unown_name(name_owner_id);
        name_owner_id = 0;
    }

    g_print("[NotificationService] Cleaned up\n");
}
