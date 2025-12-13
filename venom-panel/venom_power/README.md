# ⚡ Venom Power Daemon

> عفريت طاقة متكامل لنظام Venom Desktop Environment

[![Version](https://img.shields.io/badge/Version-2.1.0-blue.svg)]()
[![License](https://img.shields.io/badge/License-GPL--3.0-green.svg)](LICENSE)

---

## 📋 نظرة عامة

Venom Power Daemon هو خدمة D-Bus كاملة للتحكم في طاقة النظام، تعمل على Session Bus تحت الاسم `org.venom.Power`.

---

## ✨ الميزات

| الميزة | الوصف |
|--------|-------|
| ⚡ **أوامر الطاقة** | Shutdown, Reboot, Suspend, Hibernate, Logout, Lock |
| 💡 **سطوع الشاشة** | قراءة وتعيين السطوع + تعتيم تلقائي |
| ⌨️ **إضاءة الكيبورد** | التحكم الكامل + إطفاء تلقائي عند الخمول |
| 🔋 **ذكاء البطارية** | تحذيرات + إجراء طوارئ عند 2% |
| ⏳ **إدارة الخمول** | تعتيم → إطفاء الشاشة → سكون تلقائي |
| 💻 **مراقبة الأجهزة** | الغطاء، مصدر الطاقة، زر الطاقة |
| 🚫 **نظام المنع** | Inhibitors للتطبيقات |
| ⚙️ **ملف إعدادات** | تخصيص كامل عبر ملف config |

---

## 🏗️ هيكلية المشروع

```
venom_power/
├── include/          # Headers (9 files)
│   ├── venom_power.h
│   ├── backlight.h
│   ├── battery.h
│   ├── idle.h
│   ├── inhibitor.h
│   ├── logind.h
│   ├── keyboard.h    # جديد
│   ├── config.h      # جديد
│   └── dbus_service.h
│
├── src/              # Sources (8 files)
│   ├── main.c
│   ├── backlight.c
│   ├── battery.c
│   ├── idle.c
│   ├── inhibitor.c
│   ├── logind.c
│   ├── keyboard.c    # جديد
│   ├── config.c      # جديد
│   └── dbus_service.c
│
├── docs/             # التوثيق
│   ├── DBUS_API.md   # واجهات D-Bus
│   └── CONFIG.md     # ملف الإعدادات
│
├── Makefile
└── README.md
```

---

## 🔧 البناء والتثبيت

```bash
# المتطلبات
sudo apt install libglib2.0-dev

# البناء
make

# التشغيل
./venom_power

# التثبيت
sudo make install
```

---

## 📡 واجهة D-Bus

**للتفاصيل الكاملة:** [docs/DBUS_API.md](docs/DBUS_API.md)

### الاتصال
```
Bus: Session
Service: org.venom.Power
Path: /org/venom/Power
Interface: org.venom.Power
```

### الطرق الرئيسية

| الطريقة | الوصف |
|---------|-------|
| `Shutdown()` | إيقاف التشغيل |
| `Suspend()` | السكون |
| `GetBatteryInfo()` | معلومات البطارية |
| `GetBrightness()` / `SetBrightness(level)` | السطوع |
| `GetKeyboardBrightness()` / `SetKeyboardBrightness(level)` | إضاءة الكيبورد |
| `Inhibit(what, who, why)` | إضافة مانع |
| `GetCapabilities()` | القدرات المدعومة |

### الإشارات

| الإشارة | الوصف |
|---------|-------|
| `BatteryWarning(percentage)` | تحذير البطارية |
| `LidStateChanged(closed)` | تغير الغطاء |
| `PowerSourceChanged(on_battery)` | تغير مصدر الطاقة |
| `BrightnessChanged(level)` | تغير السطوع |

---

## ⚙️ ملف الإعدادات

**للتفاصيل:** [docs/CONFIG.md](docs/CONFIG.md)

المسار: `~/.config/venom/power.conf`

```ini
[Idle]
DimTimeoutBattery=120      # 2 دقيقة
BlankTimeoutBattery=300    # 5 دقائق

[Battery]
LowLevel=10
CriticalLevel=5

[Actions]
LidActionBattery=suspend
PowerButtonAction=interactive
```

---

## 🧪 الاختبار

```bash
# معلومات البطارية
make test-battery

# السطوع
make test-brightness

# جميع الاختبارات
make test-all
```

---

## 📄 الترخيص

GPL-3.0
