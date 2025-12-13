#ifndef DBUS_SERVICE_H
#define DBUS_SERVICE_H

#include <gio/gio.h>

// ═══════════════════════════════════════════════════════════════════════════
// 📡 خدمة D-Bus
// ═══════════════════════════════════════════════════════════════════════════

/**
 * إرسال إشارة D-Bus
 * @param signal_name اسم الإشارة
 * @param params المعاملات
 */
void dbus_emit_signal(const gchar *signal_name, GVariant *params);

/**
 * إرسال إشعار للمستخدم
 * @param title العنوان
 * @param body النص
 * @param urgency الأولوية (low, normal, critical)
 */
void dbus_send_notification(const gchar *title, const gchar *body, const gchar *urgency);

/**
 * الحصول على XML تعريف الواجهة
 * @return نص XML
 */
const gchar* dbus_get_introspection_xml(void);

/**
 * معالج طلبات D-Bus
 */
void dbus_handle_method_call(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *method_name, GVariant *parameters,
                             GDBusMethodInvocation *invocation, gpointer user_data);

/**
 * الجدول الافتراضي للواجهة
 */
extern const GDBusInterfaceVTable dbus_interface_vtable;

#endif // DBUS_SERVICE_H
