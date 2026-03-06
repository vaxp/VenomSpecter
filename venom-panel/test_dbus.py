#!/usr/bin/env python3
import dbus
bus = dbus.SessionBus()
obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
iface = dbus.Interface(obj, 'org.freedesktop.DBus')
names = iface.ListNames()

mpris_names = [n for n in names if n.startswith('org.mpris.MediaPlayer2.')]
print("Found MPRIS names:", mpris_names)

for name in mpris_names:
    print(f"\n--- {name} ---")
    try:
        player = bus.get_object(name, '/org/mpris/MediaPlayer2')
        props = dbus.Interface(player, 'org.freedesktop.DBus.Properties')
        all_props = props.GetAll('org.mpris.MediaPlayer2.Player')
        print(f"PlaybackStatus: {all_props.get('PlaybackStatus')}")
        meta = all_props.get('Metadata', {})
        print(f"Title: {meta.get('xesam:title')}")
        print(f"Artist: {meta.get('xesam:artist')}")
    except Exception as e:
        print("Error:", e)
