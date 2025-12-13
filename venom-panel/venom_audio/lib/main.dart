// import 'package:flutter/material.dart';
// import 'services/audio_service.dart';

// void main() => runApp(const AudioApp());

// class AudioApp extends StatelessWidget {
//   const AudioApp({super.key});

//   @override
//   Widget build(BuildContext context) => MaterialApp(
//         title: 'Venom Audio Pro',
//         debugShowCheckedModeBanner: false,
//         theme: ThemeData.dark(useMaterial3: true).copyWith(
//           colorScheme: ColorScheme.dark(
//             primary: Colors.deepPurple,
//             secondary: Colors.purpleAccent,
//             surface: const Color(0xFF1E1E2E),
//           ),
//           scaffoldBackgroundColor: const Color(0xFF0D0D15),
//           cardTheme: CardThemeData(
//             color: const Color(0xFF161B22),
//             elevation: 4,
//             shape:
//                 RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
//           ),
//         ),
//         home: const AudioPage(),
//       );
// }

// class AudioPage extends StatefulWidget {
//   const AudioPage({super.key});
//   @override
//   State<AudioPage> createState() => _AudioPageState();
// }

// class _AudioPageState extends State<AudioPage>
//     with SingleTickerProviderStateMixin {
//   final _service = AudioService();
//   bool _connected = false;
//   late TabController _tabController;

//   // Main audio
//   int _volume = 0;
//   bool _muted = false;
//   int _micVolume = 0;
//   bool _micMuted = false;
//   bool _overamp = false;
//   int _maxVolume = 100;

//   // Devices
//   List<AudioDevice> _sinks = [];
//   List<AudioDevice> _sources = [];

//   // Apps
//   List<AppStream> _apps = [];

//   // Cards/Profiles
//   List<AudioCard> _cards = [];
//   String? _selectedCard;
//   List<AudioProfile> _profiles = [];

//   @override
//   void initState() {
//     super.initState();
//     _tabController = TabController(length: 4, vsync: this);
//     _connect();
//   }

//   Future<void> _connect() async {
//     try {
//       await _service.connect();
//       setState(() => _connected = true);
//       _refresh();
//     } catch (e) {
//       _snack('❌ فشل الاتصال بالعفريت');
//     }
//   }

//   Future<void> _refresh() async {
//     if (!_connected) return;

//     final volume = await _service.getVolume();
//     final muted = await _service.getMuted();
//     final micVolume = await _service.getMicVolume();
//     final micMuted = await _service.getMicMuted();
//     final overamp = await _service.getOveramplification();
//     final maxVolume = await _service.getMaxVolume();
//     final sinks = await _service.getSinks();
//     final sources = await _service.getSources();
//     final apps = await _service.getAppStreams();
//     final cards = await _service.getCards();

//     setState(() {
//       _volume = volume;
//       _muted = muted;
//       _micVolume = micVolume;
//       _micMuted = micMuted;
//       _overamp = overamp;
//       _maxVolume = maxVolume;
//       _sinks = sinks;
//       _sources = sources;
//       _apps = apps;
//       _cards = cards;
//     });

//     if (_selectedCard != null) {
//       final profiles = await _service.getProfiles(_selectedCard!);
//       setState(() => _profiles = profiles);
//     }
//   }

//   void _snack(String msg) => ScaffoldMessenger.of(context).showSnackBar(
//       SnackBar(content: Text(msg), behavior: SnackBarBehavior.floating));

//   @override
//   Widget build(BuildContext context) {
//     return Scaffold(
//       appBar: AppBar(
//         title: const Text('🔊 Venom Audio Pro'),
//         actions: [
//           IconButton(icon: const Icon(Icons.refresh), onPressed: _refresh),
//         ],
//         bottom: TabBar(
//           controller: _tabController,
//           tabs: const [
//             Tab(icon: Icon(Icons.volume_up), text: 'الصوت'),
//             Tab(icon: Icon(Icons.apps), text: 'التطبيقات'),
//             Tab(icon: Icon(Icons.devices), text: 'الأجهزة'),
//             Tab(icon: Icon(Icons.settings), text: 'الإعدادات'),
//           ],
//         ),
//       ),
//       body: !_connected
//           ? const Center(child: CircularProgressIndicator())
//           : TabBarView(
//               controller: _tabController,
//               children: [
//                 _buildMainTab(),
//                 _buildAppsTab(),
//                 _buildDevicesTab(),
//                 _buildSettingsTab(),
//               ],
//             ),
//     );
//   }

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🔊 Main Tab
//   // ═══════════════════════════════════════════════════════════════════════════

//   Widget _buildMainTab() => SingleChildScrollView(
//         padding: const EdgeInsets.all(16),
//         child: Column(children: [
//           _buildVolumeCard(),
//           const SizedBox(height: 16),
//           _buildMicCard(),
//         ]),
//       );

