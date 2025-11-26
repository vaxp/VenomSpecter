/*
 * brightness-manager.c
 *
 * Implementation of robust brightness control.
 */

#include "brightness-manager.h"
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* =====================================================================
 * GLOBALS & STATE
 * ===================================================================== */

typedef enum {
    METHOD_NONE,
    METHOD_DBUS_GSD,    /* GNOME Settings Daemon */
    METHOD_BRIGHTNESSCTL,
    METHOD_XRANDR,
    METHOD_SYSFS
} BrightnessMethod;

static BrightnessMethod _current_method = METHOD_NONE;
static GDBusProxy *_gsd_proxy = NULL;
static char *_xrandr_output_name = NULL;
static char *_sysfs_path = NULL;
static int _max_brightness_sysfs = 1;

/* =====================================================================
 * HELPER FUNCTIONS
 * ===================================================================== */

static gboolean check_command(const char *cmd) {
    char *path = g_find_program_in_path(cmd);
    if (path) {
        g_free(path);
        return TRUE;
    }
    return FALSE;
}

/* =====================================================================
 * SYSFS BACKEND
 * ===================================================================== */

static const char *backlight_paths[] = {
    "/sys/class/backlight/intel_backlight",
    "/sys/class/backlight/amdgpu_bl0",
    "/sys/class/backlight/acpi_video0",
    "/sys/class/backlight/nv_backlight",
    NULL
};

static gboolean init_sysfs(void) {
    for (int i = 0; backlight_paths[i]; i++) {
        char brightness_file[256];
        snprintf(brightness_file, sizeof(brightness_file), "%s/brightness", backlight_paths[i]);
        if (access(brightness_file, W_OK) == 0) { /* Check for WRITE access */
            _sysfs_path = g_strdup(backlight_paths[i]);
            
            /* Read max brightness */
            char max_file[256];
            snprintf(max_file, sizeof(max_file), "%s/max_brightness", _sysfs_path);
            FILE *f = fopen(max_file, "r");
            if (f) {
                if (fscanf(f, "%d", &_max_brightness_sysfs) != 1) _max_brightness_sysfs = 1;
                fclose(f);
            }
            return TRUE;
        }
    }
    return FALSE;
}

static int get_sysfs_brightness(void) {
    if (!_sysfs_path) return 50;
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/brightness", _sysfs_path);
    FILE *f = fopen(filepath, "r");
    int cur = 0;
    if (f) {
        if (fscanf(f, "%d", &cur) != 1) cur = 0;
        fclose(f);
    }
    return (cur * 100) / _max_brightness_sysfs;
}

static void set_sysfs_brightness(int percent) {
    if (!_sysfs_path) return;
    int val = (percent * _max_brightness_sysfs) / 100;
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/brightness", _sysfs_path);
    FILE *f = fopen(filepath, "w");
    if (f) {
        fprintf(f, "%d", val);
        fclose(f);
    }
}

/* =====================================================================
 * XRANDR BACKEND
 * ===================================================================== */

static char *get_xrandr_output(void) {
    /* Run xrandr | grep " connected" | head -1 | awk '{print $1}' */
    /* Simplified: just read output and parse */
    char *output = NULL;
    GError *error = NULL;
    char *stdout_str = NULL;
    
    if (g_spawn_command_line_sync("xrandr", &stdout_str, NULL, NULL, &error)) {
        char **lines = g_strsplit(stdout_str, "\n", -1);
        for (int i = 0; lines[i]; i++) {
            if (strstr(lines[i], " connected")) {
                char **parts = g_strsplit(lines[i], " ", -1);
                if (parts[0]) {
                    output = g_strdup(parts[0]);
                }
                g_strfreev(parts);
                break;
            }
        }
        g_strfreev(lines);
        g_free(stdout_str);
    } else {
        if (error) g_error_free(error);
    }
    return output;
}

static void set_xrandr_brightness(int percent) {
    if (!_xrandr_output_name) return;
    char cmd[256];
    /* xrandr takes 0.0 to 1.0 */
    float val = (float)percent / 100.0f;
    if (val < 0.1f) val = 0.1f; /* Prevent black screen */
    
    snprintf(cmd, sizeof(cmd), "xrandr --output %s --brightness %.2f", _xrandr_output_name, val);
    g_spawn_command_line_async(cmd, NULL);
}

/* =====================================================================
 * BRIGHTNESSCTL BACKEND
 * ===================================================================== */

static int get_brightnessctl_brightness(void) {
    /* brightnessctl g -> returns raw value */
    /* brightnessctl m -> returns max value */
    /* Easier: brightnessctl -m i -> returns info in csv format: device,class,current,max,percent% */
    
    char *stdout_str = NULL;
    if (g_spawn_command_line_sync("brightnessctl -m i", &stdout_str, NULL, NULL, NULL)) {
        /* Example: intel_backlight,backlight,400,800,50% */
        /* We just need to find the last comma and read the percentage */
        char *percent_ptr = strrchr(stdout_str, ',');
        if (percent_ptr) {
            int p = atoi(percent_ptr + 1);
            g_free(stdout_str);
            return p;
        }
        g_free(stdout_str);
    }
    return 50;
}

static void set_brightnessctl_brightness(int percent) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "brightnessctl s %d%%", percent);
    g_spawn_command_line_async(cmd, NULL);
}

