#include "inhibitor.h"
#include "venom_power.h"
#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🚫 نظام المنع
// ═══════════════════════════════════════════════════════════════════════════

gboolean inhibitor_has_type(InhibitType type) {
    for (GList *l = power_state.inhibitors; l != NULL; l = l->next) {
        Inhibitor *inh = (Inhibitor*)l->data;
        if (inh->type & type) return TRUE;
    }
    return FALSE;
}

guint inhibitor_add(const gchar *what, const gchar *who, const gchar *why) {
    Inhibitor *inh = g_new0(Inhibitor, 1);
    inh->id = ++power_state.next_inhibitor_id;
    inh->app_name = g_strdup(who);
    inh->reason = g_strdup(why);
    
    // تحويل "what" إلى نوع
    if (g_str_has_prefix(what, "idle"))
        inh->type = INHIBIT_IDLE;
    else if (g_str_has_prefix(what, "suspend") || g_str_has_prefix(what, "sleep"))
        inh->type = INHIBIT_SUSPEND;
    else if (g_str_has_prefix(what, "shutdown"))
        inh->type = INHIBIT_SHUTDOWN;
    else
        inh->type = INHIBIT_IDLE | INHIBIT_SUSPEND;
    
    power_state.inhibitors = g_list_append(power_state.inhibitors, inh);
    printf("🚫 Inhibitor added: [%u] %s - %s\n", inh->id, who, why);
    return inh->id;
}

void inhibitor_remove(guint cookie) {
    for (GList *l = power_state.inhibitors; l != NULL; l = l->next) {
        Inhibitor *inh = (Inhibitor*)l->data;
        if (inh->id == cookie) {
            printf("✅ Inhibitor removed: [%u] %s\n", inh->id, inh->app_name);
            g_free(inh->app_name);
            g_free(inh->reason);
            power_state.inhibitors = g_list_delete_link(power_state.inhibitors, l);
            g_free(inh);
            return;
        }
    }
}

void inhibitor_list_to_variant(GVariantBuilder *builder) {
    for (GList *l = power_state.inhibitors; l != NULL; l = l->next) {
        Inhibitor *inh = (Inhibitor*)l->data;
        g_variant_builder_add(builder, "(uss)", inh->id, inh->app_name, inh->reason);
    }
}
