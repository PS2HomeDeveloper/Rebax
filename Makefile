ifeq ($(origin CC),default)
  DETECTED_CC := $(shell command -v gcc 2>/dev/null)
  ifeq ($(DETECTED_CC),)
    DETECTED_CC := $(shell command -v clang 2>/dev/null)
  endif
  ifeq ($(DETECTED_CC),)
    $(error No C compiler is available (neither GCC nor Clang). Install one or pass it manually with "make CC=<compiler-path>")
  endif
  CC := $(DETECTED_CC)
endif

OBJCOPY ?= $(shell command -v objcopy 2>/dev/null)
ifeq ($(OBJCOPY),)
  $(error objcopy is not available on this system. Install binutils or pass it manually with "make OBJCOPY=<path>")
endif

CC_TARGET := $(shell $(CC) -dumpmachine 2>/dev/null)
ifeq ($(CC_TARGET),)
  $(error Could not determine the compiler target. Verify that the selected compiler supports "-dumpmachine")
endif

# -----------------------------------------------------------------------------
# Rebax toolchains bootstrap
#
# The official source repository intentionally does not contain embedded/
# toolchains because those binaries are platform-specific.  Each release asset
# contains exactly two files at its root: ps2dev.tar.xz and make (make.exe on
# Windows).  Keep TOOLCHAIN_ASSET_URLS in sync with the release whenever a new
# supported compiler target or tool is added.
# -----------------------------------------------------------------------------
TOOLCHAIN_RELEASE_BASE := https://github.com/PS2HomeDeveloper/Rebax-Toolchains/releases/download/1.0.0
TOOLCHAINS_DIR         := embedded/toolchains
TOOLCHAIN_REQUIRED     := ps2dev.tar.xz make

