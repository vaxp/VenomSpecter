#include "launcher_window.h"
#include "search_bar.h"
#include "app_grid.h"
#include "../core/desktop_reader.h"
#include "../core/icon_loader.h"

#include <gdk/gdk.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Private struct
 * ------------------------------------------------------------------------- */

struct _VenomLauncherWindow {
    GtkApplicationWindow  parent_instance;

    GPtrArray    *apps;
    GtkWidget    *search_bar;
    GtkWidget    *app_grid;
    GtkWidget    *root_overlay;
};

G_DEFINE_TYPE (VenomLauncherWindow, venom_launcher_window,
               GTK_TYPE_APPLICATION_WINDOW)

/* -------------------------------------------------------------------------
 * CSS loader
 * ------------------------------------------------------------------------- */

static void
load_css (void)
{
    GtkCssProvider *provider = gtk_css_provider_new ();

    /* Try installed path first, then development path */
    const char *paths[] = {
        PKGDATADIR "/style/launcher.css",
        "data/style/launcher.css",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        GError *err = NULL;
        if (gtk_css_provider_load_from_path (provider, paths[i], &err)) {
            gtk_style_context_add_provider_for_screen (
                gdk_screen_get_default (),
                GTK_STYLE_PROVIDER (provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            break;
        }
        g_error_free (err);
    }

    g_object_unref (provider);
}

/* -------------------------------------------------------------------------
 * Transparency / RGBA visual
 * ------------------------------------------------------------------------- */

static void
setup_transparency (GtkWidget *widget)
{
    GdkScreen  *screen = gtk_widget_get_screen (widget);
    GdkVisual  *visual = gdk_screen_get_rgba_visual (screen);

    if (visual) {
        gtk_widget_set_visual (widget, visual);
        gtk_widget_set_app_paintable (widget, TRUE);
    }
}

/* -------------------------------------------------------------------------
 * Background draw (semi-transparent dark overlay)
 * ------------------------------------------------------------------------- */

static gboolean
on_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    (void) user_data;
    int w = gtk_widget_get_allocated_width  (widget);
    int h = gtk_widget_get_allocated_height (widget);

    cairo_set_source_rgba (cr, 0.05, 0.05, 0.10, 0.82);
    cairo_rectangle (cr, 0, 0, w, h);
    cairo_fill (cr);

    return FALSE;
}

/* -------------------------------------------------------------------------
 * Keyboard handling
 * ------------------------------------------------------------------------- */

static gboolean
on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    (void) user_data;
    VenomLauncherWindow *self = VENOM_LAUNCHER_WINDOW (widget);

    if (event->keyval == GDK_KEY_Escape) {
        venom_search_bar_clear (VENOM_SEARCH_BAR (self->search_bar));
        gtk_widget_hide (widget);
        return TRUE;
    }

    /* Forward typing to search bar */
    if (!gtk_widget_has_focus (self->search_bar)) {
        gtk_widget_grab_focus (self->search_bar);
    }

    return FALSE;
}

/* -------------------------------------------------------------------------
 * Search changed handler
 * ------------------------------------------------------------------------- */

static void
on_search_changed (GtkWidget *search, gpointer data)
{
    VenomLauncherWindow *self = VENOM_LAUNCHER_WINDOW (data);
    const char *query = venom_search_bar_get_text (VENOM_SEARCH_BAR (search));
    venom_app_grid_set_filter (VENOM_APP_GRID (self->app_grid), query);
}



/* -------------------------------------------------------------------------
 * GObject class init
 * ------------------------------------------------------------------------- */

static void
venom_launcher_window_finalize (GObject *obj)
{
    VenomLauncherWindow *self = VENOM_LAUNCHER_WINDOW (obj);
    if (self->apps)
        g_ptr_array_unref (self->apps);
    icon_loader_destroy ();
    G_OBJECT_CLASS (venom_launcher_window_parent_class)->finalize (obj);
}

static void
venom_launcher_window_class_init (VenomLauncherWindowClass *klass)
{
    GObjectClass *obj_class = G_OBJECT_CLASS (klass);
    obj_class->finalize = venom_launcher_window_finalize;
}

static void
venom_launcher_window_init (VenomLauncherWindow *self)
{
    GtkWindow *win = GTK_WINDOW (self);

    gtk_window_set_decorated         (win, FALSE);
    gtk_window_set_skip_taskbar_hint (win, TRUE);
    gtk_window_set_skip_pager_hint   (win, TRUE);
    gtk_window_set_type_hint         (win, GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);

    gtk_widget_set_name (GTK_WIDGET (self), "venom-launcher");
    gtk_style_context_add_class (
        gtk_widget_get_style_context (GTK_WIDGET (self)), "launcher-window");

    /* Setup RGBA transparency */
    setup_transparency (GTK_WIDGET (self));

    /* Load CSS */
    load_css ();

    /* ── Root overlay ───────────────────────────────────────────────── */
    self->root_overlay = gtk_overlay_new ();
    gtk_container_add (GTK_CONTAINER (self), self->root_overlay);

    /* ── Main content box ──────────────────────────────────────────── */
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_set_margin_top    (vbox, 40);
    gtk_widget_set_margin_bottom (vbox, 32);
    gtk_widget_set_margin_start  (vbox, 48);
    gtk_widget_set_margin_end    (vbox, 48);
    gtk_container_add (GTK_CONTAINER (self->root_overlay), vbox);

    /* ── Search Bar ────────────────────────────────────────────────── */
    self->search_bar = venom_search_bar_new ();
    gtk_widget_set_halign (self->search_bar, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (vbox), self->search_bar, FALSE, FALSE, 0);

    /* ── Load apps ─────────────────────────────────────────────────── */
    self->apps = desktop_reader_load_apps ();

    /* ── App Grid ──────────────────────────────────────────────────── */
    self->app_grid = venom_app_grid_new (self->apps);
    gtk_box_pack_start (GTK_BOX (vbox), self->app_grid, TRUE, TRUE, 0);

    /* Connect signals */
    g_signal_connect (self->search_bar, "search-changed-debounced",
                      G_CALLBACK (on_search_changed), self);

    g_signal_connect (GTK_WIDGET (self), "key-press-event",
                      G_CALLBACK (on_key_press), NULL);

    g_signal_connect (GTK_WIDGET (self), "draw",
                      G_CALLBACK (on_draw), NULL);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

GtkWidget *
venom_launcher_window_new (GtkApplication *app)
{
    VenomLauncherWindow *win = g_object_new (
        VENOM_TYPE_LAUNCHER_WINDOW,
        "application", app,
        NULL);

    /* Fullscreen after realize */
    gtk_window_fullscreen (GTK_WINDOW (win));

    return GTK_WIDGET (win);
}

void
venom_launcher_window_show_launcher (VenomLauncherWindow *win)
{
    g_return_if_fail (VENOM_IS_LAUNCHER_WINDOW (win));

    venom_search_bar_clear (VENOM_SEARCH_BAR (win->search_bar));
    venom_app_grid_set_filter (VENOM_APP_GRID (win->app_grid), NULL);

    gtk_widget_show_all (GTK_WIDGET (win));
    gtk_window_present  (GTK_WINDOW (win));

    venom_search_bar_grab_focus (VENOM_SEARCH_BAR (win->search_bar));
}
