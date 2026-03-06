#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string.h>
#include "venom-panel-plugin-api.h"

typedef struct {
    GtkWidget *box;
    GtkWidget *btn_prev;
    GtkWidget *btn_play;
    GtkWidget *btn_next;
    GtkWidget *lbl_title;
    
    GDBusConnection *bus;
    guint name_owner_id;
    char *active_player; /* e.g. "org.mpris.MediaPlayer2.spotify" */
    
    char *song_title;
    char *song_artist;
    gboolean is_playing;
    
    /* For marquee effect */
    guint scroll_timer_id;
    int scroll_offset;
    char scroll_text[256];
} MprisData;

static void update_ui(MprisData *data) {
    if (!data->active_player) {
        gtk_widget_hide(data->box);
        if (data->scroll_timer_id > 0) {
            g_source_remove(data->scroll_timer_id);
            data->scroll_timer_id = 0;
        }
        return;
    }
    
    gtk_widget_show_all(data->box);
    
    /* Update play/pause icon */
    GtkWidget *icon = gtk_bin_get_child(GTK_BIN(data->btn_play));
    if (data->is_playing) {
        gtk_image_set_from_icon_name(GTK_IMAGE(icon), "media-playback-pause-symbolic", GTK_ICON_SIZE_BUTTON);
    } else {
        gtk_image_set_from_icon_name(GTK_IMAGE(icon), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
    }
    
    /* Format label text */
    if (data->song_title && data->song_artist && strlen(data->song_artist) > 0) {
        snprintf(data->scroll_text, sizeof(data->scroll_text), "%s - %s    ", data->song_artist, data->song_title);
    } else if (data->song_title) {
        snprintf(data->scroll_text, sizeof(data->scroll_text), "%s    ", data->song_title);
    } else {
        snprintf(data->scroll_text, sizeof(data->scroll_text), "No Media    ");
    }
    
    /* Reset scroll */
    data->scroll_offset = 0;
}

static gboolean on_scroll_tick(gpointer user_data) {
    MprisData *data = (MprisData*)user_data;
    if (!GTK_IS_WIDGET(data->lbl_title)) return G_SOURCE_REMOVE;
    if (!data->active_player) return G_SOURCE_CONTINUE;
    
    size_t len = strlen(data->scroll_text);
    if (len == 0) return G_SOURCE_CONTINUE;
    
    /* Very basic character shifting for marquee */
    int display_len = 20; /* Max chars to show */
    if ((int)len <= display_len) {
        gtk_label_set_text(GTK_LABEL(data->lbl_title), data->scroll_text);
        return G_SOURCE_CONTINUE;
    }
    
    char display_str[64] = {0};
    int j = 0;
    for (int i = 0; i < display_len && j < 63; i++) {
        int idx = (data->scroll_offset + i) % len;
        
        /* Handle simple UTF8 boundaries - highly simplified for this example */
        if ((data->scroll_text[idx] & 0xC0) == 0x80) {
            /* We started in the middle of a UTF-8 char, skip ahead to a valid start */
            while ((data->scroll_text[idx] & 0xC0) == 0x80) {
                data->scroll_offset = (data->scroll_offset + 1) % len;
                idx = (data->scroll_offset + i) % len;
            }
        }
        display_str[j++] = data->scroll_text[idx];
    }
    display_str[j] = '\0';
    
    /* Ensure valid UTF-8 before setting to GTK to prevent crash */
    if (g_utf8_validate(display_str, -1, NULL)) {
        gtk_label_set_text(GTK_LABEL(data->lbl_title), display_str);
    }
    
    data->scroll_offset = (data->scroll_offset + 1) % len;
    return G_SOURCE_CONTINUE;
}

/* --- DBus Handlers --- */

static void get_mpris_properties(MprisData *data) {
    if (!data->active_player || !data->bus) return;
    
    GError *error = NULL;
    GVariant *res = g_dbus_connection_call_sync(data->bus,
                                                data->active_player,
                                                "/org/mpris/MediaPlayer2",
                                                "org.freedesktop.DBus.Properties",
                                                "GetAll",
                                                g_variant_new("(s)", "org.mpris.MediaPlayer2.Player"),
                                                G_VARIANT_TYPE("(a{sv})"),
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1, NULL, &error);
    if (error) {
        g_warning("MPRIS GetAll error: %s", error->message);
        g_error_free(error);
        return;
    }
    
    if (res) {
        GVariantIter *iter;
        g_variant_get(res, "(a{sv})", &iter);
        
        const gchar *key;
        GVariant *value;
        while (g_variant_iter_loop(iter, "{&sv}", &key, &value)) {
            if (g_strcmp0(key, "PlaybackStatus") == 0) {
                GVariant *v = g_variant_get_variant(value);
                if (v && g_variant_is_of_type(v, G_VARIANT_TYPE_STRING)) {
                    const gchar *status = g_variant_get_string(v, NULL);
                    data->is_playing = (g_strcmp0(status, "Playing") == 0);
                }
                if (v) g_variant_unref(v);
            } 
            else if (g_strcmp0(key, "Metadata") == 0) {
                GVariant *v_dict = g_variant_get_variant(value);
                if (v_dict && g_variant_is_of_type(v_dict, G_VARIANT_TYPE("a{sv}"))) {
                    GVariantIter *meta_iter;
                    g_variant_get(v_dict, "a{sv}", &meta_iter);
                    
                    const gchar *m_key;
                    GVariant *m_val;
                    while (g_variant_iter_loop(meta_iter, "{&sv}", &m_key, &m_val)) {
                        GVariant *real_val = g_variant_get_variant(m_val);
                        if (!real_val) continue;

                        if (g_strcmp0(m_key, "xesam:title") == 0) {
                            if (g_variant_is_of_type(real_val, G_VARIANT_TYPE_STRING)) {
                                g_free(data->song_title);
                                data->song_title = g_variant_dup_string(real_val, NULL);
                            }
                        }
                        else if (g_strcmp0(m_key, "xesam:artist") == 0) {
                            if (g_variant_is_of_type(real_val, G_VARIANT_TYPE_STRING_ARRAY)) {
                                GVariantIter *arr_iter;
                                g_variant_get(real_val, "as", &arr_iter);
                                char *first_artist = NULL;
                                g_variant_iter_next(arr_iter, "s", &first_artist);
                                
                                g_free(data->song_artist);
                                data->song_artist = first_artist;
                                
                                char *dummy;
                                while (g_variant_iter_next(arr_iter, "s", &dummy)) g_free(dummy);
                                g_variant_iter_free(arr_iter);
                            }
                        }
                        g_variant_unref(real_val);
                    }
                    g_variant_iter_free(meta_iter);
                }
                if (v_dict) g_variant_unref(v_dict);
            }
        }
        g_variant_iter_free(iter);
        g_variant_unref(res);
    }
    
    update_ui(data);
}

static void on_dbus_signal(GDBusConnection *connection,
                           const gchar *sender_name,
                           const gchar *object_path,
                           const gchar *interface_name,
                           const gchar *signal_name,
                           GVariant *parameters,
                           gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path; (void)interface_name; (void)signal_name; (void)parameters;
    MprisData *data = (MprisData*)user_data;
    
    /* DBus signals use unique names like :1.45. Since we only listen to
       PropertiesChanged on MediaPlayer2.Player, we can just rebuild state
       from our known active_player whenever this fires. */
    get_mpris_properties(data);
}

static void on_name_owner_changed(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path; (void)interface_name; (void)signal_name;
    MprisData *data = (MprisData*)user_data;
    
    const gchar *name, *old_owner, *new_owner;
    g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
    
    if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.")) {
        if (strlen(new_owner) > 0) {
            /* New player appeared, prefer it */
            g_free(data->active_player);
            data->active_player = g_strdup(name);
            get_mpris_properties(data);
            
            if (data->scroll_timer_id == 0) {
                data->scroll_timer_id = g_timeout_add(250, on_scroll_tick, data);
            }
        } 
        else if (strlen(old_owner) > 0 && g_strcmp0(name, data->active_player) == 0) {
            /* Our active player vanished */
            g_free(data->active_player);
            data->active_player = NULL;
            g_free(data->song_title); data->song_title = NULL;
            g_free(data->song_artist); data->song_artist = NULL;
            update_ui(data);
            
            /* TODO: check if other players exist and fallback to them */
        }
    }
}

