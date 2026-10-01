BUILD_DIR := build
.DEFAULT_GOAL := all
OBJ_DIR := $(BUILD_DIR)/obj
OUTPUT_DIR := $(BUILD_DIR)/output

RTOOL_DIR := rebax_build_tool
RTOOL_SRC_DIR := $(RTOOL_DIR)/src
ifeq ($(OS),Windows_NT)
  RTOOL_EXE := .exe
else
  RTOOL_EXE :=
endif
RTOOL := $(RTOOL_DIR)/build_tool$(RTOOL_EXE)
RTOOL_C_SOURCES := $(wildcard $(RTOOL_SRC_DIR)/*.c)
RTOOL_SOURCES := $(RTOOL_C_SOURCES) $(wildcard $(RTOOL_SRC_DIR)/*.h)
HOSTCC := cc
HOST_TRIPLE := $(shell $(HOSTCC) -dumpmachine 2>/dev/null)

TARGET_PLATFORM :=
TARGET_ARCH :=
ifneq ($(filter android-arm64 android-arm64-v8a,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := android
  TARGET_ARCH := arm64-v8a
else ifneq ($(filter android-armeabi-v7a,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := android
  TARGET_ARCH := armeabi-v7a
else ifneq ($(filter android-x86,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := android
  TARGET_ARCH := x86
else ifneq ($(filter android-x86_64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := android
  TARGET_ARCH := x86_64
else ifneq ($(filter macos-arm64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := macos
  TARGET_ARCH := arm64
else ifneq ($(filter macos-x86_64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := macos
  TARGET_ARCH := x86_64
else ifneq ($(filter ios-arm64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := ios
  TARGET_ARCH := arm64
else ifneq ($(filter ios-x86_64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := ios
  TARGET_ARCH := x86_64
else ifneq ($(filter linux-arm64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := linux
  TARGET_ARCH := arm64
else ifneq ($(filter linux-x86,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := linux
  TARGET_ARCH := x86
else ifneq ($(filter linux-x86_64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := linux
  TARGET_ARCH := x86_64
else ifneq ($(filter windows-arm64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := windows
  TARGET_ARCH := arm64
else ifneq ($(filter windows-x86,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := windows
  TARGET_ARCH := x86
else ifneq ($(filter windows-x86_64,$(MAKECMDGOALS)),)
  TARGET_PLATFORM := windows
  TARGET_ARCH := x86_64
else
  HOST_TRIPLE := $(shell $(HOSTCC) -dumpmachine 2>/dev/null)
  ifeq ($(OS),Windows_NT)
    TARGET_PLATFORM := windows
  else ifneq ($(findstring apple-ios,$(HOST_TRIPLE)),)
    TARGET_PLATFORM := ios
  else ifneq ($(findstring apple-darwin,$(HOST_TRIPLE)),)
    TARGET_PLATFORM := macos
  else
    TARGET_PLATFORM := linux
  endif
  ifneq ($(findstring aarch64,$(HOST_TRIPLE)),)
    TARGET_ARCH := arm64
  else ifneq ($(findstring arm64,$(HOST_TRIPLE)),)
    TARGET_ARCH := arm64
  else ifneq ($(findstring armv7,$(HOST_TRIPLE)),)
    TARGET_ARCH := armeabi-v7a
  else ifneq ($(findstring x86_64,$(HOST_TRIPLE)),)
    TARGET_ARCH := x86_64
  else ifneq ($(findstring amd64,$(HOST_TRIPLE)),)
    TARGET_ARCH := x86_64
  else
    TARGET_ARCH := x86
  endif
endif

ENGINE_VERSION := v0.0.1
TARGET_SUFFIX := $(TARGET_PLATFORM)_$(TARGET_ARCH)
TARGET_NAME := Rebax_Engine_$(ENGINE_VERSION)_$(TARGET_SUFFIX)
ifeq ($(TARGET_PLATFORM),windows)
  TARGET_NAME := $(TARGET_NAME).exe
endif
TARGET := $(OUTPUT_DIR)/$(TARGET_NAME)

EMBED_ASM_FIXUP := :
ifeq ($(TARGET_PLATFORM),windows)
  ifeq ($(TARGET_ARCH),x86)
    EMBED_ASM_FIXUP := sed -i -e 's/^    \.global _binary/    .global binary/' -e 's/^_binary/binary/'
  endif
endif

ifeq ($(TARGET_PLATFORM),linux)
  ifeq ($(TARGET_ARCH),x86)
    TARGET_CC := i686-linux-gnu-gcc
  else ifeq ($(TARGET_ARCH),arm64)
    TARGET_CC := aarch64-linux-gnu-gcc
  else
    TARGET_CC := gcc
  endif
else ifeq ($(TARGET_PLATFORM),windows)
  ifeq ($(TARGET_ARCH),x86)
    TARGET_TRIPLE := i686-w64-mingw32
  else ifeq ($(TARGET_ARCH),x86_64)
    TARGET_TRIPLE := x86_64-w64-mingw32
  else
    TARGET_TRIPLE := aarch64-w64-mingw32
  endif
  LLVM_MINGW_ROOT := $(firstword $(wildcard llvm-mingw-extracted/*))
  TARGET_CC := $(LLVM_MINGW_ROOT)/bin/$(TARGET_TRIPLE)-clang.exe
else ifeq ($(TARGET_PLATFORM),android)
  ifeq ($(TARGET_ARCH),armeabi-v7a)
    TARGET_TRIPLE := armv7a-linux-androideabi
  else ifeq ($(TARGET_ARCH),arm64-v8a)
    TARGET_TRIPLE := aarch64-linux-android
  else ifeq ($(TARGET_ARCH),x86)
    TARGET_TRIPLE := i686-linux-android
  else
    TARGET_TRIPLE := x86_64-linux-android
  endif
  ANDROID_NDK_ROOT := $(if $(ANDROID_NDK_HOME),$(ANDROID_NDK_HOME),$(firstword $(wildcard $(ANDROID_HOME)/ndk/*)))
  ANDROID_NDK_BIN := $(firstword $(wildcard $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/*/bin))
  TARGET_CC := $(ANDROID_NDK_BIN)/$(TARGET_TRIPLE)21-clang
