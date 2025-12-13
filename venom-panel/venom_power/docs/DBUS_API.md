# 📡 Venom Power D-Bus API Reference

> دليل واجهات D-Bus للتطبيقات مثل Panel و Settings

---

## 🔌 معلومات الاتصال

| الخاصية | القيمة |
|---------|--------|
| **Bus** | `Session` |
| **Service** | `org.venom.Power` |
| **Path** | `/org/venom/Power` |
| **Interface** | `org.venom.Power` |

### مثال الاتصال من Dart/Flutter:
```dart
final connection = DBusClient.session();
final object = DBusRemoteObject(
  connection,
  name: 'org.venom.Power',
  path: DBusObjectPath('/org/venom/Power'),
);
```

### مثال الاتصال من Python:
```python
import dbus
bus = dbus.SessionBus()
power = bus.get_object('org.venom.Power', '/org/venom/Power')
iface = dbus.Interface(power, 'org.venom.Power')
```

---

## ⚡ أوامر الطاقة

### `Shutdown() → (b success)`
إيقاف تشغيل النظام.

### `Reboot() → (b success)`
إعادة تشغيل النظام.

### `Suspend() → (b success)`
تعليق النظام (Sleep).

### `Hibernate() → (b success)`
السبات العميق.

### `Logout() → (b success)`
تسجيل الخروج من الجلسة.

### `LockScreen() → (b success)`
قفل الشاشة.

**مثال:**
```bash
dbus-send --session --print-reply --dest=org.venom.Power \
  /org/venom/Power org.venom.Power.Suspend
```

---

## 💡 سطوع الشاشة

### `GetBrightness() → (i level)`
الحصول على مستوى السطوع الحالي.

### `SetBrightness(i level) → (b success)`
تعيين مستوى السطوع.

### `GetMaxBrightness() → (i max_level)`
الحصول على أقصى مستوى سطوع.

**مثال Flutter:**
```dart
// قراءة السطوع
final result = await object.callMethod('org.venom.Power', 'GetBrightness', []);
final brightness = result.values.first.asInt32();

// تعيين السطوع
await object.callMethod('org.venom.Power', 'SetBrightness', [DBusInt32(500)]);
```

---

## ⌨️ إضاءة الكيبورد

### `IsKeyboardBacklightSupported() → (b supported)`
التحقق من دعم إضاءة الكيبورد.

### `GetKeyboardBrightness() → (i level)`
الحصول على مستوى إضاءة الكيبورد.

### `SetKeyboardBrightness(i level) → (b success)`
تعيين إضاءة الكيبورد.

### `GetKeyboardMaxBrightness() → (i max_level)`
أقصى مستوى لإضاءة الكيبورد.

---

## 🔋 البطارية

### `GetBatteryInfo() → (d percentage, b charging, x time_to_empty)`
الحصول على معلومات البطارية.

| المخرج | النوع | الوصف |
|--------|-------|-------|
| `percentage` | `double` | نسبة الشحن (0-100) |
| `charging` | `boolean` | هل يتم الشحن |
| `time_to_empty` | `int64` | الوقت حتى الفراغ (ثواني) |

### `GetPowerSource() → (b on_battery)`
هل الجهاز يعمل على البطارية.

**مثال Flutter:**
```dart
final result = await object.callMethod('org.venom.Power', 'GetBatteryInfo', []);
final percentage = result.values[0].asDouble();
final charging = result.values[1].asBoolean();
final timeToEmpty = result.values[2].asInt64();
```

---

## 💻 حالة الأجهزة

### `GetLidState() → (b closed)`
حالة غطاء اللابتوب (مغلق/مفتوح).

### `GetIdleState() → (b is_idle, b screen_dimmed, b screen_blanked)`
حالة الخمول والشاشة.

---

## 🚫 نظام المنع (Inhibitors)

### `Inhibit(s what, s who, s why) → (u cookie)`
إضافة مانع (لمنع السكون/إطفاء الشاشة).

| المدخل | القيم الممكنة |
|--------|---------------|
| `what` | `idle`, `suspend`, `shutdown` |
| `who` | اسم التطبيق |
| `why` | سبب المنع |

### `UnInhibit(u cookie)`
إزالة المانع.

### `ListInhibitors() → (a(uss) inhibitors)`
قائمة المانعات النشطة: `[(id, app_name, reason), ...]`

