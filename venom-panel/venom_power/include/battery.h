#ifndef BATTERY_H
#define BATTERY_H

#include <glib.h>
#include <gio/gio.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🔋 مراقبة البطارية
// ═══════════════════════════════════════════════════════════════════════════

/**
 * الحصول على معلومات البطارية
 * @param percentage نسبة الشحن (مخرج)
 * @param charging هل يتم الشحن (مخرج)
 * @param time_to_empty الوقت حتى الفراغ بالثواني (مخرج)
 */
void battery_get_info(gdouble *percentage, gboolean *charging, gint64 *time_to_empty);

/**
 * فحص مستوى البطارية وإرسال التحذيرات
 * @param percentage نسبة الشحن
 * @param charging هل يتم الشحن
 */
void battery_check_level(gdouble percentage, gboolean charging);

/**
 * إعداد مراقبة UPower
 */
void battery_setup_monitoring(void);

/**
 * معالج تغير خصائص البطارية
 */
void battery_on_properties_changed(GDBusConnection *connection,
                                   const gchar *sender_name,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *signal_name,
                                   GVariant *parameters,
                                   gpointer user_data);

/**
 * معالج تغير مصدر الطاقة
 */
void battery_on_power_source_changed(GDBusConnection *connection,
                                     const gchar *sender_name,
                                     const gchar *object_path,
                                     const gchar *interface_name,
                                     const gchar *signal_name,
                                     GVariant *parameters,
                                     gpointer user_data);

#endif // BATTERY_H
