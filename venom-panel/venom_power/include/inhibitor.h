#ifndef INHIBITOR_H
#define INHIBITOR_H

#include <glib.h>
#include "venom_power.h"

// ═══════════════════════════════════════════════════════════════════════════
// 🚫 نظام المنع
// ═══════════════════════════════════════════════════════════════════════════

/**
 * التحقق من وجود مانع من نوع معين
 * @param type نوع المانع
 * @return TRUE إذا وجد مانع
 */
gboolean inhibitor_has_type(InhibitType type);

/**
 * إضافة مانع جديد
 * @param what ماذا يُمنع (idle, suspend, shutdown)
 * @param who اسم التطبيق
 * @param why سبب المنع
 * @return معرف المانع (cookie)
 */
guint inhibitor_add(const gchar *what, const gchar *who, const gchar *why);

/**
 * إزالة مانع
 * @param cookie معرف المانع
 */
void inhibitor_remove(guint cookie);

/**
 * الحصول على قائمة المانعات لـ D-Bus
 * @param builder بناء الـ GVariant
 */
void inhibitor_list_to_variant(GVariantBuilder *builder);

#endif // INHIBITOR_H
