/*
 * brightness-manager.h
 *
 * Manages screen brightness using multiple backends:
 * 1. D-Bus (GNOME Settings Daemon / UPower)
 * 2. brightnessctl (External tool)
 * 3. xrandr (Software fallback)
 * 4. sysfs (Direct hardware access)
 */

#ifndef BRIGHTNESS_MANAGER_H
#define BRIGHTNESS_MANAGER_H

#include <glib.h>

/* Initialize the brightness manager */
void brightness_manager_init(void);

/* Get current brightness percentage (0-100) */
int brightness_manager_get_brightness(void);

/* Set brightness percentage (0-100) */
void brightness_manager_set_brightness(int percent);

/* Cleanup resources */
void brightness_manager_cleanup(void);

#endif /* BRIGHTNESS_MANAGER_H */
