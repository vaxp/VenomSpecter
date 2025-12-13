# ⚙️ Venom Power - Configuration File

المسار: `~/.config/venom/power.conf`

---

## صيغة الملف

```ini
[Idle]
# أوقات الخمول بالثواني
DimTimeoutAC=300          # تعتيم الشاشة على الكهرباء (5 دقائق)
DimTimeoutBattery=120     # تعتيم الشاشة على البطارية (2 دقيقة)
BlankTimeoutAC=600        # إطفاء الشاشة على الكهرباء (10 دقائق)
BlankTimeoutBattery=300   # إطفاء الشاشة على البطارية (5 دقائق)
SuspendTimeoutBattery=900 # السكون التلقائي على البطارية (15 دقيقة)

[Battery]
# مستويات التحذير
LowLevel=10               # تحذير انخفاض البطارية
CriticalLevel=5           # تحذير حرج
DangerLevel=2             # إجراء طوارئ

[Actions]
# إجراءات الأحداث
# القيم: ignore, lock, suspend, hibernate, poweroff, interactive
LidActionAC=lock          # إغلاق الغطاء على الكهرباء
LidActionBattery=suspend  # إغلاق الغطاء على البطارية
PowerButtonAction=interactive  # زر الطاقة
CriticalAction=hibernate  # عند البطارية الحرجة

[Brightness]
# السطوع (-1 = لا تغيير تلقائي)
AC=-1                     # السطوع على الكهرباء
Battery=-1                # السطوع على البطارية
DimOnIdle=true            # تعتيم عند الخمول
DimLevelPercent=30        # نسبة التعتيم

[Keyboard]
# إضاءة الكيبورد
AutoOff=true              # إطفاء تلقائي عند الخمول
Timeout=30                # وقت الإطفاء (ثواني)

[Lock]
# قفل الشاشة
OnSuspend=true            # قفل قبل السكون
OnLid=true                # قفل عند إغلاق الغطاء
Command=/path/to/lockscreen
```

---

## إنشاء ملف الإعدادات

```bash
mkdir -p ~/.config/venom
cat > ~/.config/venom/power.conf << 'EOF'
[Idle]
DimTimeoutAC=300
DimTimeoutBattery=120
BlankTimeoutAC=600
BlankTimeoutBattery=300
SuspendTimeoutBattery=900

[Battery]
LowLevel=10
CriticalLevel=5
DangerLevel=2

[Actions]
LidActionAC=lock
LidActionBattery=suspend
PowerButtonAction=interactive
CriticalAction=hibernate

[Brightness]
DimOnIdle=true
DimLevelPercent=30

[Keyboard]
AutoOff=true
Timeout=30

[Lock]
OnSuspend=true
OnLid=true
EOF
```

---

## إعادة تحميل الإعدادات

```bash
dbus-send --session --print-reply --dest=org.venom.Power \
  /org/venom/Power org.venom.Power.ReloadConfig
```
