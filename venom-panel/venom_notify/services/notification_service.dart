import 'package:dbus/dbus.dart';
import '../models/notification_model.dart';

/// Service للتعامل مع D-Bus والاشعارات
class NotificationService {
  final DBusClient _client;
  static const String _serviceName = 'org.freedesktop.Notifications';
  static const String _objectPath = '/org/freedesktop/Notifications';
  static const String _interface = 'org.venom.NotificationHistory';

  NotificationService({DBusClient? client})
      : _client = client ?? DBusClient.session();

  /// الحصول على سجل الإشعارات
  Future<List<NotificationModel>> getHistory() async {
    try {
      var object = DBusRemoteObject(
        _client,
        name: _serviceName,
        path: DBusObjectPath(_objectPath),
      );

      var response = await object.callMethod(
        _interface,
        'GetHistory',
        [],
        replySignature: DBusSignature('a(ussss)'),
      );

      List<NotificationModel> notifications = [];
      var array = response.values[0].asArray();

      for (var item in array) {
        var struct = item.asStruct();
        notifications.add(NotificationModel(
          id: struct[0].asUint32(),
          app: struct[1].asString(),
          icon: struct[2].asString(),
          title: struct[3].asString(),
          body: struct[4].asString(),
        ));
      }

      return notifications;
    } catch (e) {
      print('❌ Error fetching history: $e');
      rethrow;
    }
  }

  /// مسح جميع الإشعارات
  Future<void> clearHistory() async {
    try {
      var object = DBusRemoteObject(
        _client,
        name: _serviceName,
        path: DBusObjectPath(_objectPath),
      );

      await object.callMethod(_interface, 'ClearHistory', []);
      print('✅ History cleared');
    } catch (e) {
      print('❌ Error clearing history: $e');
      rethrow;
    }
  }

  /// حذف إشعار واحد
  Future<void> removeNotification(int id) async {
    try {
      var object = DBusRemoteObject(
        _client,
        name: _serviceName,
        path: DBusObjectPath(_objectPath),
      );

      await object.callMethod(
        _interface,
        'RemoveNotification',
        [DBusUint32(id)],
      );
      print('✅ Notification $id removed');
    } catch (e) {
      print('❌ Error removing notification: $e');
      rethrow;
    }
  }

  /// تفعيل/تعطيل وضع عدم الإزعاج
  Future<void> setDoNotDisturb(bool enabled) async {
    try {
      var object = DBusRemoteObject(
        _client,
        name: _serviceName,
        path: DBusObjectPath(_objectPath),
      );

      await object.callMethod(
        _interface,
        'SetDoNotDisturb',
        [DBusBoolean(enabled)],
      );
      print('✅ Do Not Disturb: ${enabled ? 'ON' : 'OFF'}');
    } catch (e) {
      print('❌ Error setting Do Not Disturb: $e');
      rethrow;
    }
  }

  /// الحصول على حالة وضع عدم الإزعاج
  Future<bool> getDoNotDisturb() async {
    try {
      var object = DBusRemoteObject(
        _client,
        name: _serviceName,
        path: DBusObjectPath(_objectPath),
      );

      var response = await object.callMethod(
        _interface,
        'GetDoNotDisturb',
        [],
        replySignature: DBusSignature('b'),
      );

      return response.values[0].asBoolean();
    } catch (e) {
      print('❌ Error getting Do Not Disturb state: $e');
      rethrow;
    }
  }

  /// الاستماع لتحديثات الإشعارات
  Stream<void> listenToUpdates() {
    return DBusSignalStream(
      _client,
      sender: _serviceName,
      interface: _interface,
      name: 'HistoryUpdated',
    ).map((_) => null); // تحويل Signal إلى void Stream
  }

  /// الاستماع لتغيرات وضع عدم الإزعاج
  Stream<bool> listenToDoNotDisturbChanges() {
    return DBusSignalStream(
      _client,
      sender: _serviceName,
      interface: _interface,
      name: 'DoNotDisturbChanged',
    ).map((signal) => signal.values[0].asBoolean());
  }

  /// إغلاق الاتصال
  Future<void> dispose() async {
    await _client.close();
  }
}
