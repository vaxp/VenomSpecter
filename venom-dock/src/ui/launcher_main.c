#include <gtk/gtk.h>
#include <gio/gio.h>
#include "launcher.h"

static void
on_method_call(GDBusConnection *connection,
               const gchar *sender,
               const gchar *object_path,
               const gchar *interface_name,
               const gchar *method_name,
               GVariant *parameters,
               GDBusMethodInvocation *invocation,
               gpointer user_data)
{
    (void)connection; (void)sender; (void)object_path; (void)interface_name; (void)parameters; (void)user_data;

    if (g_strcmp0(method_name, "Toggle") == 0) {
        launcher_toggle_visibility();
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

static const GDBusInterfaceVTable interface_vtable = {
    on_method_call,
    NULL,
    NULL
};

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.venomspecter.Launcher'>"
    "    <method name='Toggle'/>"
    "  </interface>"
    "</node>";

static void
on_bus_acquired(GDBusConnection *connection,
                const gchar *name,
                gpointer user_data)
{
    (void)name; (void)user_data;
    GError *error = NULL;
    GDBusNodeInfo *node_info = g_dbus_node_info_new_for_xml(introspection_xml, NULL);

    g_dbus_connection_register_object(connection,
                                      "/org/venomspecter/Launcher",
                                      node_info->interfaces[0],
                                      &interface_vtable,
                                      NULL,
                                      NULL,
                                      &error);

    if (error) {
        g_warning("Failed to register object: %s", error->message);
        g_error_free(error);
    }
    g_dbus_node_info_unref(node_info);
}

static void
on_name_acquired(GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
    (void)connection; (void)name; (void)user_data;
    /* We own the name, so we are the primary instance. Start GTK. */
    
    /* Load CSS */
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    
    gtk_style_context_add_provider_for_screen(screen,
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    GError *error = NULL;
    if (!gtk_css_provider_load_from_path(provider, "style.css", &error)) {
        g_warning("Failed to load CSS: %s", error->message);
        g_error_free(error);
    }
    g_object_unref(provider);

    /* Initialize Launcher Logic */
    launcher_start_standalone();
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    /* Try to call Toggle on existing instance first */
    GError *error = NULL;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!conn) {
        g_error("Failed to connect to bus: %s", error->message);
        return 1;
    }

    /* Check if name has owner by trying to call Toggle */
    GVariant *result = g_dbus_connection_call_sync(conn,
                                                   "org.venomspecter.Launcher",
                                                   "/org/venomspecter/Launcher",
                                                   "org.venomspecter.Launcher",
                                                   "Toggle",
                                                   NULL,
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);

    if (result) {
        /* Success! Launcher toggled. Exit. */
        g_variant_unref(result);
        return 0;
    }
    
    if (error) {
        /* Failed to find service, so we start it. */
        g_clear_error(&error); 
    }

    /* We failed to call Toggle, so assume we are the first instance. Become the Server. */
    guint owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                    "org.venomspecter.Launcher",
                                    G_BUS_NAME_OWNER_FLAGS_NONE,
                                    on_bus_acquired,
                                    on_name_acquired,
                                    NULL,
                                    NULL,
                                    NULL);

    gtk_main();
    
    g_bus_unown_name(owner_id);
    return 0;
}
