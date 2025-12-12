// import 'dart:async';
// import 'dart:typed_data';
// import 'package:flutter/material.dart';
// import 'services/sni_service.dart';

// void main() => runApp(const SNIApp());

// class SNIApp extends StatelessWidget {
//   const SNIApp({super.key});

//   @override
//   Widget build(BuildContext context) => MaterialApp(
//         title: 'Venom SNI Test',
//         debugShowCheckedModeBanner: false,
//         theme: ThemeData.dark(useMaterial3: true).copyWith(
//           colorScheme: const ColorScheme.dark(
//             primary: Colors.cyan,
//             secondary: Colors.cyanAccent,
//             surface: Color(0xFF1E1E2E),
//           ),
//           scaffoldBackgroundColor: const Color(0xFF0D0D15),
//           cardTheme: CardThemeData(
//             color: const Color(0xFF161B22),
//             elevation: 4,
//             shape:
//                 RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
//           ),
//         ),
//         home: const SNIPage(),
//       );
// }

// class SNIPage extends StatefulWidget {
//   const SNIPage({super.key});
//   @override
//   State<SNIPage> createState() => _SNIPageState();
// }

// class _SNIPageState extends State<SNIPage> {
//   final _service = SNIService();
//   bool _connected = false;
//   List<SNIItem> _items = [];
//   List<MenuItem> _menuItems = [];
//   String? _selectedItem;
//   StreamSubscription<SNIEvent>? _eventSub;

//   @override
//   void initState() {
//     super.initState();
//     _connect();
//   }

//   Future<void> _connect() async {
//     try {
//       await _service.connect();
//       setState(() => _connected = true);

//       // Subscribe to real-time updates
//       _eventSub = _service.events.listen(_handleEvent);

//       _refresh();
//     } catch (e) {
//       _snack('❌ فشل الاتصال بالعفريت');
//     }
//   }

//   void _handleEvent(SNIEvent event) {
//     print('📡 SNI Event: ${event.type} - ${event.id}');

//     // Refresh on any change
//     _refresh();
//   }

//   Future<void> _refresh() async {
//     if (!_connected) return;
//     final items = await _service.getItems();

//     // Load icons for items without icon names
//     for (final item in items) {
//       if (!item.hasIconName) {
//         await _service.loadIconPixmap(item);
//       }
//     }

//     setState(() => _items = items);
//   }

//   Future<void> _showMenu(SNIItem item) async {
//     final menu = await _service.getMenu(item.id);
//     setState(() {
//       _selectedItem = item.id;
//       _menuItems = menu;
//     });
//   }

//   void _snack(String msg) => ScaffoldMessenger.of(context).showSnackBar(
//       SnackBar(content: Text(msg), behavior: SnackBarBehavior.floating));

//   @override
//   Widget build(BuildContext context) {
//     return Scaffold(
//       appBar: AppBar(
//         title: Row(
//           children: [
//             const Text('🔔 System Tray Items'),
//             const SizedBox(width: 12),
//             if (_connected)
//               Container(
//                 padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
//                 decoration: BoxDecoration(
//                   color: Colors.green.withValues(alpha: 0.2),
//                   borderRadius: BorderRadius.circular(12),
//                 ),
//                 child: Text('${_items.length} items',
//                     style: const TextStyle(fontSize: 12, color: Colors.green)),
//               ),
//           ],
//         ),
//         actions: [
//           IconButton(icon: const Icon(Icons.refresh), onPressed: _refresh),
//         ],
//       ),
//       body: !_connected
//           ? const Center(child: CircularProgressIndicator())
//           : Row(
//               children: [
//                 // Items list
//                 Expanded(flex: 2, child: _buildItemsList()),
//                 // Menu panel
//                 if (_selectedItem != null)
//                   Expanded(flex: 3, child: _buildMenuPanel()),
//               ],
//             ),
//     );
//   }

//   Widget _buildItemsList() => Card(
//         margin: const EdgeInsets.all(16),
//         child: Padding(
//           padding: const EdgeInsets.all(16),
//           child: Column(
//             crossAxisAlignment: CrossAxisAlignment.start,
//             children: [
//               const Row(
//                 children: [
//                   Icon(Icons.apps, color: Colors.cyan),
//                   SizedBox(width: 8),
//                   Text('Tray Items',
//                       style:
//                           TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
//                 ],
//               ),
//               const SizedBox(height: 12),
//               if (_items.isEmpty)
//                 const Expanded(
//                   child: Center(
//                     child: Text(
//                         'لا توجد تطبيقات في الـ System Tray\n\nشغل Discord أو Telegram أو Steam',
//                         textAlign: TextAlign.center,
//                         style: TextStyle(color: Colors.grey)),
//                   ),
//                 )
//               else
//                 Expanded(
//                   child: ListView.builder(
//                     itemCount: _items.length,
//                     itemBuilder: (_, i) {
//                       final item = _items[i];
//                       final isSelected = _selectedItem == item.id;
//                       return Card(
//                         color: isSelected
//                             ? Colors.cyan.withValues(alpha: 0.2)
//                             : null,
//                         child: ListTile(
//                           leading: _buildItemIcon(item),
//                           title: Text(item.title,
//                               maxLines: 1, overflow: TextOverflow.ellipsis),
//                           subtitle: Text(
//                             item.hasIconName
//                                 ? item.iconName
//                                 : (item.hasPixmap
//                                     ? '${item.iconWidth}x${item.iconHeight} pixmap'
//                                     : item.id),
//                             style: TextStyle(
//                                 color: Colors.grey[600], fontSize: 12),
//                           ),
//                           trailing: Row(
//                             mainAxisSize: MainAxisSize.min,
//                             children: [
//                               IconButton(
//                                 icon: const Icon(Icons.touch_app),
//                                 tooltip: 'Activate',
//                                 onPressed: () async {
//                                   await _service.activate(item.id);
//                                   _snack('✅ Activated: ${item.title}');
//                                 },
//                               ),
//                               IconButton(
//                                 icon: const Icon(Icons.menu),
//                                 tooltip: 'Menu',
//                                 onPressed: () => _showMenu(item),
//                               ),
//                             ],
//                           ),
//                         ),
//                       );
//                     },
//                   ),
//                 ),
//             ],
//           ),
//         ),
//       );

