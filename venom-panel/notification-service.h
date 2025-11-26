#ifndef NOTIFICATION_SERVICE_H
#define NOTIFICATION_SERVICE_H

#include <gio/gio.h>
#include <glib.h>
#include <time.h>

typedef struct {
    guint32 id;
    char *app_name;
    char *summary;
    char *body;
    char *app_icon;
    time_t timestamp;
} VenomNotification;

/* Callback function type for when a notification arrives */
typedef void (*NotificationCallback)(VenomNotification *notif, gpointer user_data);

/**
 * notification_service_init
 * Initialize and register the D-Bus notification service
 * Also registers a callback for when notifications arrive
 */
void notification_service_init(NotificationCallback callback, gpointer user_data);

/**
 * notification_service_cleanup
 * Unregister the notification service
 */
void notification_service_cleanup(void);

#endif
