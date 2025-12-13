#ifndef AUDIO_CLIENT_H
#define AUDIO_CLIENT_H

#include <glib.h>
#include <gio/gio.h>

void audio_client_init(void);
void audio_client_cleanup(void);

/* Volume Control */
gint audio_get_volume(void);
void audio_set_volume(gint percent);

/* Mute Control */
gboolean audio_get_muted(void);
void audio_set_muted(gboolean muted);

/* Limits */
gint audio_get_max_volume(void);

/* Callbacks */
typedef void (*AudioVolumeCallback)(gint volume, gpointer user_data);
typedef void (*AudioMuteCallback)(gboolean muted, gpointer user_data);

void audio_client_on_volume_changed(AudioVolumeCallback cb, gpointer user_data);
void audio_client_on_mute_changed(AudioMuteCallback cb, gpointer user_data);

#endif
