#include "keyboard.h"
#include "venom_power.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

// ═══════════════════════════════════════════════════════════════════════════
// ⌨️ إضاءة الكيبورد
// ═══════════════════════════════════════════════════════════════════════════

#define KBD_BACKLIGHT_PATH "/sys/class/leds"
#define KBD_BACKLIGHT_SUFFIX "kbd_backlight"

static gint original_kbd_brightness = -1;

static gchar* find_kbd_backlight_device(void) {
    DIR *dir = opendir(KBD_BACKLIGHT_PATH);
    if (!dir) return NULL;
    
    struct dirent *entry;
    gchar *device = NULL;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, KBD_BACKLIGHT_SUFFIX)) {
            device = g_strdup(entry->d_name);
            break;
        }
    }
    closedir(dir);
    return device;
}

static gint read_kbd_value(const gchar *file) {
    gchar *device = find_kbd_backlight_device();
    if (!device) return -1;
    
    gchar *path = g_strdup_printf("%s/%s/%s", KBD_BACKLIGHT_PATH, device, file);
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

gboolean keyboard_is_supported(void) {
    gchar *device = find_kbd_backlight_device();
    if (device) {
        g_free(device);
        return TRUE;
    }
    return FALSE;
}

gint keyboard_get_brightness(void) {
    return read_kbd_value("brightness");
}

gint keyboard_get_max_brightness(void) {
    return read_kbd_value("max_brightness");
}

gboolean keyboard_set_brightness(gint level) {
    gchar *device = find_kbd_backlight_device();
    if (!device) return FALSE;
    
    gchar *path = g_strdup_printf("%s/%s/brightness", KBD_BACKLIGHT_PATH, device);
    g_free(device);
    
    gint max = keyboard_get_max_brightness();
    if (max <= 0) {
        g_free(path);
        return FALSE;
    }
    
    // تحديد المستوى بين 0 والحد الأقصى
    if (level < 0) level = 0;
    if (level > max) level = max;
    
    gchar *value_str = g_strdup_printf("%d", level);
    gboolean success = g_file_set_contents(path, value_str, -1, NULL);
    
    if (success) {
        printf("⌨️ Keyboard brightness set to %d\n", level);
    }
    
    g_free(value_str);
    g_free(path);
    return success;
}

void keyboard_auto_off(gboolean off) {
    if (!keyboard_is_supported()) return;
    
    if (off) {
        // حفظ السطوع الحالي ثم إطفاء
        if (original_kbd_brightness < 0) {
            original_kbd_brightness = keyboard_get_brightness();
        }
        keyboard_set_brightness(0);
        printf("⌨️ Keyboard backlight off (saved: %d)\n", original_kbd_brightness);
    } else {
        // استعادة السطوع السابق
        if (original_kbd_brightness >= 0) {
            keyboard_set_brightness(original_kbd_brightness);
            printf("⌨️ Keyboard backlight restored to %d\n", original_kbd_brightness);
            original_kbd_brightness = -1;
        }
    }
}
