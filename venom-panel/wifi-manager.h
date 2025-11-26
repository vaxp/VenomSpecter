#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <glib.h>

/* Simple WiFi manager - just status for now */
void wifi_manager_init(void);
void wifi_manager_cleanup(void);
gboolean wifi_manager_is_wireless_enabled(void);
void wifi_manager_set_wireless_enabled(gboolean enabled);

#endif /* WIFI_MANAGER_H */