//   Widget _buildItemIcon(SNIItem item) {
//     // Priority: IconName > IconPixmap > Status icon
//     if (item.hasIconName) {
//       return Container(
//         width: 40,
//         height: 40,
//         decoration: BoxDecoration(
//           color: Colors.cyan.withValues(alpha: 0.1),
//           borderRadius: BorderRadius.circular(8),
//         ),
//         child: Center(
//           child: Text(
//               item.iconName
//                   .substring(0, item.iconName.length.clamp(0, 2))
//                   .toUpperCase(),
//               style: const TextStyle(
//                   color: Colors.cyan, fontWeight: FontWeight.bold)),
//         ),
//       );
//     } else if (item.hasPixmap) {
//       return Container(
//         width: 40,
//         height: 40,
//         decoration: BoxDecoration(
//           borderRadius: BorderRadius.circular(8),
//         ),
//         child: ClipRRect(
//           borderRadius: BorderRadius.circular(8),
//           child: _buildPixmapImage(
//               item.iconPixmap!, item.iconWidth, item.iconHeight),
//         ),
//       );
//     }
//     return Icon(
//       _getIconForStatus(item.status),
//       color: _getColorForStatus(item.status),
//       size: 32,
//     );
//   }

//   Widget _buildPixmapImage(Uint8List argbData, int width, int height) {
//     // Convert ARGB to RGBA for Flutter
//     final rgbaData = Uint8List(argbData.length);
//     for (int i = 0; i < argbData.length; i += 4) {
//       // ARGB -> RGBA
//       rgbaData[i] = argbData[i + 1]; // R
//       rgbaData[i + 1] = argbData[i + 2]; // G
//       rgbaData[i + 2] = argbData[i + 3]; // B
//       rgbaData[i + 3] = argbData[i]; // A
//     }

//     return Image.memory(
//       _createBmpFromRgba(rgbaData, width, height),
//       width: 40,
//       height: 40,
//       fit: BoxFit.contain,
//       errorBuilder: (_, __, ___) =>
//           const Icon(Icons.broken_image, color: Colors.grey),
//     );
//   }

//   Uint8List _createBmpFromRgba(Uint8List rgba, int width, int height) {
//     // Simple raw image - just show colored box as fallback
//     return Uint8List(0);
//   }

//   Widget _buildMenuPanel() => Card(
//         margin: const EdgeInsets.fromLTRB(0, 16, 16, 16),
//         child: Padding(
//           padding: const EdgeInsets.all(16),
//           child: Column(
//             crossAxisAlignment: CrossAxisAlignment.start,
//             children: [
//               Row(
//                 children: [
//                   const Icon(Icons.menu_open, color: Colors.cyan),
//                   const SizedBox(width: 8),
//                   Expanded(
//                     child: Text('Menu: $_selectedItem',
//                         style: const TextStyle(
//                             fontSize: 16, fontWeight: FontWeight.bold),
//                         overflow: TextOverflow.ellipsis),
//                   ),
//                   IconButton(
//                     icon: const Icon(Icons.close),
//                     onPressed: () => setState(() {
//                       _selectedItem = null;
//                       _menuItems = [];
//                     }),
//                   ),
//                 ],
//               ),
//               const Divider(),
//               if (_menuItems.isEmpty)
//                 const Expanded(
//                     child: Center(
//                         child: Text('No menu items',
//                             style: TextStyle(color: Colors.grey))))
//               else
//                 Expanded(
//                   child: ListView.builder(
//                     itemCount: _menuItems.length,
//                     itemBuilder: (_, i) {
//                       final menu = _menuItems[i];
//                       if (menu.label.isEmpty) {
//                         return const Divider(height: 1);
//                       }
//                       return ListTile(
//                         title: Text(menu.label.replaceAll('_', '')),
//                         dense: true,
//                         onTap: () async {
//                           await _service.menuClick(_selectedItem!, menu.id);
//                           _snack('✅ Clicked: ${menu.label}');
//                         },
//                       );
//                     },
//                   ),
//                 ),
//             ],
//           ),
//         ),
//       );

//   IconData _getIconForStatus(String status) {
//     switch (status.toLowerCase()) {
//       case 'needsattention':
//         return Icons.notification_important;
//       case 'passive':
//         return Icons.visibility_off;
//       default:
//         return Icons.apps;
//     }
//   }

//   Color _getColorForStatus(String status) {
//     switch (status.toLowerCase()) {
//       case 'needsattention':
//         return Colors.orange;
//       case 'passive':
//         return Colors.grey;
//       default:
//         return Colors.cyan;
//     }
//   }

//   @override
//   void dispose() {
//     _eventSub?.cancel();
//     _service.dispose();
//     super.dispose();
//   }
// }
