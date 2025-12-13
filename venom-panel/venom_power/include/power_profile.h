#ifndef POWER_PROFILE_H
#define POWER_PROFILE_H

#include <gio/gio.h>
#include <glib.h>

#define PPD_DBUS_NAME "net.hadess.PowerProfiles"
#define PPD_DBUS_PATH "/net/hadess/PowerProfiles"
#define PPD_DBUS_INTERFACE "net.hadess.PowerProfiles"

typedef enum {
    POWER_PROFILE_POWER_SAVER,
    POWER_PROFILE_BALANCED,
    POWER_PROFILE_PERFORMANCE,
    POWER_PROFILE_UNKNOWN
} PowerProfileType;

gboolean power_profile_init(void);
void power_profile_cleanup(void);

const gchar* power_profile_get_active(void);
gboolean power_profile_set_active(const gchar *profile);
GVariant* power_profile_get_list(void);
const gchar* power_profile_get_inhibited_reason(void);
gboolean power_profile_is_available(void);

PowerProfileType power_profile_string_to_type(const gchar *profile);
const gchar* power_profile_type_to_string(PowerProfileType type);

#endif
