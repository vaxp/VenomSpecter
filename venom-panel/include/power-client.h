#ifndef POWER_CLIENT_H
#define POWER_CLIENT_H

#include <gtk/gtk.h>

/* Initialization & Cleanup */
void power_client_init(void);
void power_client_cleanup(void);

/* Power Actions */
void power_client_shutdown(void);
void power_client_reboot(void);
void power_client_suspend(void);
void power_client_lock_screen(void);

/* Power Profiles */
typedef void (*PowerProfileCallback)(const gchar *profile, gpointer user_data);

void power_get_active_profile(void); /* Async - triggers callback */
void power_set_active_profile(const gchar *profile);
void power_client_on_profile_changed(PowerProfileCallback cb, gpointer user_data);

/* Battery */
typedef void (*PowerBatteryCallback)(gdouble percentage, gboolean charging, gint64 time_to_empty, gpointer user_data);
void power_get_battery_info(void); /* Async - triggers callback */
void power_client_on_battery_changed(PowerBatteryCallback cb, gpointer user_data);

#endif