else ifeq ($(TARGET_PLATFORM),macos)
  TARGET_CC := clang -arch $(TARGET_ARCH) -isysroot $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null) -mmacosx-version-min=11.0
else
  ifeq ($(TARGET_ARCH),arm64)
    TARGET_CC := clang -arch arm64 -isysroot $(shell xcrun --sdk iphoneos --show-sdk-path 2>/dev/null) -mios-version-min=13.0
  else
    TARGET_CC := clang -arch x86_64 -isysroot $(shell xcrun --sdk iphonesimulator --show-sdk-path 2>/dev/null) -mios-simulator-version-min=13.0
  endif
endif

TARGET_LDFLAGS :=
ifeq ($(TARGET_PLATFORM),macos)
  ifeq ($(TARGET_ARCH),arm64)
    TARGET_LDFLAGS := -Wl,-ld_classic
  endif
endif

RTOOL_PLATFORM_CFLAGS :=
ifeq ($(TARGET_PLATFORM),macos)
  RTOOL_PLATFORM_CFLAGS := -D_DARWIN_C_SOURCE
else ifeq ($(TARGET_PLATFORM),ios)
  RTOOL_PLATFORM_CFLAGS := -D_DARWIN_C_SOURCE
endif

$(RTOOL): $(RTOOL_SOURCES)
	@echo "==> Building $(RTOOL) with $(HOSTCC)"
	$(HOSTCC) -std=c99 -O2 -DZ7_ST -D_7ZIP_ST -D_POSIX_C_SOURCE=200809L $(RTOOL_PLATFORM_CFLAGS) -o $@ $(RTOOL_C_SOURCES) -lm

.PHONY: rebax-build-tool
rebax-build-tool: $(RTOOL)

TOOLCHAIN_RELEASE_BASE := https://github.com/PS2HomeDeveloper/Rebax-Toolchains/releases/download/1.0.0
TOOLCHAINS_DIR := embedded/toolchains
ifeq ($(TARGET_PLATFORM),android)
  ifeq ($(TARGET_ARCH),arm64-v8a)
    TOOLCHAIN_ASSET := rebax-toolchains-android-arm64-v8a.tar.xz
  else ifeq ($(TARGET_ARCH),armeabi-v7a)
    TOOLCHAIN_ASSET := rebax-toolchains-android-armeabi-v7a.tar.xz
  else ifeq ($(TARGET_ARCH),x86_64)
    TOOLCHAIN_ASSET := rebax-toolchains-android-x86_64.tar.xz
  else
    TOOLCHAIN_ASSET := rebax-toolchains-android-x86.tar.xz
  endif
else ifeq ($(TARGET_PLATFORM),ios)
  ifeq ($(TARGET_ARCH),arm64)
    TOOLCHAIN_ASSET := rebax-toolchains-ios-arm64.tar.xz
  else
    TOOLCHAIN_ASSET := rebax-toolchains-ios-x86_64.tar.xz
  endif
