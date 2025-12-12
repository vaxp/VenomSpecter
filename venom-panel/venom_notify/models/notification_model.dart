/// نموذج الإشعار
class NotificationModel {
  final int id;
  final String app;
  final String icon;
  final String title;
  final String body;

  NotificationModel({
    required this.id,
    required this.app,
    required this.icon,
    required this.title,
    required this.body,
  });

  /// تحويل من Map (من D-Bus) إلى Model
  factory NotificationModel.fromMap(Map<String, dynamic> map) {
    return NotificationModel(
      id: map['id'] as int,
      app: map['app'] as String,
      icon: map['icon'] as String,
      title: map['title'] as String,
      body: map['body'] as String,
    );
  }

  /// تحويل من Model إلى Map
  Map<String, dynamic> toMap() {
    return {
      'id': id,
      'app': app,
      'icon': icon,
      'title': title,
      'body': body,
    };
  }

  @override
  String toString() => 'NotificationModel(id: $id, app: $app, title: $title)';
}