ifneq ($(findstring android,$(CC_TARGET)),)
  ifneq ($(findstring aarch64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-arm64-v8a.tar.xz
  else ifneq ($(findstring armv7,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-armeabi-v7a.tar.xz
  else ifneq ($(findstring arm,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-armeabi-v7a.tar.xz
  else ifneq ($(findstring x86_64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-x86_64.tar.xz
  else ifneq ($(findstring i686,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-x86.tar.xz
  else ifneq ($(findstring x86,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-x86.tar.xz
  endif
else ifneq ($(findstring apple-ios,$(CC_TARGET)),)
  ifneq ($(findstring arm64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-ios-arm64.tar.xz
  else
    TOOLCHAIN_ASSET := rebax-toolchains-ios-x86_64.tar.xz
  endif
else ifneq ($(findstring apple-darwin,$(CC_TARGET)),)
  ifneq ($(findstring arm64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-macos-arm64.tar.xz
  else
    TOOLCHAIN_ASSET := rebax-toolchains-macos-x86_64.tar.xz
  endif
else ifneq ($(findstring mingw,$(CC_TARGET)),)
  ifneq ($(findstring aarch64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-windows-arm64.tar.xz
  else ifneq ($(findstring x86_64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-windows-x86_64.tar.xz
  else
    TOOLCHAIN_ASSET := rebax-toolchains-windows-x86.tar.xz
  endif
else ifneq ($(findstring windows,$(CC_TARGET)),)
  ifneq ($(findstring aarch64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-windows-arm64.tar.xz
  else ifneq ($(findstring x86_64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-windows-x86_64.tar.xz
  else
    TOOLCHAIN_ASSET := rebax-toolchains-windows-x86.tar.xz
  endif
else ifneq ($(findstring x86_64,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86_64.tar.xz
else ifneq ($(findstring aarch64,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-arm64.tar.xz
else ifneq ($(findstring i686,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86.tar.xz
else ifneq ($(findstring x86,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86.tar.xz
endif

ifeq ($(strip $(TOOLCHAIN_ASSET)),)
  $(error Unsupported compiler target '$(CC_TARGET)'; no Rebax toolchain release asset is defined for it)
endif

ifneq ($(findstring mingw,$(CC_TARGET)),)
  TOOLCHAIN_REQUIRED := ps2dev.tar.xz make.exe
else ifneq ($(findstring windows,$(CC_TARGET)),)
  TOOLCHAIN_REQUIRED := ps2dev.tar.xz make.exe
else
  TOOLCHAIN_REQUIRED := ps2dev.tar.xz make
endif

TOOLCHAIN_ASSET_URL := $(TOOLCHAIN_RELEASE_BASE)/$(TOOLCHAIN_ASSET)

# This runs while Make parses the file, before embedded resources are expanded.
# That ordering is deliberate: the downloaded files must be visible to the
# existing embedded-resource wildcard during this same first build.
define ENSURE_TOOLCHAINS
set -eu; \
dir='$(TOOLCHAINS_DIR)'; \
mkdir -p "$$dir"; \
required_missing=0; \
for f in $(TOOLCHAIN_REQUIRED); do \
  test -f "$$dir/$$f" || required_missing=1; \
done; \
entry_count=$$(find "$$dir" -mindepth 1 -maxdepth 1 -print 2>/dev/null | wc -l); \
if test "$$entry_count" -eq 0; then \
  printf '\033[1;34m[toolchains]\033[0m missing or empty; downloading $(TOOLCHAIN_ASSET)...\n' >&2; \
  tmp="$$dir/.$(TOOLCHAIN_ASSET).part"; \
  rm -f "$$tmp"; \
  if command -v curl >/dev/null 2>&1; then \
    curl -fL --retry 3 --connect-timeout 15 -o "$$tmp" '$(TOOLCHAIN_ASSET_URL)'; \
  elif command -v wget >/dev/null 2>&1; then \
    wget -O "$$tmp" '$(TOOLCHAIN_ASSET_URL)'; \
  else \
    printf '%s\n' '[toolchains] ERROR: curl or wget is required for the first build.' >&2; exit 1; \
  fi; \
  tar -xJf "$$tmp" -C "$$dir"; \
  rm -f "$$tmp"; \
elif test "$$required_missing" -ne 0; then \
  printf '\033[1;33m[toolchains] WARNING:\033[0m required toolchain files are missing, but the toolchains directory is not empty. Continuing...\n' >&2; \
fi
endef

TOOLCHAIN_BOOTSTRAP := $(shell $(ENSURE_TOOLCHAINS); printf '__REBAX_TOOLCHAINS_OK__')
ifeq ($(strip $(TOOLCHAIN_BOOTSTRAP)),)
  $(error Could not prepare embedded/toolchains for compiler target '$(CC_TARGET)'; see the toolchains error above)
endif

ifneq ($(findstring mingw,$(CC_TARGET)),)
  OC_FORMAT := pe-x86-64
  OC_ARCH   := i386:x86-64
else ifneq ($(findstring windows,$(CC_TARGET)),)
  OC_FORMAT := pe-x86-64
  OC_ARCH   := i386:x86-64
else ifneq ($(findstring apple-darwin,$(CC_TARGET)),)
  ifneq ($(findstring arm64,$(CC_TARGET)),)
    OC_FORMAT := mach-o-arm64
    OC_ARCH   := aarch64
  else ifneq ($(findstring aarch64,$(CC_TARGET)),)
    OC_FORMAT := mach-o-arm64
    OC_ARCH   := aarch64
  else
    OC_FORMAT := mach-o-x86-64
    OC_ARCH   := i386:x86-64
  endif
else ifneq ($(findstring x86_64,$(CC_TARGET)),)
  OC_FORMAT := elf64-x86-64
  OC_ARCH   := i386:x86-64
else ifneq ($(findstring amd64,$(CC_TARGET)),)
  OC_FORMAT := elf64-x86-64
  OC_ARCH   := i386:x86-64
else ifneq ($(findstring aarch64,$(CC_TARGET)),)
  OC_FORMAT := elf64-littleaarch64
  OC_ARCH   := aarch64
else ifneq ($(findstring arm64,$(CC_TARGET)),)
  OC_FORMAT := elf64-littleaarch64
  OC_ARCH   := aarch64
else
  $(warning Unknown objcopy format for compiler target ($(CC_TARGET)). Pass it manually with "make OC_FORMAT=... OC_ARCH=...")
endif

TARGET_NAME := Rebax_Engine

SRC_DIR      := src
EMBEDDED_DIR := embedded
BUILD_DIR    := build
OBJ_DIR      := $(BUILD_DIR)/obj
OUTPUT_DIR   := $(BUILD_DIR)/output

TARGET := $(OUTPUT_DIR)/$(TARGET_NAME)

ICON_SRC_DIR      := $(EMBEDDED_DIR)/images/icons/icons_src
ICON_ATLAS_PNG    := $(EMBEDDED_DIR)/images/icons/icons.png
ICON_NAMES_HEADER := $(SRC_DIR)/icon_names.h
ICON_SOURCE_FILES := $(sort $(wildcard $(ICON_SRC_DIR)/*.png))

NODE_SRC_DIR        := $(SRC_DIR)/nodes
NODE_EDITOR_SRC_DIR := $(SRC_DIR)/nodes_editor
NODE_SOURCE_FILES        := $(sort $(wildcard $(NODE_SRC_DIR)/*.c))
NODE_EDITOR_SOURCE_FILES := $(sort $(wildcard $(NODE_EDITOR_SRC_DIR)/*.c))

NODE_REGISTRY_GENERATED        := $(SRC_DIR)/node_registry_generated.h
NODE_EDITOR_REGISTRY_GENERATED := $(SRC_DIR)/node_editor_registry_generated.h
NODE_TYPES_HEADER               := $(SRC_DIR)/node_types.h

EXPORT_DIR             := $(EMBEDDED_DIR)/export
NODE_SOURCES_ARCHIVE   := $(EXPORT_DIR)/node_sources.tar.xz
NODE_SOURCES_ALL_FILES := $(sort $(wildcard $(NODE_SRC_DIR)/*.c) $(wildcard $(NODE_SRC_DIR)/*.h))

PS2DEV_ARCHIVE := $(EMBEDDED_DIR)/toolchains/ps2dev.tar.xz

rwildcard = $(filter-out $(patsubst %/,%,$(wildcard $1*/)),$(wildcard $1$2)) \
            $(foreach d,$(wildcard $1*/),$(call rwildcard,$d,$2))

SRCS := $(call rwildcard,$(SRC_DIR)/,*.c)
SRCS := $(filter-out $(NODE_SRC_DIR)/%,$(SRCS))
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

EMBEDDED_FILES := $(call rwildcard,$(EMBEDDED_DIR),*)
EMBEDDED_FILES := $(filter-out $(ICON_SRC_DIR)/%,$(EMBEDDED_FILES))
EMBEDDED_FILES := $(sort $(EMBEDDED_FILES) $(ICON_ATLAS_PNG) $(NODE_SOURCES_ARCHIVE) $(PS2DEV_ARCHIVE))

EMBEDDED_OBJS := $(patsubst $(EMBEDDED_DIR)/%,$(OBJ_DIR)/%.o,$(EMBEDDED_FILES))

OBJS := $(SRC_OBJS) $(EMBEDDED_OBJS)
DEPS := $(SRC_OBJS:.o=.d)

# ============================================================
# External libraries
# ============================================================

PKG_CONFIG ?= $(shell command -v pkg-config 2>/dev/null)

SDL2_CFLAGS :=
SDL2_LIBS   :=


ifneq ($(PKG_CONFIG),)

  SDL2_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
  SDL2_LIBS   := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)


endif

ifeq ($(strip $(SDL2_CFLAGS)),)
  SDL2_CFLAGS :=
endif

ifeq ($(strip $(SDL2_LIBS)),)
  SDL2_LIBS := -lSDL2
endif



CFLAGS := -Wall -Wextra -std=c11 \
          -D_POSIX_C_SOURCE=200809L \
          -I$(SRC_DIR) \
          -MMD -MP \
          $(SDL2_CFLAGS) \
          $(ARCHIVE_CFLAGS)

# فك ps2dev يتم داخل xz_embedded بدلاً من برنامج xz الخارجي؛ يجب تحسين
# هذه الوحدة تحديداً، وإلا سيُترجم مفكك LZMA بسرعة Debug الافتراضية ويكون
# أبطأ بكثير من xz النظامي المحسن.
$(OBJ_DIR)/xz_embedded.o: CFLAGS += -O3
$(OBJ_DIR)/rebax_fs.o: CFLAGS += -O2

LDLIBS := -lm \
          $(SDL2_LIBS)

.PHONY: all clean run generate gen-icons gen-node-registry gen-node-editor-registry gen-node-archive

all: generate $(TARGET)

generate: gen-icons gen-node-registry gen-node-editor-registry gen-node-archive

gen-icons: $(ICON_NAMES_HEADER) $(ICON_ATLAS_PNG)

gen-node-registry: $(NODE_REGISTRY_GENERATED)

gen-node-editor-registry: $(NODE_EDITOR_REGISTRY_GENERATED)

gen-node-archive: $(NODE_SOURCES_ARCHIVE)

$(ICON_NAMES_HEADER): $(ICON_SOURCE_FILES)
	@echo "==> Generating $(ICON_NAMES_HEADER) from $(words $(ICON_SOURCE_FILES)) icons"
	@echo '/* Automatically generated during the build from embedded/images/icons/icons_src/ - do not edit manually */' > $(ICON_NAMES_HEADER)
	@echo '#ifndef ICON_NAMES_H' >> $(ICON_NAMES_HEADER)
	@echo '#define ICON_NAMES_H' >> $(ICON_NAMES_HEADER)
	@i=0; \
	for f in $(ICON_SOURCE_FILES); do \
		base=$$(basename "$$f"); \
		name=$${base%.*}; \
		safe_name=$$(printf '%s' "$$name" | tr -- '- ' '__' | tr -dc 'A-Za-z0-9_'); \
		echo "#define ICON_$$safe_name $$i" >> $(ICON_NAMES_HEADER); \
		i=$$((i + 1)); \
	done
	@echo "#define ICON_COUNT $(words $(ICON_SOURCE_FILES))" >> $(ICON_NAMES_HEADER)
	@echo '#endif' >> $(ICON_NAMES_HEADER)

$(ICON_ATLAS_PNG): $(ICON_SOURCE_FILES)
	@echo "==> Building icon atlas: $(ICON_ATLAS_PNG)"
	@mkdir -p $(dir $(ICON_ATLAS_PNG))
	@magick -size 256x256 xc:none $(ICON_ATLAS_PNG)
	@i=0; \
	for f in $(ICON_SOURCE_FILES); do \
		x=$$(( (i % 16) * 16 )); \
		y=$$(( (i / 16) * 16 )); \
		magick composite -geometry +$${x}+$${y} "$$f" $(ICON_ATLAS_PNG) $(ICON_ATLAS_PNG); \
		i=$$((i + 1)); \
	done

$(TARGET): $(OBJS) | $(OUTPUT_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(NODE_SOURCES_ARCHIVE): $(NODE_SOURCES_ALL_FILES)
	@echo "==> Compressing src/nodes ($(words $(NODE_SOURCES_ALL_FILES)) files) into $(NODE_SOURCES_ARCHIVE)"
	@mkdir -p $(dir $(NODE_SOURCES_ARCHIVE))
	@tar -cJf $(NODE_SOURCES_ARCHIVE) -C $(SRC_DIR) nodes

define NODE_REGISTRY_AWK
BEGIN {
    entry_count = 0
}

FNR == 1 {
    in_prop_block = 0
    cur_prop_name = ""
}

/^static[ \t]+const[ \t]+node_property_t[ \t]+[A-Za-z0-9_]+\[\][ \t]*=[ \t]*\{/ {
    line = $$0
    sub(/^static[ \t]+const[ \t]+node_property_t[ \t]+/, "", line)
    sub(/\[\].*/, "", line)
    cur_prop_name = line
    in_prop_block = 1
    prop_body[cur_prop_name] = ""
    prop_count[cur_prop_name] = 0
    next
}

in_prop_block && /^\};/ {
    in_prop_block = 0
    next
}

in_prop_block {
    trimmed = $$0
    gsub(/^[ \t]+/, "", trimmed)
    if (trimmed ~ /^\{/) {
        prop_count[cur_prop_name]++
    }
    prop_body[cur_prop_name] = prop_body[cur_prop_name] $$0 "\n"
    next
}

/@NODE/ {
    line = $$0
    sub(/.*@NODE[ \t]+/, "", line)
    sub(/[ \t]*\*\/.*/, "", line)

    node_type = ""; node_name = ""; node_icon = ""; node_props = ""
    n = split(line, pairs, /[ \t]+/)
    for (i = 1; i <= n; i++) {
        eq = index(pairs[i], "=")
        if (eq == 0) continue
        key = substr(pairs[i], 1, eq - 1)
        val = substr(pairs[i], eq + 1)
        gsub(/^"|"$$/, "", val)
        if (key == "type") node_type = val
        else if (key == "name") node_name = val
        else if (key == "icon") node_icon = val
        else if (key == "properties") node_props = val
    }

    entry_count++
    entry_type[entry_count] = node_type
    entry_name[entry_count] = node_name
    entry_icon[entry_count] = node_icon
    entry_props[entry_count] = node_props
}

END {
    print "/* ============================================================"
    print " * node_registry_generated.h"
    print " * ============================================================"
    print " * Automatically generated during the build from each @NODE line"
    print " * in src/nodes .c files and their declared property arrays."
    print " * ============================================================"
    print " */"
    print ""
    print "#ifndef NODE_REGISTRY_GENERATED_H"
    print "#define NODE_REGISTRY_GENERATED_H"
    print ""

    for (e = 1; e <= entry_count; e++) {
        props_var = entry_props[e]
        if (props_var != "NULL") {
            gen_var = props_var "_gen"
            printf "static const node_property_t %s[] = {\n", gen_var
            printf "%s", prop_body[props_var]
            printf "};\n\n"
        }
    }

    print "static const node_registry_entry_t g_node_registry_table[] = {"
    for (e = 1; e <= entry_count; e++) {
        props_var = entry_props[e]
        if (props_var == "NULL") {
            printf "    { %s, \"%s\", ICON_%s, NULL, 0 },\n", entry_type[e], entry_name[e], entry_icon[e]
        } else {
            gen_var = props_var "_gen"
            printf "    { %s, \"%s\", ICON_%s, %s, %d },\n", entry_type[e], entry_name[e], entry_icon[e], gen_var, prop_count[props_var]
        }
    }
    print "};"
    printf "#define NODE_REGISTRY_GENERATED_COUNT %d\n", entry_count
    print ""

    seen_count = 0
    for (e = 1; e <= entry_count; e++) {
        t = entry_type[e]
        if (!(t in seen)) {
            seen[t] = 1
            seen_count++
            unique_type[seen_count] = t
        }
    }

    print "/* ============================================================" > types_header
    print " * node_types.h" > types_header
    print " * ============================================================" > types_header
    print " * Automatically generated during the build from each @NODE line" > types_header
    print " * in src/nodes .c files. Do not edit manually." > types_header
    print " * A new node adds a new @NODE line and appears automatically." > types_header
    print " * ============================================================" > types_header
    print " */" > types_header
    print "" > types_header
    print "#ifndef NODE_TYPES_H" > types_header
    print "#define NODE_TYPES_H" > types_header
    print "" > types_header
    print "typedef enum {" > types_header
    for (u = 1; u <= seen_count; u++) {
        sep = (u < seen_count) ? "," : ""
        printf "    %s%s\n", unique_type[u], sep > types_header
    }
    print "} node_type_t;" > types_header
    print "" > types_header
    print "#endif /* NODE_TYPES_H */" > types_header

    print "#endif /* NODE_REGISTRY_GENERATED_H */"
}
endef

define NODE_EDITOR_REGISTRY_AWK

BEGIN {
    entry_count = 0
}

/@NODE_EDITOR/ {
    line = $$0
    sub(/.*@NODE_EDITOR[ \t]*/, "", line)
    sub(/[ \t]*\*\/.*/, "", line)

    node_type = ""; draw_2d = "NULL"; draw_3d = "NULL"
    n = split(line, pairs, /[ \t]+/)
    for (i = 1; i <= n; i++) {
        eq = index(pairs[i], "=")
        if (eq == 0) continue
        key = substr(pairs[i], 1, eq - 1)
        val = substr(pairs[i], eq + 1)
        if (key == "type") node_type = val
        else if (key == "draw_2d") draw_2d = val
        else if (key == "draw_3d") draw_3d = val
    }
    if (node_type == "") next

    entry_count++
    entry_type[entry_count] = node_type
    entry_draw2d[entry_count] = draw_2d
    entry_draw3d[entry_count] = draw_3d
}

END {
    print "/* ============================================================"
    print " * node_editor_registry_generated.h"
    print " * ============================================================"
    print " * Automatically generated during the build from each @NODE_EDITOR"
    print " * line in src/nodes_editor .c files."
    print " * ============================================================"
    print " */"
    print ""
    print "#ifndef NODE_EDITOR_REGISTRY_GENERATED_H"
    print "#define NODE_EDITOR_REGISTRY_GENERATED_H"
    print ""

    for (e = 1; e <= entry_count; e++) {
        if (entry_draw2d[e] != "NULL") {
            printf "void %s(const node_property_value_t *values, int property_count, int cam_x, int cam_y, int cam_w, int cam_h, int selected);\n", entry_draw2d[e]
        }
        if (entry_draw3d[e] != "NULL") {
            printf "void %s(const node_property_value_t *values, int property_count, int cam_x, int cam_y, int cam_w, int cam_h, int selected);\n", entry_draw3d[e]
        }
    }
    print ""

    print "static const node_editor_registry_entry_t g_node_editor_registry_table[] = {"
    for (e = 1; e <= entry_count; e++) {
        printf "    { %s, %s, %s },\n", entry_type[e], entry_draw2d[e], entry_draw3d[e]
    }
    print "};"
    printf "#define NODE_EDITOR_REGISTRY_GENERATED_COUNT %d\n", entry_count
    print ""
    print "#endif /* NODE_EDITOR_REGISTRY_GENERATED_H */"
}
endef

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER): $(NODE_SOURCE_FILES) | $(BUILD_DIR)
	@echo "==> Generating $(NODE_REGISTRY_GENERATED) and $(NODE_TYPES_HEADER) from $(words $(NODE_SOURCE_FILES)) node files"
	$(file >$(BUILD_DIR)/.gen_node_registry.awk,$(NODE_REGISTRY_AWK))
	@awk -v types_header=$(NODE_TYPES_HEADER) -f $(BUILD_DIR)/.gen_node_registry.awk $(NODE_SOURCE_FILES) > $(NODE_REGISTRY_GENERATED)

$(NODE_EDITOR_REGISTRY_GENERATED): $(NODE_EDITOR_SOURCE_FILES) | $(BUILD_DIR)
	@echo "==> Generating $(NODE_EDITOR_REGISTRY_GENERATED) from $(words $(NODE_EDITOR_SOURCE_FILES)) visual representation files"
	$(file >$(BUILD_DIR)/.gen_node_editor_registry.awk,$(NODE_EDITOR_REGISTRY_AWK))
	@awk -f $(BUILD_DIR)/.gen_node_editor_registry.awk $(NODE_EDITOR_SOURCE_FILES) > $(NODE_EDITOR_REGISTRY_GENERATED)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(ICON_NAMES_HEADER) $(NODE_TYPES_HEADER) $(NODE_REGISTRY_GENERATED) $(NODE_EDITOR_REGISTRY_GENERATED)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(EMBEDDED_DIR)/%
	mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O $(OC_FORMAT) -B $(OC_ARCH) $< $@

clean:
	rm -rf $(BUILD_DIR) $(ICON_NAMES_HEADER) $(NODE_TYPES_HEADER) $(NODE_REGISTRY_GENERATED) $(NODE_EDITOR_REGISTRY_GENERATED) $(NODE_SOURCES_ARCHIVE)

run: all
	$(TARGET)

-include $(DEPS)
