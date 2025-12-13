// import 'package:dbus/dbus.dart';

// // ═══════════════════════════════════════════════════════════════════════════
// // 🔊 Models
// // ═══════════════════════════════════════════════════════════════════════════

// class AudioDevice {
//   final String name;
//   final String description;
//   final int volume;
//   final bool isDefault;

//   AudioDevice({
//     required this.name,
//     required this.description,
//     required this.volume,
//     required this.isDefault,
//   });

//   factory AudioDevice.fromDBus(DBusStruct s) {
//     final v = s.children.toList();
//     return AudioDevice(
//       name: (v[0] as DBusString).value,
//       description: (v[1] as DBusString).value,
//       volume: (v[2] as DBusInt32).value,
//       isDefault: (v[3] as DBusBoolean).value,
//     );
//   }
// }

// class AppStream {
//   final int index;
//   final String name;
//   final String icon;
//   final int volume;
//   final bool muted;

//   AppStream({
//     required this.index,
//     required this.name,
//     required this.icon,
//     required this.volume,
//     required this.muted,
//   });

//   factory AppStream.fromDBus(DBusStruct s) {
//     final v = s.children.toList();
//     return AppStream(
//       index: (v[0] as DBusUint32).value,
//       name: (v[1] as DBusString).value,
//       icon: (v[2] as DBusString).value,
//       volume: (v[3] as DBusInt32).value,
//       muted: (v[4] as DBusBoolean).value,
//     );
//   }
// }

// class AudioCard {
//   final String name;
//   final String description;

//   AudioCard({required this.name, required this.description});

//   factory AudioCard.fromDBus(DBusStruct s) {
//     final v = s.children.toList();
//     return AudioCard(
//       name: (v[0] as DBusString).value,
//       description: (v[1] as DBusString).value,
//     );
//   }
// }

// class AudioProfile {
//   final String name;
//   final String description;
//   final bool available;

//   AudioProfile(
//       {required this.name, required this.description, required this.available});

//   factory AudioProfile.fromDBus(DBusStruct s) {
//     final v = s.children.toList();
//     return AudioProfile(
//       name: (v[0] as DBusString).value,
//       description: (v[1] as DBusString).value,
//       available: (v[2] as DBusBoolean).value,
//     );
//   }
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 🔊 Audio Service
// // ═══════════════════════════════════════════════════════════════════════════

// class AudioService {
//   late final DBusClient _client;
//   late final DBusRemoteObject _audio;
//   bool _isConnected = false;

//   bool get isConnected => _isConnected;

//   Future<void> connect() async {
//     try {
//       _client = DBusClient.session();
//       _audio = DBusRemoteObject(
//         _client,
//         name: 'org.venom.Audio',
//         path: DBusObjectPath('/org/venom/Audio'),
//       );
//       _isConnected = true;
//     } catch (e) {
//       _isConnected = false;
//       rethrow;
//     }
//   }

//   Future<void> dispose() async {
//     if (_isConnected) {
//       await _client.close();
//       _isConnected = false;
//     }
//   }

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🔊 Volume Control
//   // ═══════════════════════════════════════════════════════════════════════════

//   Future<int> getVolume() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetVolume', []);
//       return (r.values.first as DBusInt32).value;
//     } catch (e) {
//       return 0;
//     }
//   }

//   Future<bool> setVolume(int volume) async {
//     try {
//       final r = await _audio
//           .callMethod('org.venom.Audio', 'SetVolume', [DBusInt32(volume)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> getMuted() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetMuted', []);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> setMuted(bool muted) async {
//     try {
//       final r = await _audio
//           .callMethod('org.venom.Audio', 'SetMuted', [DBusBoolean(muted)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🎤 Microphone Control
//   // ═══════════════════════════════════════════════════════════════════════════

//   Future<int> getMicVolume() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetMicVolume', []);
//       return (r.values.first as DBusInt32).value;
//     } catch (e) {
//       return 0;
//     }
//   }

