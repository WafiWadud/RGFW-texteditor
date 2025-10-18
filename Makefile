# ==============================
#  Wayland RGFW Build Makefile
# ==============================

# ---- Compiler settings ----
CC      := clang
CFLAGS  := -Wall -Wextra -O3 -march=native -flto -ffast-math -funroll-loops \
           -fomit-frame-pointer -ffunction-sections -fdata-sections
LDFLAGS := -Wl,--gc-sections
PKGS    := wayland-client wayland-cursor xkbcommon
LIBS    := $(shell pkg-config --cflags --libs $(PKGS))

# Optional OpenGL/EGL support (uncomment if needed)
# PKGS   += wayland-egl egl glesv2

# ---- Project files ----
SRC     := main.c \
           xdg-shell.c \
           xdg-decoration-unstable-v1.c \
           relative-pointer-unstable-v1.c \
           pointer-constraints-unstable-v1.c \
           xdg-output-unstable-v1.c \
           xdg-toplevel-icon-v1.c
OBJ     := $(SRC:.c=.o)

# ---- Protocol XML sources ----
PROTO_DIR := /usr/share/wayland-protocols
PROTO_STABLE := $(PROTO_DIR)/stable
PROTO_UNSTABLE := $(PROTO_DIR)/unstable
PROTO_STAGING := $(PROTO_DIR)/staging

PROTOCOLS := \
  $(PROTO_STABLE)/xdg-shell/xdg-shell.xml \
  $(PROTO_UNSTABLE)/xdg-decoration/xdg-decoration-unstable-v1.xml \
  $(PROTO_UNSTABLE)/relative-pointer/relative-pointer-unstable-v1.xml \
  $(PROTO_UNSTABLE)/pointer-constraints/pointer-constraints-unstable-v1.xml \
  $(PROTO_UNSTABLE)/xdg-output/xdg-output-unstable-v1.xml \
  $(PROTO_STAGING)/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml

# ---- Output binary ----
TARGET := app

# ---- Default rule ----
all: $(TARGET)

# ---- Build binary ----
main.o: xdg-shell.h xdg-decoration-unstable-v1.h relative-pointer-unstable-v1.h pointer-constraints-unstable-v1.h xdg-output-unstable-v1.h xdg-toplevel-icon-v1.h
$(TARGET): $(OBJ)
	@echo "🔗 Linking $@..."
	$(CC) $(CFLAGS) $^ $(LIBS) $(LDFLAGS) -o $@

# ---- Generate protocol bindings ----
%.h %.c: $(PROTO_DIR)/%/%.xml
	@echo "🧩 Generating $* protocol..."
	wayland-scanner client-header $< $*.h
	wayland-scanner private-code $< $*.c

# ---- Manual dependency generation ----
xdg-shell.c: $(PROTO_STABLE)/xdg-shell/xdg-shell.xml
	wayland-scanner private-code $< $@
xdg-shell.h: $(PROTO_STABLE)/xdg-shell/xdg-shell.xml
	wayland-scanner client-header $< $@

xdg-decoration-unstable-v1.c: $(PROTO_UNSTABLE)/xdg-decoration/xdg-decoration-unstable-v1.xml
	wayland-scanner private-code $< $@
xdg-decoration-unstable-v1.h: $(PROTO_UNSTABLE)/xdg-decoration/xdg-decoration-unstable-v1.xml
	wayland-scanner client-header $< $@

relative-pointer-unstable-v1.c: $(PROTO_UNSTABLE)/relative-pointer/relative-pointer-unstable-v1.xml
	wayland-scanner private-code $< $@
relative-pointer-unstable-v1.h: $(PROTO_UNSTABLE)/relative-pointer/relative-pointer-unstable-v1.xml
	wayland-scanner client-header $< $@

pointer-constraints-unstable-v1.c: $(PROTO_UNSTABLE)/pointer-constraints/pointer-constraints-unstable-v1.xml
	wayland-scanner private-code $< $@
pointer-constraints-unstable-v1.h: $(PROTO_UNSTABLE)/pointer-constraints/pointer-constraints-unstable-v1.xml
	wayland-scanner client-header $< $@

xdg-output-unstable-v1.c: $(PROTO_UNSTABLE)/xdg-output/xdg-output-unstable-v1.xml
	wayland-scanner private-code $< $@
xdg-output-unstable-v1.h: $(PROTO_UNSTABLE)/xdg-output/xdg-output-unstable-v1.xml
	wayland-scanner client-header $< $@

xdg-toplevel-icon-v1.c: $(PROTO_STAGING)/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml
	wayland-scanner private-code $< $@
xdg-toplevel-icon-v1.h: $(PROTO_STAGING)/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml
	wayland-scanner client-header $< $@

# ---- Clean up ----
clean:
	@echo "🧹 Cleaning build files..."
	rm -f $(OBJ) $(TARGET) xdg-*.c xdg-*.h relative-pointer-unstable-v1.* pointer-constraints-unstable-v1.*

.PHONY: all clean