**مثال - منع السكون أثناء تشغيل الفيديو:**
```dart
// إضافة مانع
final result = await object.callMethod('org.venom.Power', 'Inhibit', [
  DBusString('idle'),
  DBusString('VideoPlayer'),
  DBusString('Playing video'),
]);
final cookie = result.values.first.asUint32();

// إزالة المانع عند الانتهاء
await object.callMethod('org.venom.Power', 'UnInhibit', [DBusUint32(cookie)]);
```

---

## ⏰ إعدادات الخمول

### `GetIdleTimeouts() → (u dim, u blank, u suspend)`
الحصول على أوقات الخمول بالثواني.

### `SetIdleTimeouts(u dim, u blank, u suspend)`
تعيين أوقات الخمول.

### `SimulateActivity()`
محاكاة نشاط المستخدم (إعادة ضبط المؤقتات).

---

## ⚙️ إعدادات الإجراءات

### `GetLidAction() → (s action_ac, s action_battery)`
الحصول على إجراء الغطاء.

### `SetLidAction(s action_ac, s action_battery)`
تعيين إجراء الغطاء.

| الإجراء | الوصف |
|---------|-------|
| `ignore` | لا تفعل شيئاً |
| `lock` | قفل الشاشة فقط |
| `suspend` | تعليق النظام |
| `hibernate` | السبات |

### `GetPowerButtonAction() → (s action)`
### `SetPowerButtonAction(s action)`

| الإجراء | الوصف |
|---------|-------|
| `interactive` | إظهار قائمة خيارات |
| `suspend` | تعليق فوري |
| `hibernate` | سبات فوري |
| `poweroff` | إيقاف تشغيل |

### `GetCriticalAction() → (s action)`
### `SetCriticalAction(s action)`
الإجراء عند وصول البطارية للحد الخطر.

---

## 📋 إدارة الإعدادات

### `SaveConfig() → (b success)`
حفظ الإعدادات إلى الملف.

### `ReloadConfig() → (b success)`
إعادة تحميل الإعدادات.

### `ResetConfig()`
إعادة الإعدادات للقيم الافتراضية.

---

## ℹ️ معلومات العفريت

### `GetVersion() → (s version)`
الحصول على رقم الإصدار.

### `GetCapabilities() → (as capabilities)`
قائمة القدرات المدعومة:
- `power-control`
- `screen-brightness`
- `keyboard-backlight`
- `battery-monitor`
- `lid-monitor`
- `idle-management`
- `inhibitors`
- `config-management`

---

## 📢 الإشارات (Signals)

للاشتراك في الإشارات من Flutter:

```dart
object.subscribeSignal('org.venom.Power', 'BatteryWarning').listen((signal) {
  final percentage = signal.values.first.asDouble();
  print('Battery warning: $percentage%');
});
```

| الإشارة | المعاملات | الوصف |
|---------|-----------|-------|
| `BatteryWarning` | `(d percentage)` | تحذير انخفاض البطارية |
| `BatteryCritical` | `(d percentage)` | بطارية حرجة |
| `LidStateChanged` | `(b closed)` | تغير حالة الغطاء |
| `PowerSourceChanged` | `(b on_battery)` | تغير مصدر الطاقة |
| `ScreenDimmed` | `(b dimmed)` | تعتيم الشاشة |
| `ScreenBlanked` | `(b blanked)` | إطفاء الشاشة |
| `BrightnessChanged` | `(i level)` | تغير السطوع |
| `KeyboardBrightnessChanged` | `(i level)` | تغير إضاءة الكيبورد |
| `IdleStateChanged` | `(b is_idle)` | تغير حالة الخمول |
| `ConfigChanged` | - | تغير الإعدادات |

---

## 🧪 أوامر الاختبار

```bash
# معلومات البطارية
dbus-send --session --print-reply --dest=org.venom.Power \
  /org/venom/Power org.venom.Power.GetBatteryInfo

# السطوع
dbus-send --session --print-reply --dest=org.venom.Power \
  /org/venom/Power org.venom.Power.GetBrightness

# تعيين السطوع
dbus-send --session --print-reply --dest=org.venom.Power \
  /org/venom/Power org.venom.Power.SetBrightness int32:500

# إضافة مانع
dbus-send --session --print-reply --dest=org.venom.Power \
  /org/venom/Power org.venom.Power.Inhibit \
  string:"idle" string:"Test" string:"Testing"

# القدرات
dbus-send --session --print-reply --dest=org.venom.Power \
  /org/venom/Power org.venom.Power.GetCapabilities

# الإصدار
dbus-send --session --print-reply --dest=org.venom.Power \
  /org/venom/Power org.venom.Power.GetVersion
```