//   Future<bool> setMicVolume(int volume) async {
//     try {
//       final r = await _audio
//           .callMethod('org.venom.Audio', 'SetMicVolume', [DBusInt32(volume)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> getMicMuted() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetMicMuted', []);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> setMicMuted(bool muted) async {
//     try {
//       final r = await _audio
//           .callMethod('org.venom.Audio', 'SetMicMuted', [DBusBoolean(muted)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🔈 Sinks (Output Devices)
//   // ═══════════════════════════════════════════════════════════════════════════

//   Future<List<AudioDevice>> getSinks() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetSinks', []);
//       return (r.values.first as DBusArray)
//           .children
//           .map((v) => AudioDevice.fromDBus(v as DBusStruct))
//           .toList();
//     } catch (e) {
//       return [];
//     }
//   }

//   Future<bool> setDefaultSink(String name) async {
//     try {
//       final r = await _audio
//           .callMethod('org.venom.Audio', 'SetDefaultSink', [DBusString(name)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> setSinkVolume(String name, int volume) async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'SetSinkVolume',
//           [DBusString(name), DBusInt32(volume)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🎤 Sources (Input Devices)
//   // ═══════════════════════════════════════════════════════════════════════════

//   Future<List<AudioDevice>> getSources() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetSources', []);
//       return (r.values.first as DBusArray)
//           .children
//           .map((v) => AudioDevice.fromDBus(v as DBusStruct))
//           .toList();
//     } catch (e) {
//       return [];
//     }
//   }

//   Future<bool> setDefaultSource(String name) async {
//     try {
//       final r = await _audio.callMethod(
//           'org.venom.Audio', 'SetDefaultSource', [DBusString(name)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> setSourceVolume(String name, int volume) async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'SetSourceVolume',
//           [DBusString(name), DBusInt32(volume)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🎵 App Streams
//   // ═══════════════════════════════════════════════════════════════════════════

//   Future<List<AppStream>> getAppStreams() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetAppStreams', []);
//       return (r.values.first as DBusArray)
//           .children
//           .map((v) => AppStream.fromDBus(v as DBusStruct))
//           .toList();
//     } catch (e) {
//       return [];
//     }
//   }

//   Future<bool> setAppVolume(int index, int volume) async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'SetAppVolume',
//           [DBusUint32(index), DBusInt32(volume)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> setAppMuted(int index, bool muted) async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'SetAppMuted',
//           [DBusUint32(index), DBusBoolean(muted)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> moveAppToSink(int index, String sinkName) async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'MoveAppToSink',
//           [DBusUint32(index), DBusString(sinkName)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 📊 Cards & Profiles
//   // ═══════════════════════════════════════════════════════════════════════════

//   Future<List<AudioCard>> getCards() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetCards', []);
//       return (r.values.first as DBusArray)
//           .children
//           .map((v) => AudioCard.fromDBus(v as DBusStruct))
//           .toList();
//     } catch (e) {
//       return [];
//     }
//   }

//   Future<List<AudioProfile>> getProfiles(String cardName) async {
//     try {
//       final r = await _audio
//           .callMethod('org.venom.Audio', 'GetProfiles', [DBusString(cardName)]);
//       return (r.values.first as DBusArray)
//           .children
//           .map((v) => AudioProfile.fromDBus(v as DBusStruct))
//           .toList();
//     } catch (e) {
//       return [];
//     }
//   }

//   Future<bool> setProfile(String cardName, String profile) async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'SetProfile',
//           [DBusString(cardName), DBusString(profile)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🎚️ Over-amplification
//   // ═══════════════════════════════════════════════════════════════════════════

//   Future<bool> getOveramplification() async {
//     try {
//       final r = await _audio
//           .callMethod('org.venom.Audio', 'GetOveramplification', []);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<bool> setOveramplification(bool enabled) async {
//     try {
//       final r = await _audio.callMethod(
//           'org.venom.Audio', 'SetOveramplification', [DBusBoolean(enabled)]);
//       return (r.values.first as DBusBoolean).value;
//     } catch (e) {
//       return false;
//     }
//   }

//   Future<int> getMaxVolume() async {
//     try {
//       final r = await _audio.callMethod('org.venom.Audio', 'GetMaxVolume', []);
//       return (r.values.first as DBusInt32).value;
//     } catch (e) {
//       return 100;
//     }
//   }
// }