/* Control actions */
static void send_mpris_command(MprisData *data, const char *method) {
    if (!data->active_player || !data->bus) return;
    
    g_dbus_connection_call(data->bus,
                           data->active_player,
                           "/org/mpris/MediaPlayer2",
                           "org.mpris.MediaPlayer2.Player",
                           method,
                           NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                           NULL, NULL, NULL);
}

static void on_prev_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; send_mpris_command((MprisData*)user_data, "Previous");
}
static void on_play_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; send_mpris_command((MprisData*)user_data, "PlayPause");
}
static void on_next_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; send_mpris_command((MprisData*)user_data, "Next");
}

/* Initialization */
static void find_initial_player(MprisData *data) {
    GError *error = NULL;
    GVariant *res = g_dbus_connection_call_sync(data->bus,
                                                "org.freedesktop.DBus",
                                                "/org/freedesktop/DBus",
                                                "org.freedesktop.DBus",
                                                "ListNames",
                                                NULL,
                                                G_VARIANT_TYPE("(as)"),
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1, NULL, &error);
    if (!error && res) {
        GVariantIter *iter;
        g_variant_get(res, "(as)", &iter);
        char *name;
        while (g_variant_iter_next(iter, "s", &name)) {
            if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.")) {
                data->active_player = g_strdup(name);
                g_free(name);
                break; /* Just grab the first one we see initially */
            }
            g_free(name);
        }
        g_variant_iter_free(iter);
        g_variant_unref(res);
        
        if (data->active_player) {
            get_mpris_properties(data);
            data->scroll_timer_id = g_timeout_add(250, on_scroll_tick, data);
        }
    } else if (error) {
        g_error_free(error);
    }
}