static gboolean test_brightnessctl(void) {
    /* Try to set brightness to current value to check permissions */
    /* First get current value */
    char *stdout_str = NULL;
    int current_val = -1;
    
    if (g_spawn_command_line_sync("brightnessctl g", &stdout_str, NULL, NULL, NULL)) {
        current_val = atoi(stdout_str);
        g_free(stdout_str);
    }
    
    if (current_val < 0) return FALSE;

    /* Try to set it back to current value */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "brightnessctl s %d", current_val);
    
    int exit_status = 0;
    GError *error = NULL;
    if (g_spawn_command_line_sync(cmd, NULL, NULL, &exit_status, &error)) {
        if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0) {
            return TRUE;
        }
    }
    if (error) g_error_free(error);
    return FALSE;
}

/* =====================================================================
 * D-BUS BACKEND (GNOME Settings Daemon)
 * ===================================================================== */

static void init_dbus(void) {
    g_print("[Brightness] Initializing D-Bus...\n");
    GError *error = NULL;
    _gsd_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.gnome.SettingsDaemon.Power",
        "/org/gnome/SettingsDaemon/Power",
        "org.gnome.SettingsDaemon.Power.Screen",
        NULL,
        &error
    );

    if (error) {
        g_print("[Brightness] Failed to connect to GSD Power: %s\n", error->message);
        g_error_free(error);
        _gsd_proxy = NULL;
    } else {
        g_print("[Brightness] D-Bus proxy created.\n");
    }
}

static int get_dbus_brightness(void) {
    if (!_gsd_proxy) return 50;
    GVariant *ret = g_dbus_proxy_get_cached_property(_gsd_proxy, "Brightness");
    if (ret) {
        int val = 50;
        if (g_variant_is_of_type(ret, G_VARIANT_TYPE_INT32)) {
            val = g_variant_get_int32(ret);
        } else if (g_variant_is_of_type(ret, G_VARIANT_TYPE_UINT32)) {
            val = (int)g_variant_get_uint32(ret);
        } else {
            g_warning("[Brightness] Unexpected variant type: %s", g_variant_get_type_string(ret));
        }
        g_variant_unref(ret);
        return val;
    }
    return 50;
}

static void set_dbus_brightness(int percent) {
    if (!_gsd_proxy) return;
    
    /* Call SetBrightness(uint) */
    g_dbus_proxy_call(
        _gsd_proxy,
        "SetBrightness",
        g_variant_new("(u)", (guint32)percent),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        NULL,
        NULL
    );
}

/* =====================================================================
 * PUBLIC API
 * ===================================================================== */

void brightness_manager_init(void) {
    g_print("[Brightness] Manager Init...\n");
    
    /* Priority 1: D-Bus (GNOME) */
    init_dbus();
    if (_gsd_proxy) {
        /* Check if we can actually read a property to verify it works */
        g_print("[Brightness] Checking 'Brightness' property...\n");
        GVariant *test = g_dbus_proxy_get_cached_property(_gsd_proxy, "Brightness");
        if (test) {
            _current_method = METHOD_DBUS_GSD;
            g_print("Brightness Manager: Using D-Bus (GSD)\n");
            g_variant_unref(test);
            return;
        } else {
             g_print("[Brightness] 'Brightness' property not found or null.\n");
        }
    }

    /* Priority 2: brightnessctl (Check permissions first!) */
    g_print("[Brightness] Checking brightnessctl...\n");
    if (check_command("brightnessctl")) {
        if (test_brightnessctl()) {
            _current_method = METHOD_BRIGHTNESSCTL;
            g_print("Brightness Manager: Using brightnessctl\n");
            return;
        } else {
            g_print("Brightness Manager: brightnessctl found but lacks permissions. Skipping.\n");
        }
    }

    /* Priority 3: Sysfs (if writable) */
    g_print("[Brightness] Checking sysfs...\n");
    if (init_sysfs()) {
        _current_method = METHOD_SYSFS;
        g_print("Brightness Manager: Using sysfs (direct write)\n");
        return;
    }

    /* Priority 4: xrandr (fallback) */
    g_print("[Brightness] Checking xrandr...\n");
    if (check_command("xrandr")) {
        _xrandr_output_name = get_xrandr_output();
        if (_xrandr_output_name) {
            _current_method = METHOD_XRANDR;
            g_print("Brightness Manager: Using xrandr (output: %s)\n", _xrandr_output_name);
            return;
        }
    }

    g_warning("Brightness Manager: No suitable method found.\n");
    _current_method = METHOD_NONE;
}

int brightness_manager_get_brightness(void) {
    switch (_current_method) {
        case METHOD_DBUS_GSD: return get_dbus_brightness();
        case METHOD_BRIGHTNESSCTL: return get_brightnessctl_brightness();
        case METHOD_SYSFS: return get_sysfs_brightness();
        case METHOD_XRANDR: return 100; /* xrandr doesn't easily report current brightness */
        default: return 50;
    }
}

void brightness_manager_set_brightness(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    switch (_current_method) {
        case METHOD_DBUS_GSD: set_dbus_brightness(percent); break;
        case METHOD_BRIGHTNESSCTL: set_brightnessctl_brightness(percent); break;
        case METHOD_SYSFS: set_sysfs_brightness(percent); break;
        case METHOD_XRANDR: set_xrandr_brightness(percent); break;
        default: break;
    }
}

void brightness_manager_cleanup(void) {
    if (_gsd_proxy) g_object_unref(_gsd_proxy);
    if (_xrandr_output_name) g_free(_xrandr_output_name);
    if (_sysfs_path) g_free(_sysfs_path);
}
