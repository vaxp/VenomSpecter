#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>

// ═══════════════════════════════════════════════════════════════════════════
// ⚙️ ملف الإعدادات
// ═══════════════════════════════════════════════════════════════════════════

#define CONFIG_PATH "/etc/venom/power.conf"
#define CONFIG_USER_PATH "~/.config/venom/power.conf"

typedef struct {
    // إعدادات الخمول (بالثواني)
    guint dim_timeout_ac;
    guint dim_timeout_battery;
    guint blank_timeout_ac;
    guint blank_timeout_battery;
    guint suspend_timeout_battery;
    
    // مستويات البطارية
    guint battery_low_level;
    guint battery_critical_level;
    guint battery_danger_level;
    
    // إجراءات
    gchar *lid_action_ac;         // ignore, lock, suspend, hibernate
    gchar *lid_action_battery;
    gchar *power_button_action;   // interactive, suspend, hibernate, poweroff
    gchar *critical_action;       // hibernate, poweroff
    
    // سطوع
    gint brightness_ac;
    gint brightness_battery;
    gboolean dim_on_idle;
    gint dim_level_percent;
    
    // إضاءة الكيبورد
    gboolean kbd_backlight_auto_off;
    guint kbd_backlight_timeout;
    
    // متنوع
    gboolean lock_on_suspend;
    gboolean lock_on_lid;
    gchar *lock_screen_cmd;
} PowerConfig;

/**
 * الحصول على الإعدادات الحالية
 * @return مؤشر للإعدادات
 */
PowerConfig* config_get(void);

/**
 * تحميل الإعدادات من الملف
 * @return TRUE في حالة النجاح
 */
gboolean config_load(void);

/**
 * حفظ الإعدادات إلى الملف
 * @return TRUE في حالة النجاح
 */
gboolean config_save(void);

/**
 * إعادة الإعدادات إلى القيم الافتراضية
 */
void config_reset_defaults(void);

/**
 * تحرير موارد الإعدادات
 */
void config_free(void);

#endif // CONFIG_H