static void on_widget_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    MprisData *data = (MprisData*)user_data;
    if (data->scroll_timer_id > 0) g_source_remove(data->scroll_timer_id);
    if (data->bus && data->name_owner_id > 0) g_dbus_connection_signal_unsubscribe(data->bus, data->name_owner_id);
    /* Also unsubscribe properties changed, omitted for brevity */
    
    g_free(data->active_player);
    g_free(data->song_title);
    g_free(data->song_artist);
    
    if (data->bus) g_object_unref(data->bus);
    g_free(data);
}

static GtkWidget* create_mpris_widget(void) {
    MprisData *data = g_new0(MprisData, 1);
    
    data->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_margin_start(data->box, 4);
    gtk_widget_set_margin_end(data->box, 4);
    
    /* Buttons */
    data->btn_prev = gtk_button_new_from_icon_name("media-skip-backward-symbolic", GTK_ICON_SIZE_BUTTON);
    data->btn_play = gtk_button_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
    data->btn_next = gtk_button_new_from_icon_name("media-skip-forward-symbolic", GTK_ICON_SIZE_BUTTON);
    
    gtk_button_set_relief(GTK_BUTTON(data->btn_prev), GTK_RELIEF_NONE);
    gtk_button_set_relief(GTK_BUTTON(data->btn_play), GTK_RELIEF_NONE);
    gtk_button_set_relief(GTK_BUTTON(data->btn_next), GTK_RELIEF_NONE);
    
    g_signal_connect(data->btn_prev, "clicked", G_CALLBACK(on_prev_clicked), data);
    g_signal_connect(data->btn_play, "clicked", G_CALLBACK(on_play_clicked), data);
    g_signal_connect(data->btn_next, "clicked", G_CALLBACK(on_next_clicked), data);
    
    gtk_box_pack_start(GTK_BOX(data->box), data->btn_prev, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->box), data->btn_play, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->box), data->btn_next, FALSE, FALSE, 0);
    
    /* Label */
    data->lbl_title = gtk_label_new("");
    gtk_widget_set_size_request(data->lbl_title, 120, -1); /* fixed width for marquee */
    gtk_label_set_xalign(GTK_LABEL(data->lbl_title), 0.0);
    gtk_box_pack_start(GTK_BOX(data->box), data->lbl_title, FALSE, FALSE, 4);
    
    /* Setup DBus */
    GError *error = NULL;
    data->bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!error && data->bus) {
        /* Listen for property changes (Metadata, PlaybackStatus) */
        g_dbus_connection_signal_subscribe(data->bus,
                                           NULL,
                                           "org.freedesktop.DBus.Properties",
                                           "PropertiesChanged",
                                           NULL,
                                           "org.mpris.MediaPlayer2.Player",
                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                           on_dbus_signal,
                                           data, NULL);
                                           
        /* Listen for NameOwnerChanged to detect open/close of players */
        data->name_owner_id = g_dbus_connection_signal_subscribe(data->bus,
                                           "org.freedesktop.DBus",
                                           "org.freedesktop.DBus",
                                           "NameOwnerChanged",
                                           "/org/freedesktop/DBus",
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                           on_name_owner_changed,
                                           data, NULL);
                                           
        find_initial_player(data);
    } else if (error) {
        g_warning("Could not connect to session bus for MPRIS: %s", error->message);
        g_error_free(error);
    }
    
    g_signal_connect(data->box, "destroy", G_CALLBACK(on_widget_destroy), data);
    
    if (!data->active_player) {
        gtk_widget_hide(data->box);
    }
    
    return data->box;
}

VenomPanelPluginAPI* venom_panel_plugin_init(void) {
    static VenomPanelPluginAPI api;
    api.name          = "Media Controls";
    api.description   = "Displays MPRIS media controls (Play/Pause/Skip).";
    api.author        = "Venom";
    api.expand        = FALSE;
    api.padding       = 4;
    api.create_widget = create_mpris_widget;
    return &api;
}