else ifeq ($(TARGET_PLATFORM),macos)
  ifeq ($(TARGET_ARCH),arm64)
    TOOLCHAIN_ASSET := rebax-toolchains-macos-arm64.tar.xz
  else
    TOOLCHAIN_ASSET := rebax-toolchains-macos-x86_64.tar.xz
  endif
else ifeq ($(TARGET_PLATFORM),windows)
  ifeq ($(TARGET_ARCH),arm64)
    TOOLCHAIN_ASSET := rebax-toolchains-windows-arm64.tar.xz
  else ifeq ($(TARGET_ARCH),x86_64)
    TOOLCHAIN_ASSET := rebax-toolchains-windows-x86_64.tar.xz
  else
    TOOLCHAIN_ASSET := rebax-toolchains-windows-x86.tar.xz
  endif
else ifeq ($(TARGET_ARCH),arm64)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-arm64.tar.xz
else ifeq ($(TARGET_ARCH),x86)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86.tar.xz
else
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86_64.tar.xz
endif
ifeq ($(TARGET_PLATFORM),windows)
  TOOLCHAIN_MAKE_BIN := make.exe
else
  TOOLCHAIN_MAKE_BIN := make
endif
TOOLCHAIN_REQUIRED := $(TOOLCHAINS_DIR)/ps2dev.tar.xz $(TOOLCHAINS_DIR)/$(TOOLCHAIN_MAKE_BIN)
TOOLCHAIN_ASSET_URL := $(TOOLCHAIN_RELEASE_BASE)/$(TOOLCHAIN_ASSET)
TOOLCHAIN_TMP := $(BUILD_DIR)/.$(TOOLCHAIN_ASSET).part

.PHONY: prepare-toolchain
prepare-toolchain: $(RTOOL) | $(BUILD_DIR)
	@dir_exists=$$($(RTOOL) exists $(TOOLCHAINS_DIR)); \
	dir_empty=$$($(RTOOL) dir-empty $(TOOLCHAINS_DIR)); \
	first_exists=$$($(RTOOL) exists $(word 1,$(TOOLCHAIN_REQUIRED))); \
	second_exists=$$($(RTOOL) exists $(word 2,$(TOOLCHAIN_REQUIRED))); \
	if [ "$$dir_exists" = 1 ] && [ "$$dir_empty" = 0 ]; then \
		if [ "$$first_exists" = 1 ] && [ "$$second_exists" = 1 ]; then \
			exit 0; \
		fi; \
		echo "Warning: PS2 toolchain directory is not empty but required files are missing."; \
		exit 0; \
	fi; \
	echo "==> Preparing PS2 toolchain $(TOOLCHAIN_ASSET)"; \
	$(RTOOL) mkdir $(TOOLCHAINS_DIR); \
	$(RTOOL) rm $(TOOLCHAIN_TMP); \
	if curl --version >/dev/null 2>&1; then \
		curl -fL --retry 3 --connect-timeout 15 -o $(TOOLCHAIN_TMP) $(TOOLCHAIN_ASSET_URL); \
	elif wget --version >/dev/null 2>&1; then \
		wget -O $(TOOLCHAIN_TMP) $(TOOLCHAIN_ASSET_URL); \
	else \
		echo "curl or wget is required"; exit 1; \
	fi; \
	$(RTOOL) extract $(TOOLCHAIN_TMP) $(TOOLCHAINS_DIR); \
	$(RTOOL) rm $(TOOLCHAIN_TMP); \
	if [ "$$($(RTOOL) exists $(word 1,$(TOOLCHAIN_REQUIRED)))" != 1 ] || [ "$$($(RTOOL) exists $(word 2,$(TOOLCHAIN_REQUIRED)))" != 1 ]; then \
		echo "Warning: downloaded PS2 toolchain is missing required files."; \
	fi

$(TOOLCHAIN_REQUIRED): prepare-toolchain

