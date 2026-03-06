#include <gio/gio.h>
#include <stdio.h>

static void on_prop_changed(GDBusConnection *connection,
                            const gchar *sender_name,
                            const gchar *object_path,
                            const gchar *interface_name,
                            const gchar *signal_name,
                            GVariant *parameters,
                            gpointer user_data) {
    (void)connection; (void)object_path; (void)interface_name; (void)signal_name; (void)user_data;
    printf("Signal from %s\n", sender_name);
    gchar *str = g_variant_print(parameters, TRUE);
    printf("Params: %s\n", str);
    g_free(str);
}

int main() {
    GError *err = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (!bus) return 1;
    
    g_dbus_connection_signal_subscribe(bus,
                                       NULL,
                                       "org.freedesktop.DBus.Properties",
                                       "PropertiesChanged",
                                       NULL,
                                       "org.mpris.MediaPlayer2.Player",
                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                       on_prop_changed,
                                       NULL, NULL);
                                       
    printf("Listening for 10 seconds. Play/Pause Chromium now...\n");
    
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add_seconds(10, (GSourceFunc)g_main_loop_quit, loop);
    g_main_loop_run(loop);
    
    return 0;
}