//   Widget _buildVolumeCard() => Card(
//         child: Padding(
//           padding: const EdgeInsets.all(20),
//           child: Column(children: [
//             Row(children: [
//               Icon(_muted ? Icons.volume_off : Icons.volume_up,
//                   size: 48, color: _muted ? Colors.red : Colors.deepPurple),
//               const SizedBox(width: 16),
//               Expanded(
//                   child: Column(
//                 crossAxisAlignment: CrossAxisAlignment.start,
//                 children: [
//                   const Text('الصوت الرئيسي',
//                       style:
//                           TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
//                   Text('$_volume%',
//                       style: TextStyle(
//                           fontSize: 36,
//                           fontWeight: FontWeight.bold,
//                           color: _muted
//                               ? Colors.grey
//                               : (_volume > 100
//                                   ? Colors.orange
//                                   : Colors.deepPurple))),
//                 ],
//               )),
//               IconButton(
//                 icon:
//                     Icon(_muted ? Icons.volume_off : Icons.volume_up, size: 32),
//                 color: _muted ? Colors.red : Colors.grey,
//                 onPressed: () async {
//                   await _service.setMuted(!_muted);
//                   _refresh();
//                 },
//               ),
//             ]),
//             const SizedBox(height: 16),
//             Row(children: [
//               const Text('0%'),
//               Expanded(
//                   child: Slider(
//                 value: _volume.toDouble().clamp(0, _maxVolume.toDouble()),
//                 min: 0,
//                 max: _maxVolume.toDouble(),
//                 divisions: _maxVolume,
//                 activeColor: _volume > 100 ? Colors.orange : Colors.deepPurple,
//                 onChanged: (v) => setState(() => _volume = v.round()),
//                 onChangeEnd: (v) => _service.setVolume(v.round()),
//               )),
//               Text('$_maxVolume%'),
//             ]),
//           ]),
//         ),
//       );

//   Widget _buildMicCard() => Card(
//         child: Padding(
//           padding: const EdgeInsets.all(20),
//           child: Column(children: [
//             Row(children: [
//               Icon(_micMuted ? Icons.mic_off : Icons.mic,
//                   size: 48, color: _micMuted ? Colors.red : Colors.teal),
//               const SizedBox(width: 16),
//               Expanded(
//                   child: Column(
//                 crossAxisAlignment: CrossAxisAlignment.start,
//                 children: [
//                   const Text('الميكروفون',
//                       style:
//                           TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
//                   Text('$_micVolume%',
//                       style: TextStyle(
//                           fontSize: 36,
//                           fontWeight: FontWeight.bold,
//                           color: _micMuted ? Colors.grey : Colors.teal)),
//                 ],
//               )),
//               IconButton(
//                 icon: Icon(_micMuted ? Icons.mic_off : Icons.mic, size: 32),
//                 color: _micMuted ? Colors.red : Colors.grey,
//                 onPressed: () async {
//                   await _service.setMicMuted(!_micMuted);
//                   _refresh();
//                 },
//               ),
//             ]),
//             const SizedBox(height: 16),
//             Slider(
//               value: _micVolume.toDouble().clamp(0, 100),
//               min: 0,
//               max: 100,
//               divisions: 100,
//               activeColor: Colors.teal,
//               onChanged: (v) => setState(() => _micVolume = v.round()),
//               onChangeEnd: (v) => _service.setMicVolume(v.round()),
//             ),
//           ]),
//         ),
//       );

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🎵 Apps Tab
//   // ═══════════════════════════════════════════════════════════════════════════

//   Widget _buildAppsTab() => _apps.isEmpty
//       ? const Center(
//           child: Text('لا توجد تطبيقات تستخدم الصوت حالياً',
//               style: TextStyle(color: Colors.grey)))
//       : ListView.builder(
//           padding: const EdgeInsets.all(16),
//           itemCount: _apps.length,
//           itemBuilder: (_, i) {
//             final app = _apps[i];
//             return Card(
//               child: Padding(
//                 padding: const EdgeInsets.all(16),
//                 child: Column(children: [
//                   Row(children: [
//                     Icon(app.muted ? Icons.volume_off : Icons.apps,
//                         size: 40, color: app.muted ? Colors.red : Colors.blue),
//                     const SizedBox(width: 12),
//                     Expanded(
//                         child: Column(
//                       crossAxisAlignment: CrossAxisAlignment.start,
//                       children: [
//                         Text(app.name,
//                             style: const TextStyle(
//                                 fontSize: 16, fontWeight: FontWeight.bold)),
//                         Text('${app.volume}%',
//                             style: TextStyle(
//                                 color: app.muted ? Colors.grey : Colors.blue)),
//                       ],
//                     )),
//                     IconButton(
//                       icon:
//                           Icon(app.muted ? Icons.volume_off : Icons.volume_up),
//                       onPressed: () async {
//                         await _service.setAppMuted(app.index, !app.muted);
//                         _refresh();
//                       },
//                     ),
//                   ]),
//                   Slider(
//                     value:
//                         app.volume.toDouble().clamp(0, _maxVolume.toDouble()),
//                     min: 0,
//                     max: _maxVolume.toDouble(),
//                     activeColor: Colors.blue,
//                     onChangeEnd: (v) async {
//                       await _service.setAppVolume(app.index, v.round());
//                       _refresh();
//                     },
//                     onChanged: (_) {},
//                   ),
//                 ]),
//               ),
//             );
//           },
//         );