SRC_DIR := src
EMBEDDED_DIR := embedded
ICON_SRC_DIR := $(EMBEDDED_DIR)/resources/images/icons/icons_src
ICON_ATLAS_DIR := $(EMBEDDED_DIR)/resources/images/icons
ICON_ATLAS_PNGS := $(sort $(wildcard $(ICON_ATLAS_DIR)/icons[0-9]*.png))
ICON_NAMES_HEADER := $(SRC_DIR)/icon_names.h
ICON_ATLAS_HEADER := $(SRC_DIR)/icon_atlas_pages.h
ICON_SOURCE_FILES := $(sort $(wildcard $(ICON_SRC_DIR)/*.png))
rwildcard = $(filter-out $(patsubst %/,%,$(wildcard $1*/)),$(wildcard $1$2)) $(foreach d,$(wildcard $1*/),$(call rwildcard,$d,$2))
NODE_SRC_DIR := $(EMBEDDED_DIR)/nodes
NODE_EDITOR_SRC_DIR := $(SRC_DIR)/nodes_editor
NODE_SOURCE_FILES := $(sort $(wildcard $(NODE_SRC_DIR)/*.c))
NODE_SOURCE_ALL_FILES := $(call rwildcard,$(NODE_SRC_DIR)/,*)
NODE_EDITOR_SOURCE_FILES := $(sort $(wildcard $(NODE_EDITOR_SRC_DIR)/*.c))
NODE_REGISTRY_GENERATED := $(SRC_DIR)/node_registry_generated.h
NODE_EDITOR_REGISTRY_GENERATED := $(SRC_DIR)/node_editor_registry_generated.h
NODE_TYPES_HEADER := $(SRC_DIR)/node_types.h
NODE_ARCHIVE := $(EMBEDDED_DIR)/nodes.tar.xz
SRCS := $(call rwildcard,$(SRC_DIR)/,*.c)
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
EMBEDDED_RESOURCE_FILES := $(sort $(call rwildcard,$(EMBEDDED_DIR)/resources/,*))
EMBEDDED_FILES := $(sort $(filter-out $(ICON_SRC_DIR)/%,$(EMBEDDED_RESOURCE_FILES)) $(TOOLCHAIN_REQUIRED) $(ICON_ATLAS_PNGS) $(NODE_ARCHIVE))
EMBEDDED_OBJS := $(patsubst $(EMBEDDED_DIR)/%,$(OBJ_DIR)/embedded/%.o,$(EMBEDDED_FILES))
OBJS := $(SRC_OBJS) $(EMBEDDED_OBJS)
DEPS := $(SRC_OBJS:.o=.d)

ifeq ($(TARGET_PLATFORM),android)
  SDL2_INCLUDE_DIR := $(firstword $(wildcard sdl2-android/include/SDL2 sdl2-android/include))
  SDL2_LIB_DIR := $(firstword $(wildcard sdl2-android/lib))
  ifeq ($(strip $(SDL2_INCLUDE_DIR)),)
    $(error Android SDL2 headers not found in sdl2-android; build SDL2 before make)
  endif
  ifeq ($(strip $(SDL2_LIB_DIR)),)
    $(error Android SDL2 libraries not found in sdl2-android; build SDL2 before make)
  endif
else
  SDL2_INCLUDE_DIR := $(firstword $(wildcard sdl2-$(TARGET_PLATFORM)/include/SDL2 sdl2-$(TARGET_PLATFORM)/include /usr/include/SDL2 /usr/local/include/SDL2 /opt/homebrew/include/SDL2))
  ifeq ($(strip $(SDL2_INCLUDE_DIR)),)
    SDL2_INCLUDE_DIR := /usr/include/SDL2
  endif
  SDL2_LIB_DIR := $(firstword $(wildcard sdl2-$(TARGET_PLATFORM)/lib /usr/lib/$(HOST_TRIPLE) /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib /opt/homebrew/lib))
  ifeq ($(strip $(SDL2_LIB_DIR)),)
    SDL2_LIB_DIR := /usr/lib
  endif
endif
SDL2_CFLAGS := -I$(SDL2_INCLUDE_DIR)
SDL2_LIBS := -L$(SDL2_LIB_DIR) -lSDL2

CFLAGS := -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I$(SRC_DIR) -MMD -MP $(SDL2_CFLAGS) $(ARCHIVE_CFLAGS)
$(OBJ_DIR)/xz_embedded.o: CFLAGS += -O3
$(OBJ_DIR)/rebax_fs.o: CFLAGS += -O2
LDLIBS := -lm $(SDL2_LIBS)

ifeq ($(TARGET_PLATFORM),windows)
  RUNTIME_LIBS := SDL2.dll
  RUNTIME_DIRS := sdl2-windows/bin sdl2-windows/lib
else ifeq ($(TARGET_PLATFORM),android)
  RUNTIME_LIBS := libSDL2.so
  RUNTIME_DIRS := sdl2-android/lib
else ifeq ($(TARGET_PLATFORM),linux)
  RUNTIME_LIBS := libSDL2-2.0.so.0
  RUNTIME_DIRS := sdl2-linux/lib /usr/lib/$(HOST_TRIPLE) /usr/lib/x86_64-linux-gnu
else ifeq ($(TARGET_PLATFORM),macos)
  RUNTIME_LIBS := libSDL2-2.0.0.dylib
  RUNTIME_DIRS := sdl2-macos/lib /opt/homebrew/lib /usr/local/lib
else
  RUNTIME_LIBS :=
  RUNTIME_DIRS :=
endif

.PHONY: all clean run generate bundle-runtime-libs gen-icons gen-node-registry gen-node-editor-registry gen-node-archive \
  build-selected \
  android-arm64 android-arm64-v8a android-armeabi-v7a android-x86 android-x86_64 \
  macos-arm64 macos-x86_64 ios-arm64 ios-x86_64 \
  linux-arm64 linux-x86 linux-x86_64 windows-arm64 windows-x86 windows-x86_64
all: $(RTOOL) $(TOOLCHAIN_REQUIRED) generate
	+$(MAKE) --no-print-directory TARGET_PLATFORM=$(TARGET_PLATFORM) TARGET_ARCH=$(TARGET_ARCH) build-selected

build-selected: $(TARGET) bundle-runtime-libs

android-arm64 android-arm64-v8a android-armeabi-v7a android-x86 android-x86_64 \
macos-arm64 macos-x86_64 ios-arm64 ios-x86_64 \
linux-arm64 linux-x86 linux-x86_64 windows-arm64 windows-x86 windows-x86_64: all
generate: gen-icons gen-node-registry gen-node-editor-registry gen-node-archive
gen-icons: | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) icon-names $(ICON_NAMES_HEADER) $(ICON_SOURCE_FILES)
	@$(RTOOL) icon-atlas-pages $(ICON_ATLAS_DIR) $(ICON_ATLAS_HEADER) $(ICON_SOURCE_FILES)
gen-node-registry: $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER)
gen-node-editor-registry: $(NODE_EDITOR_REGISTRY_GENERATED)
gen-node-archive: $(NODE_ARCHIVE)

$(ICON_NAMES_HEADER): $(ICON_SOURCE_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) icon-names $@ $(ICON_SOURCE_FILES)
$(NODE_ARCHIVE): $(NODE_SOURCE_ALL_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) pack $@ $(EMBEDDED_DIR) nodes
$(TARGET): $(OBJS) | $(RTOOL) $(OUTPUT_DIR)
	$(TARGET_CC) $(OBJS) -o $@ $(TARGET_LDFLAGS) $(LDFLAGS) $(LDLIBS)

bundle-runtime-libs: $(TARGET) | $(RTOOL)
	@$(RTOOL) mkdir $(OUTPUT_DIR)/libs
	@for lib in $(RUNTIME_LIBS); do for dir in $(RUNTIME_DIRS); do if $(RTOOL) exists "$$dir/$$lib"; then $(RTOOL) cp "$$dir/$$lib" "$(OUTPUT_DIR)/libs/$$lib"; break; fi; done; done

$(BUILD_DIR): | $(RTOOL)
	@$(RTOOL) mkdir $@
$(OUTPUT_DIR): | $(RTOOL)
	@$(RTOOL) mkdir $@
$(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER): $(NODE_SOURCE_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) node-registry $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER) $(NODE_SOURCE_FILES)
$(NODE_EDITOR_REGISTRY_GENERATED): $(NODE_EDITOR_SOURCE_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) node-editor-registry $@ $(NODE_EDITOR_SOURCE_FILES)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(ICON_NAMES_HEADER) $(NODE_TYPES_HEADER) $(NODE_REGISTRY_GENERATED) $(NODE_EDITOR_REGISTRY_GENERATED) | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	$(TARGET_CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/embedded/toolchains/$(TOOLCHAIN_MAKE_BIN).o: $(TOOLCHAINS_DIR)/$(TOOLCHAIN_MAKE_BIN) | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	@$(RTOOL) embed-asm $@.S $< embedded/toolchains/make
	@$(EMBED_ASM_FIXUP) $@.S
	$(TARGET_CC) -c $@.S -o $@
$(OBJ_DIR)/embedded/%.o: $(EMBEDDED_DIR)/% | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	@$(RTOOL) embed-asm $@.S $<
	@$(EMBED_ASM_FIXUP) $@.S
	$(TARGET_CC) -c $@.S -o $@
clean: | $(RTOOL)
	@$(RTOOL) rm $(BUILD_DIR) $(NODE_ARCHIVE) $(NODE_EDITOR_REGISTRY_GENERATED) $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER) $(ICON_NAMES_HEADER) $(ICON_ATLAS_HEADER)
	@for atlas in $(ICON_ATLAS_DIR)/icons[0-9]*.png; do if [ -f "$$atlas" ]; then $(RTOOL) rm "$$atlas"; fi; done
run: all
	$(TARGET)
-include $(DEPS)
