import 'dart:async';
import 'dart:typed_data';
import 'package:dbus/dbus.dart';

// ═══════════════════════════════════════════════════════════════════════════
// 📦 Models
// ═══════════════════════════════════════════════════════════════════════════

class SNIItem {
  final String id;
  final String title;
  final String iconName;
  final String status;
  Uint8List? iconPixmap;
  int iconWidth;
  int iconHeight;

  SNIItem({
    required this.id,
    required this.title,
    required this.iconName,
    required this.status,
    this.iconPixmap,
    this.iconWidth = 0,
    this.iconHeight = 0,
  });

  factory SNIItem.fromDBus(DBusStruct s) {
    final v = s.children.toList();
    return SNIItem(
      id: (v[0] as DBusString).value,
      title: (v[1] as DBusString).value,
      iconName: (v[2] as DBusString).value,
      status: (v[3] as DBusString).value,
    );
  }

  bool get hasIconName => iconName.isNotEmpty;
  bool get hasPixmap => iconPixmap != null && iconPixmap!.isNotEmpty;
}

class MenuItem {
  final int id;
  final String label;
  final Map<String, dynamic> properties;

  MenuItem({required this.id, required this.label, required this.properties});

  factory MenuItem.fromDBus(DBusStruct s) {
    final v = s.children.toList();
    return MenuItem(
      id: (v[0] as DBusInt32).value,
      label: (v[1] as DBusString).value,
      properties: {},
    );
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// 📡 Signal Events
// ═══════════════════════════════════════════════════════════════════════════

enum SNIEventType { added, removed, changed }

class SNIEvent {
  final SNIEventType type;
  final String id;
  SNIEvent(this.type, this.id);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔌 SNI Service
// ═══════════════════════════════════════════════════════════════════════════

class SNIService {
  late final DBusClient _client;
  late final DBusRemoteObject _sni;
  bool _isConnected = false;

  final _eventController = StreamController<SNIEvent>.broadcast();
  Stream<SNIEvent> get events => _eventController.stream;

  StreamSubscription<DBusSignal>? _addedSub;
  StreamSubscription<DBusSignal>? _removedSub;
  StreamSubscription<DBusSignal>? _changedSub;

  bool get isConnected => _isConnected;

  Future<void> connect() async {
    try {
      _client = DBusClient.session();
      _sni = DBusRemoteObject(
        _client,
        name: 'org.venom.SNI',
        path: DBusObjectPath('/org/venom/SNI'),
      );
      _isConnected = true;

      // Subscribe to signals using DBusSignalStream
      final addedRule = DBusSignalStream(
        _client,
        interface: 'org.venom.SNI',
        name: 'ItemAdded',
        path: DBusObjectPath('/org/venom/SNI'),
      );
      _addedSub = addedRule.listen((signal) {
        final id = signal.values.isNotEmpty
            ? (signal.values.first as DBusString).value
            : '';
        _eventController.add(SNIEvent(SNIEventType.added, id));
      });

      final removedRule = DBusSignalStream(
        _client,
        interface: 'org.venom.SNI',
        name: 'ItemRemoved',
        path: DBusObjectPath('/org/venom/SNI'),
      );
      _removedSub = removedRule.listen((signal) {
        final id = signal.values.isNotEmpty
            ? (signal.values.first as DBusString).value
            : '';
        _eventController.add(SNIEvent(SNIEventType.removed, id));
      });

      final changedRule = DBusSignalStream(
        _client,
        interface: 'org.venom.SNI',
        name: 'ItemChanged',
        path: DBusObjectPath('/org/venom/SNI'),
      );
      _changedSub = changedRule.listen((signal) {
        final id = signal.values.isNotEmpty
            ? (signal.values.first as DBusString).value
            : '';
        _eventController.add(SNIEvent(SNIEventType.changed, id));
      });
    } catch (e) {
      _isConnected = false;
      rethrow;
    }
  }

  Future<void> dispose() async {
    await _addedSub?.cancel();
    await _removedSub?.cancel();
    await _changedSub?.cancel();
    _eventController.close();
    if (_isConnected) {
      await _client.close();
      _isConnected = false;
    }
  }

  // Get all tray items
  Future<List<SNIItem>> getItems() async {
    try {
      final r = await _sni.callMethod('org.venom.SNI', 'GetItems', []);
      return (r.values.first as DBusArray)
          .children
          .map((v) => SNIItem.fromDBus(v as DBusStruct))
          .toList();
    } catch (e) {
      return [];
    }
  }

  // Get icon pixmap for an item
  Future<void> loadIconPixmap(SNIItem item) async {
    try {
      final r = await _sni
          .callMethod('org.venom.SNI', 'GetIconPixmap', [DBusString(item.id)]);
      final values = r.values.toList();

      final width = (values[0] as DBusInt32).value;
      final height = (values[1] as DBusInt32).value;
      final data = (values[2] as DBusArray)
          .children
          .map((v) => (v as DBusByte).value)
          .toList();

      if (width > 0 && height > 0 && data.isNotEmpty) {
        item.iconWidth = width;
        item.iconHeight = height;
        item.iconPixmap = Uint8List.fromList(data);
      }
    } catch (e) {
      // Ignore errors
    }
  }

  // Activate item (left click)
  Future<bool> activate(String id, {int x = 0, int y = 0}) async {
    try {
      final r = await _sni.callMethod('org.venom.SNI', 'Activate',
          [DBusString(id), DBusInt32(x), DBusInt32(y)]);
      return (r.values.first as DBusBoolean).value;
    } catch (e) {
      return false;
    }
  }

  // Secondary activate (middle click)
  Future<bool> secondaryActivate(String id, {int x = 0, int y = 0}) async {
    try {
      final r = await _sni.callMethod('org.venom.SNI', 'SecondaryActivate',
          [DBusString(id), DBusInt32(x), DBusInt32(y)]);
      return (r.values.first as DBusBoolean).value;
    } catch (e) {
      return false;
    }
  }

  // Get menu items
  Future<List<MenuItem>> getMenu(String id) async {
    try {
      final r =
          await _sni.callMethod('org.venom.SNI', 'GetMenu', [DBusString(id)]);
      return (r.values.first as DBusArray)
          .children
          .map((v) => MenuItem.fromDBus(v as DBusStruct))
          .toList();
    } catch (e) {
      return [];
    }
  }

  // Click menu item
  Future<bool> menuClick(String id, int menuId) async {
    try {
      final r = await _sni.callMethod(
          'org.venom.SNI', 'MenuClick', [DBusString(id), DBusInt32(menuId)]);
      return (r.values.first as DBusBoolean).value;
    } catch (e) {
      return false;
    }
  }
}