//   // ═══════════════════════════════════════════════════════════════════════════
//   // 🔈 Devices Tab
//   // ═══════════════════════════════════════════════════════════════════════════

//   Widget _buildDevicesTab() => SingleChildScrollView(
//         padding: const EdgeInsets.all(16),
//         child: Column(children: [
//           _buildSinksCard(),
//           const SizedBox(height: 16),
//           _buildSourcesCard(),
//         ]),
//       );

//   Widget _buildSinksCard() => Card(
//         child: Padding(
//           padding: const EdgeInsets.all(16),
//           child:
//               Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
//             const Text('🔈 أجهزة الإخراج',
//                 style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
//             const SizedBox(height: 8),
//             if (_sinks.isEmpty)
//               const Text('لا توجد أجهزة', style: TextStyle(color: Colors.grey))
//             else
//               ..._sinks.map((s) => ListTile(
//                     leading: Icon(Icons.speaker,
//                         color: s.isDefault ? Colors.deepPurple : Colors.grey),
//                     title: Text(s.description,
//                         maxLines: 1, overflow: TextOverflow.ellipsis),
//                     trailing: s.isDefault
//                         ? const Icon(Icons.check, color: Colors.deepPurple)
//                         : TextButton(
//                             onPressed: () async {
//                               await _service.setDefaultSink(s.name);
//                               _refresh();
//                             },
//                             child: const Text('تعيين')),
//                   )),
//           ]),
//         ),
//       );

//   Widget _buildSourcesCard() => Card(
//         child: Padding(
//           padding: const EdgeInsets.all(16),
//           child:
//               Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
//             const Text('🎙️ أجهزة الإدخال',
//                 style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
//             const SizedBox(height: 8),
//             if (_sources.isEmpty)
//               const Text('لا توجد أجهزة', style: TextStyle(color: Colors.grey))
//             else
//               ..._sources.map((s) => ListTile(
//                     leading: Icon(Icons.mic,
//                         color: s.isDefault ? Colors.teal : Colors.grey),
//                     title: Text(s.description,
//                         maxLines: 1, overflow: TextOverflow.ellipsis),
//                     trailing: s.isDefault
//                         ? const Icon(Icons.check, color: Colors.teal)
//                         : TextButton(
//                             onPressed: () async {
//                               await _service.setDefaultSource(s.name);
//                               _refresh();
//                             },
//                             child: const Text('تعيين')),
//                   )),
//           ]),
//         ),
//       );

//   // ═══════════════════════════════════════════════════════════════════════════
//   // ⚙️ Settings Tab
//   // ═══════════════════════════════════════════════════════════════════════════

//   Widget _buildSettingsTab() => SingleChildScrollView(
//         padding: const EdgeInsets.all(16),
//         child: Column(children: [
//           _buildOverampCard(),
//           const SizedBox(height: 16),
//           _buildProfilesCard(),
//         ]),
//       );

//   Widget _buildOverampCard() => Card(
//         child: SwitchListTile(
//           title: const Text('🎚️ تضخيم الصوت (Over-Amplification)'),
//           subtitle: Text(_overamp ? 'الحد الأقصى: 150%' : 'الحد الأقصى: 100%'),
//           value: _overamp,
//           activeColor: Colors.orange,
//           onChanged: (v) async {
//             await _service.setOveramplification(v);
//             _refresh();
//           },
//         ),
//       );

//   Widget _buildProfilesCard() => Card(
//         child: Padding(
//           padding: const EdgeInsets.all(16),
//           child:
//               Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
//             const Text('📊 ملفات تعريف الصوت',
//                 style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
//             const SizedBox(height: 12),
//             if (_cards.isEmpty)
//               const Text('لا توجد كروت صوت',
//                   style: TextStyle(color: Colors.grey))
//             else
//               DropdownButton<String>(
//                 value: _selectedCard,
//                 hint: const Text('اختر كرت الصوت'),
//                 isExpanded: true,
//                 items: _cards
//                     .map((c) => DropdownMenuItem(
//                           value: c.name,
//                           child: Text(c.description,
//                               overflow: TextOverflow.ellipsis),
//                         ))
//                     .toList(),
//                 onChanged: (v) async {
//                   setState(() => _selectedCard = v);
//                   if (v != null) {
//                     final profiles = await _service.getProfiles(v);
//                     setState(() => _profiles = profiles);
//                   }
//                 },
//               ),
//             if (_profiles.isNotEmpty) ...[
//               const SizedBox(height: 12),
//               ..._profiles.where((p) => p.available).map((p) => ListTile(
//                     leading: const Icon(Icons.audio_file, color: Colors.amber),
//                     title: Text(p.description,
//                         maxLines: 2, overflow: TextOverflow.ellipsis),
//                     dense: true,
//                     onTap: () async {
//                       await _service.setProfile(_selectedCard!, p.name);
//                       _snack('✅ تم تعيين: ${p.description}');
//                     },
//                   )),
//             ],
//           ]),
//         ),
//       );

//   @override
//   void dispose() {
//     _tabController.dispose();
//     _service.dispose();
//     super.dispose();
//   }
// }
