/*
 * notification-client.c
 *
 * D-Bus client for venom_notify daemon.
 * Connects to org.venom.NotificationHistory interface.
 */

#include <gio/gio.h>
#include "notification-client.h"

#define DBUS_NAME "org.freedesktop.Notifications"
#define DBUS_PATH "/org/freedesktop/Notifications"
#define DBUS_IFACE_HISTORY "org.venom.NotificationHistory"

static GDBusProxy *_history_proxy = NULL;
static gboolean _initialized = FALSE;
static guint _signal_subscription = 0;
static guint _dnd_signal_subscription = 0;

/* Callbacks */
static NotificationHistoryCallback _history_callback = NULL;
static gpointer _history_callback_data = NULL;
static DoNotDisturbCallback _dnd_callback = NULL;
static gpointer _dnd_callback_data = NULL;

/* =====================================================================
 * Memory Management
 * ===================================================================== */

void notification_item_free(NotificationItem *item) {
    if (item) {
        g_free(item->app_name);
        g_free(item->icon_path);
        g_free(item->summary);
        g_free(item->body);
        g_free(item);
    }
}

void notification_item_list_free(GList *list) {
    g_list_free_full(list, (GDestroyNotify)notification_item_free);
}

/* =====================================================================
 * Signal Handlers
 * ===================================================================== */

static void on_history_signal(GDBusConnection *connection,
                              const gchar *sender_name,
                              const gchar *object_path,
                              const gchar *interface_name,
                              const gchar *signal_name,
                              GVariant *parameters,
                              gpointer user_data) {
    (void)connection;
    (void)sender_name;
    (void)object_path;
    (void)interface_name;
    (void)parameters;
    (void)user_data;
    
    if (g_strcmp0(signal_name, "HistoryUpdated") == 0) {
        if (_history_callback) {
            _history_callback(_history_callback_data);
        }
    } else if (g_strcmp0(signal_name, "DoNotDisturbChanged") == 0) {
        gboolean enabled = FALSE;
        g_variant_get(parameters, "(b)", &enabled);
        if (_dnd_callback) {
            _dnd_callback(enabled, _dnd_callback_data);
        }
    }
}

/* =====================================================================
 * Initialization
 * ===================================================================== */

void notification_client_init(void) {
    if (_initialized) return;
    
    GError *error = NULL;
    
    g_print("[NotificationClient] Connecting to %s...\n", DBUS_NAME);
    
    /* Create proxy for history interface */
    _history_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        DBUS_NAME,
        DBUS_PATH,
        DBUS_IFACE_HISTORY,
        NULL, &error);
    
    if (error) {
        g_print("[NotificationClient] ❌ Failed to connect: %s\n", error->message);
        g_clear_error(&error);
    } else if (_history_proxy) {
        g_print("[NotificationClient] ✅ Connected to venom_notify\n");
        
        /* Subscribe to signals */
        GDBusConnection *conn = g_dbus_proxy_get_connection(_history_proxy);
        _signal_subscription = g_dbus_connection_signal_subscribe(
            conn,
            DBUS_NAME,
            DBUS_IFACE_HISTORY,
            NULL,  /* All signals */
            DBUS_PATH,
            NULL,
            G_DBUS_SIGNAL_FLAGS_NONE,
            on_history_signal,
            NULL,
            NULL);
    }
    
    _initialized = TRUE;
}

void notification_client_cleanup(void) {
    if (_signal_subscription > 0 && _history_proxy) {
        GDBusConnection *conn = g_dbus_proxy_get_connection(_history_proxy);
        g_dbus_connection_signal_unsubscribe(conn, _signal_subscription);
        _signal_subscription = 0;
    }
    
    g_clear_object(&_history_proxy);
    _initialized = FALSE;
}

gboolean notification_client_is_available(void) {
    return _history_proxy != NULL;
}

/* =====================================================================
 * Get History
 * ===================================================================== */

GList* notification_client_get_history(void) {
    if (!_history_proxy) return NULL;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _history_proxy, "GetHistory", NULL,
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[NotificationClient] GetHistory failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    /* Result is (a(ussss)) - array of (id, app_name, icon, summary, body) */
    GList *items = NULL;
    GVariantIter *iter;
    g_variant_get(result, "(a(ussss))", &iter);
    
    guint32 id;
    const gchar *app_name, *icon, *summary, *body;
    
    while (g_variant_iter_next(iter, "(u&s&s&s&s)", &id, &app_name, &icon, &summary, &body)) {
        NotificationItem *item = g_new0(NotificationItem, 1);
        item->id = id;
        item->app_name = g_strdup(app_name);
        item->icon_path = g_strdup(icon);
        item->summary = g_strdup(summary);
        item->body = g_strdup(body);
        
        items = g_list_append(items, item);
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    
    g_print("[NotificationClient] Got %d notifications\n", g_list_length(items));
    return items;
}

/* =====================================================================
 * Actions
 * ===================================================================== */

void notification_client_clear_history(void) {
    if (!_history_proxy) return;
    
    GError *error = NULL;
    g_dbus_proxy_call_sync(
        _history_proxy, "ClearHistory", NULL,
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[NotificationClient] ClearHistory failed: %s", error->message);
        g_error_free(error);
    } else {
        g_print("[NotificationClient] History cleared\n");
    }
}

void notification_client_remove(guint32 id) {
    if (!_history_proxy) return;
    
    GError *error = NULL;
    g_dbus_proxy_call_sync(
        _history_proxy, "RemoveNotification",
        g_variant_new("(u)", id),
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[NotificationClient] RemoveNotification failed: %s", error->message);
        g_error_free(error);
    }
}

/* =====================================================================
 * Do Not Disturb
 * ===================================================================== */

gboolean notification_client_get_dnd(void) {
    if (!_history_proxy) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _history_proxy, "GetDoNotDisturb", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_warning("[NotificationClient] GetDoNotDisturb failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean enabled = FALSE;
    g_variant_get(result, "(b)", &enabled);
    g_variant_unref(result);
    return enabled;
}

void notification_client_set_dnd(gboolean enabled) {
    if (!_history_proxy) return;
    
    GError *error = NULL;
    g_dbus_proxy_call_sync(
        _history_proxy, "SetDoNotDisturb",
        g_variant_new("(b)", enabled),
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_warning("[NotificationClient] SetDoNotDisturb failed: %s", error->message);
        g_error_free(error);
    } else {
        g_print("[NotificationClient] DND set to: %d\n", enabled);
    }
}

/* =====================================================================
 * Callbacks
 * ===================================================================== */

void notification_client_on_history_update(NotificationHistoryCallback callback, gpointer user_data) {
    _history_callback = callback;
    _history_callback_data = user_data;
}

void notification_client_on_dnd_change(DoNotDisturbCallback callback, gpointer user_data) {
    _dnd_callback = callback;
    _dnd_callback_data = user_data;
}
