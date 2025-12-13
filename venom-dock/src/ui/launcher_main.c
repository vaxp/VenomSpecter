#include <gtk/gtk.h>
#include "launcher.h"

int main(int argc, char *argv[]) {
    /* Initialize GTK */
    gtk_init(&argc, &argv);
    
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

    /* Start Launcher in Standalone Mode */
    launcher_start_standalone();

    /* Main Loop */
    gtk_main();

    return 0;
}
