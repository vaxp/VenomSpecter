#include "backlight.h"
#include "venom_power.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

// ═══════════════════════════════════════════════════════════════════════════
// 💡 التحكم بالسطوع
// ═══════════════════════════════════════════════════════════════════════════

static gchar* find_backlight_device(void) {
    DIR *dir = opendir(BACKLIGHT_PATH);
    if (!dir) return NULL;
    
    struct dirent *entry;
    gchar *device = NULL;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            device = g_strdup(entry->d_name);
            break;
        }
    }
    closedir(dir);
    return device;
}

static gint read_backlight_value(const gchar *file) {
    gchar *device = find_backlight_device();
    if (!device) return -1;
    
    gchar *path = g_strdup_printf("%s/%s/%s", BACKLIGHT_PATH, device, file);
    g_free(device);
    
    gchar *contents = NULL;
    gint value = -1;
    
    if (g_file_get_contents(path, &contents, NULL, NULL)) {
        value = atoi(contents);
        g_free(contents);
    }
    g_free(path);
    return value;
}

gint backlight_get_brightness(void) {
    return read_backlight_value("brightness");
}

gint backlight_get_max_brightness(void) {
    return read_backlight_value("max_brightness");
}

gboolean backlight_set_brightness(gint level) {
    gchar *device = find_backlight_device();
    if (!device) return FALSE;
    
    gchar *path = g_strdup_printf("%s/%s/brightness", BACKLIGHT_PATH, device);
    g_free(device);
    
    gint max = backlight_get_max_brightness();
    if (max <= 0) {
        g_free(path);
        return FALSE;
    }
    
    // تحديد المستوى بين 0 والحد الأقصى
    if (level < 0) level = 0;
    if (level > max) level = max;
    
    gchar *value_str = g_strdup_printf("%d", level);
    gboolean success = g_file_set_contents(path, value_str, -1, NULL);
    
    g_free(value_str);
    g_free(path);
    return success;
}

void backlight_dim_screen(gboolean dim) {
    if (dim && !power_state.screen_dimmed) {
        power_state.original_brightness = backlight_get_brightness();
        gint dimmed = power_state.original_brightness * 30 / 100;
        if (dimmed < 1) dimmed = 1;
        backlight_set_brightness(dimmed);
        power_state.screen_dimmed = TRUE;
        printf("🔅 Screen dimmed to 30%%\n");
    } else if (!dim && power_state.screen_dimmed) {
        backlight_set_brightness(power_state.original_brightness);
        power_state.screen_dimmed = FALSE;
        printf("🔆 Screen brightness restored\n");
    }
}

void backlight_blank_screen(gboolean blank) {
    if (blank) {
        system("xset dpms force off 2>/dev/null");
        power_state.screen_blanked = TRUE;
        printf("🖥️ Screen blanked\n");
    } else {
        system("xset dpms force on 2>/dev/null");
        power_state.screen_blanked = FALSE;
        printf("🖥️ Screen unblanked\n");
    }
}
