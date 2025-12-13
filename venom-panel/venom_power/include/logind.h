#ifndef LOGIND_H
#define LOGIND_H

#include <glib.h>
#include <gio/gio.h>

// ═══════════════════════════════════════════════════════════════════════════
// 💻 التكامل مع Logind
// ═══════════════════════════════════════════════════════════════════════════

/**
 * استدعاء طريقة من logind
 * @param method اسم الطريقة (PowerOff, Reboot, Suspend, Hibernate)
 * @param params المعاملات
 * @return TRUE في حالة النجاح
 */
gboolean logind_call(const gchar *method, GVariant *params);

/**
 * تسجيل الخروج من الجلسة
 * @return TRUE في حالة النجاح
 */
gboolean logind_logout(void);

/**
 * قفل الشاشة
 * @return TRUE في حالة النجاح
 */
gboolean logind_lock_screen(void);

/**
 * إعداد مراقبة إشارات logind
 */
void logind_setup_monitoring(void);

/**
 * معالج إشارة الاستعداد للنوم
 */
void logind_on_prepare_for_sleep(GDBusConnection *connection,
                                 const gchar *sender_name,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *signal_name,
                                 GVariant *parameters,
                                 gpointer user_data);

/**
 * معالج تغير خصائص logind (الغطاء)
 */
void logind_on_properties_changed(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data);

#endif // LOGIND_H
