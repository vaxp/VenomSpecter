# ═══════════════════════════════════════════════════════════════════════════
# 🔧 Venom Power Daemon - Makefile
# ═══════════════════════════════════════════════════════════════════════════

# المترجم والأعلام
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = $(shell pkg-config --cflags --libs gio-2.0 x11 xscrnsaver)

# المجلدات
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# الملفات المصدرية
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# الهدف النهائي
TARGET = venom_power

# ═══════════════════════════════════════════════════════════════════════════
# 🎯 الأهداف الرئيسية
# ═══════════════════════════════════════════════════════════════════════════

.PHONY: all clean install uninstall run debug

# البناء الافتراضي
all: $(BUILD_DIR) $(TARGET)

# إنشاء مجلد البناء
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ربط الملفات
$(TARGET): $(OBJECTS)
	@echo "🔗 Linking $(TARGET)..."
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "✅ Build complete: $(TARGET)"

# ترجمة الملفات المصدرية
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "📦 Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

# ═══════════════════════════════════════════════════════════════════════════
# 🧹 التنظيف
# ═══════════════════════════════════════════════════════════════════════════

clean:
	@echo "🧹 Cleaning..."
	@rm -rf $(BUILD_DIR) $(TARGET)
	@echo "✅ Clean complete"

# ═══════════════════════════════════════════════════════════════════════════
# 📦 التثبيت
# ═══════════════════════════════════════════════════════════════════════════

install: $(TARGET)
	@echo "📦 Installing $(TARGET)..."
	@sudo cp $(TARGET) /usr/local/bin/
	@sudo cp org.venom.Power.service /usr/share/dbus-1/services/ 2>/dev/null || true
	@echo "✅ Installed to /usr/local/bin/$(TARGET)"

uninstall:
	@echo "🗑️ Uninstalling $(TARGET)..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@sudo rm -f /usr/share/dbus-1/services/org.venom.Power.service
	@echo "✅ Uninstalled"

# ═══════════════════════════════════════════════════════════════════════════
# 🚀 التشغيل
# ═══════════════════════════════════════════════════════════════════════════

run: $(TARGET)
	@echo "🚀 Running $(TARGET)..."
	@./$(TARGET)

# بناء مع رموز التصحيح
debug: CFLAGS += -g -DDEBUG
debug: clean all
	@echo "🐛 Debug build complete"

# ═══════════════════════════════════════════════════════════════════════════
# 🧪 الاختبار
# ═══════════════════════════════════════════════════════════════════════════

test-battery:
	@echo "🔋 Testing GetBatteryInfo..."
	@dbus-send --session --print-reply --dest=org.venom.Power \
		/org/venom/Power org.venom.Power.GetBatteryInfo

test-brightness:
	@echo "💡 Testing GetBrightness..."
	@dbus-send --session --print-reply --dest=org.venom.Power \
		/org/venom/Power org.venom.Power.GetBrightness

test-lid:
	@echo "💻 Testing GetLidState..."
	@dbus-send --session --print-reply --dest=org.venom.Power \
		/org/venom/Power org.venom.Power.GetLidState

test-inhibit:
	@echo "🚫 Testing Inhibit..."
	@dbus-send --session --print-reply --dest=org.venom.Power \
		/org/venom/Power org.venom.Power.Inhibit \
		string:"idle" string:"Test" string:"Testing inhibitor"

test-all: test-battery test-brightness test-lid test-profile
	@echo "✅ All tests completed"

test-profile:
	@echo "⚡ Testing Power Profiles..."
	@dbus-send --session --print-reply --dest=org.venom.Power \
		/org/venom/Power org.venom.Power.GetActiveProfile
	@dbus-send --session --print-reply --dest=org.venom.Power \
		/org/venom/Power org.venom.Power.GetProfiles
