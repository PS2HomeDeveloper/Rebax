# =============================================================================
# Requirements installed on the machine (searched for by this Makefile):
#   - a C compiler: gcc (or clang)
#   - make
#   - curl (or wget), used once to download the toolchains
#   - SDL2 (SDL.h and the library)
# Every other step (mkdir, rm, cp, tar, xz, awk, find, icon atlas, ...) is done
# by rebax_build_tool/build_tool, which is compiled from rebax_build_tool/src the first
# time make runs (and again whenever the executable is missing).
# =============================================================================

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
OUTPUT_DIR := $(BUILD_DIR)/output

RTOOL_DIR     := rebax_build_tool
RTOOL_SRC_DIR := $(RTOOL_DIR)/src
ifeq ($(OS),Windows_NT)
  RTOOL_EXE := .exe
else
  RTOOL_EXE :=
endif
RTOOL         := $(RTOOL_DIR)/build_tool$(RTOOL_EXE)
RTOOL_SOURCES := $(wildcard $(RTOOL_SRC_DIR)/*.c)

# build_tool runs on the build host, so never fall back to the target compiler (CC).
ifeq ($(wildcard $(RTOOL)),)
  ifeq ($(strip $(HOSTCC)),)
    ifneq ($(strip $(shell gcc -dumpmachine)),)
      HOSTCC := gcc
    else ifneq ($(strip $(shell clang -dumpmachine)),)
      HOSTCC := clang
    else ifneq ($(strip $(shell cc -dumpmachine)),)
      HOSTCC := cc
    else
      $(error No C compiler is available (neither GCC nor Clang). Install one or pass it manually with "make HOSTCC=<compiler-path>")
    endif
  endif
  $(info [rebax_build_tool] building $(RTOOL) with $(HOSTCC)...)
  ifeq ($(shell uname -s),Darwin)
  RTOOL_PLATFORM_CFLAGS := -D_DARWIN_C_SOURCE
else
  RTOOL_PLATFORM_CFLAGS :=
endif

$(shell $(HOSTCC) -std=c99 -O2 -DZ7_ST -D_7ZIP_ST -D_POSIX_C_SOURCE=200809L $(RTOOL_PLATFORM_CFLAGS) -o $(RTOOL) $(RTOOL_SOURCES) -lm)
  ifeq ($(wildcard $(RTOOL)),)
    $(error Could not build $(RTOOL) from $(RTOOL_SRC_DIR))
  endif
endif

# $(call have,NAME) returns NAME if that program is found in PATH, else nothing
have = $(if $(strip $(shell $(RTOOL) which $(1))),$(1))

ifeq ($(origin CC),default)
  DETECTED_CC := $(call have,gcc)
  ifeq ($(DETECTED_CC),)
    DETECTED_CC := $(call have,clang)
  endif
  ifeq ($(DETECTED_CC),)
    $(error No C compiler is available (neither GCC nor Clang). Install one or pass it manually with "make CC=<compiler-path>")
  endif
  CC := $(DETECTED_CC)
endif

CC_TARGET := $(shell $(CC) -dumpmachine)
ifeq ($(CC_TARGET),)
  $(error Could not determine the compiler target. Verify that the selected compiler supports "-dumpmachine")
endif

# PS2 export toolchain bootstrap: preserve the existing release download flow,
# but keep the payload in embedded/toolchains so every Rebax build ships it.
TOOLCHAIN_RELEASE_BASE := https://github.com/PS2HomeDeveloper/Rebax-Toolchains/releases/download/1.0.0
TOOLCHAINS_DIR := embedded/toolchains
ifneq ($(findstring android,$(CC_TARGET)),)
  ifneq ($(findstring aarch64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-arm64-v8a.tar.xz
  else ifneq ($(findstring arm,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-armeabi-v7a.tar.xz
  else ifneq ($(findstring x86_64,$(CC_TARGET)),)
    TOOLCHAIN_ASSET := rebax-toolchains-android-x86_64.tar.xz
  else
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
else ifneq ($(findstring aarch64,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-arm64.tar.xz
else ifneq ($(findstring x86_64,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86_64.tar.xz
else ifneq ($(findstring amd64,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86_64.tar.xz
else ifneq ($(findstring i686,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86.tar.xz
else ifneq ($(findstring x86,$(CC_TARGET)),)
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86.tar.xz
else
  TOOLCHAIN_ASSET := rebax-toolchains-linux-x86_64.tar.xz
endif

ifneq ($(findstring mingw,$(CC_TARGET)),)
  TOOLCHAIN_REQUIRED := ps2dev.tar.xz make.exe
else ifneq ($(findstring windows,$(CC_TARGET)),)
  TOOLCHAIN_REQUIRED := ps2dev.tar.xz make.exe
else
  TOOLCHAIN_REQUIRED := ps2dev.tar.xz make
endif
TOOLCHAIN_ASSET_URL := $(TOOLCHAIN_RELEASE_BASE)/$(TOOLCHAIN_ASSET)
TOOLCHAIN_TMP := $(BUILD_DIR)/.$(TOOLCHAIN_ASSET).part
TOOLCHAIN_MKDIR := $(shell $(RTOOL) mkdir $(TOOLCHAINS_DIR))
BUILD_TMP_MKDIR := $(shell $(RTOOL) mkdir $(BUILD_DIR))
TOOLCHAIN_PRESENT := $(notdir $(wildcard $(addprefix $(TOOLCHAINS_DIR)/,$(TOOLCHAIN_REQUIRED))))
TOOLCHAIN_MISSING := $(filter-out $(TOOLCHAIN_PRESENT),$(TOOLCHAIN_REQUIRED))
ifneq ($(strip $(MAKECMDGOALS)),clean)
ifneq ($(TOOLCHAIN_MISSING),)
  $(info [toolchains] missing $(TOOLCHAIN_MISSING); downloading $(TOOLCHAIN_ASSET)...)
  TOOLCHAIN_CURL := $(call have,curl)
  TOOLCHAIN_WGET := $(call have,wget)
  TOOLCHAIN_PRE := $(shell $(RTOOL) rm $(TOOLCHAIN_TMP))
  ifneq ($(TOOLCHAIN_CURL),)
    TOOLCHAIN_DL := $(shell $(TOOLCHAIN_CURL) -fL --retry 3 --connect-timeout 15 -o $(TOOLCHAIN_TMP) $(TOOLCHAIN_ASSET_URL))
  else ifneq ($(TOOLCHAIN_WGET),)
    TOOLCHAIN_DL := $(shell $(TOOLCHAIN_WGET) -O $(TOOLCHAIN_TMP) $(TOOLCHAIN_ASSET_URL))
  else
    $(error [toolchains] curl or wget is required to download the PS2 toolchains)
  endif
  ifneq ($(strip $(.SHELLSTATUS)),)
    ifneq ($(strip $(.SHELLSTATUS)),0)
      $(error [toolchains] could not download $(TOOLCHAIN_ASSET_URL))
    endif
  endif
  ifeq ($(wildcard $(TOOLCHAIN_TMP)),)
    $(error [toolchains] download did not produce $(TOOLCHAIN_TMP))
  endif
  TOOLCHAIN_UNPACK := $(shell $(RTOOL) extract $(TOOLCHAIN_TMP) $(TOOLCHAINS_DIR))
  ifneq ($(strip $(.SHELLSTATUS)),)
    ifneq ($(strip $(.SHELLSTATUS)),0)
      $(error [toolchains] could not extract $(TOOLCHAIN_TMP))
    endif
  endif
  TOOLCHAIN_POST := $(shell $(RTOOL) rm $(TOOLCHAIN_TMP))
  TOOLCHAIN_PRESENT_AFTER := $(notdir $(wildcard $(addprefix $(TOOLCHAINS_DIR)/,$(TOOLCHAIN_REQUIRED))))
  TOOLCHAIN_MISSING_AFTER := $(filter-out $(TOOLCHAIN_PRESENT_AFTER),$(TOOLCHAIN_REQUIRED))
  ifneq ($(TOOLCHAIN_MISSING_AFTER),)
    $(error [toolchains] downloaded package did not provide required files: $(TOOLCHAIN_MISSING_AFTER))
  endif
endif
endif

ENGINE_VERSION := v0.0.1
TARGET_PLATFORM := linux
ifneq ($(findstring android,$(CC_TARGET)),)
  TARGET_PLATFORM := android
else ifneq ($(findstring apple-ios,$(CC_TARGET)),)
  TARGET_PLATFORM := ios
else ifneq ($(findstring apple-darwin,$(CC_TARGET)),)
  TARGET_PLATFORM := macos
else ifneq ($(findstring mingw,$(CC_TARGET)),)
  TARGET_PLATFORM := windows
else ifneq ($(findstring windows,$(CC_TARGET)),)
  TARGET_PLATFORM := windows
endif
TARGET_ARCH := x86
ifneq ($(findstring aarch64,$(CC_TARGET)),)
  TARGET_ARCH := arm64
else ifneq ($(findstring arm64,$(CC_TARGET)),)
  TARGET_ARCH := arm64
else ifneq ($(findstring armv7,$(CC_TARGET)),)
  TARGET_ARCH := armeabi-v7a
else ifneq ($(findstring arm,$(CC_TARGET)),)
  TARGET_ARCH := armeabi-v7a
else ifneq ($(findstring x86_64,$(CC_TARGET)),)
  TARGET_ARCH := x86_64
else ifneq ($(findstring amd64,$(CC_TARGET)),)
  TARGET_ARCH := x86_64
endif
ifeq ($(TARGET_PLATFORM),android)
  ifeq ($(TARGET_ARCH),arm64)
    TARGET_ARCH := arm64-v8a
  endif
endif
TARGET_SUFFIX := $(TARGET_PLATFORM)_$(TARGET_ARCH)
TARGET_NAME := Rebax_Engine_$(ENGINE_VERSION)_$(TARGET_SUFFIX)
ifeq ($(TARGET_PLATFORM),windows)
  TARGET_NAME := $(TARGET_NAME).exe
endif

SRC_DIR := src
EMBEDDED_DIR := embedded
OBJ_DIR := $(BUILD_DIR)/obj
OUTPUT_DIR := $(BUILD_DIR)/output
TARGET := $(OUTPUT_DIR)/$(TARGET_NAME)

ICON_SRC_DIR := $(EMBEDDED_DIR)/resources/images/icons/icons_src
ICON_ATLAS_PNG := $(EMBEDDED_DIR)/resources/images/icons/icons.png
ICON_NAMES_HEADER := $(SRC_DIR)/icon_names.h
ICON_SOURCE_FILES := $(sort $(wildcard $(ICON_SRC_DIR)/*.png))

rwildcard = $(filter-out $(patsubst %/,%,$(wildcard $1*/)),$(wildcard $1$2)) \
            $(foreach d,$(wildcard $1*/),$(call rwildcard,$d,$2))

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
EMBEDDED_FILES := $(sort $(call rwildcard,$(EMBEDDED_DIR)/resources/,*) \
	$(call rwildcard,$(TOOLCHAINS_DIR)/,*) $(ICON_ATLAS_PNG) $(NODE_ARCHIVE))
EMBEDDED_FILES := $(filter-out $(ICON_SRC_DIR)/%,$(EMBEDDED_FILES))
EMBEDDED_OBJS := $(patsubst $(EMBEDDED_DIR)/%,$(OBJ_DIR)/embedded/%.o,$(EMBEDDED_FILES))
OBJS := $(SRC_OBJS) $(EMBEDDED_OBJS)
DEPS := $(SRC_OBJS:.o=.d)

# ============================================================
# Linked libraries
# ============================================================

ifeq ($(origin PKG_CONFIG),undefined)
  PKG_CONFIG := $(call have,pkg-config)
endif

SDL2_CFLAGS :=
SDL2_LIBS   :=


ifneq ($(PKG_CONFIG),)

  SDL2_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2)
  SDL2_LIBS   := $(shell $(PKG_CONFIG) --libs sdl2)


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

$(OBJ_DIR)/xz_embedded.o: CFLAGS += -O3
$(OBJ_DIR)/rebax_fs.o: CFLAGS += -O2

LDLIBS := -lm $(SDL2_LIBS)

# ============================================================
# Explicit runtime libraries to ship
# ============================================================
# These lists are intentionally manual.  System libraries are not listed and
# are never scanned, inferred, or copied automatically.
WINDOWS_RUNTIME_LIBS := SDL2.dll
ANDROID_RUNTIME_LIBS := libSDL2.so
LINUX_RUNTIME_LIBS   := libSDL2-2.0.so.0
MACOS_RUNTIME_LIBS   := libSDL2-2.0.0.dylib
IOS_RUNTIME_LIBS     :=

ifeq ($(TARGET_PLATFORM),windows)
  RUNTIME_LIBS := $(WINDOWS_RUNTIME_LIBS)
else ifeq ($(TARGET_PLATFORM),android)
  RUNTIME_LIBS := $(ANDROID_RUNTIME_LIBS)
else ifeq ($(TARGET_PLATFORM),linux)
  RUNTIME_LIBS := $(LINUX_RUNTIME_LIBS)
else ifeq ($(TARGET_PLATFORM),macos)
  RUNTIME_LIBS := $(MACOS_RUNTIME_LIBS)
else ifeq ($(TARGET_PLATFORM),ios)
  RUNTIME_LIBS := $(IOS_RUNTIME_LIBS)
else
  RUNTIME_LIBS :=
endif

# Derive the runtime search path from the same SDL2 link settings used by the
# target link.  Explicit -L paths retain priority; the compiler's resolved
# linker file covers targets whose SDL2 directory is in the compiler's normal
# target-specific search path (for example Debian multiarch directories).
ifeq ($(TARGET_PLATFORM),windows)
  SDL2_LINKER_NAME := libSDL2.dll.a
else ifeq ($(TARGET_PLATFORM),macos)
  SDL2_LINKER_NAME := libSDL2.dylib
else
  SDL2_LINKER_NAME := libSDL2.so
endif
SDL2_LINKER_FILE := $(shell $(CC) $(LDFLAGS) $(SDL2_LIBS) -print-file-name=$(SDL2_LINKER_NAME) 2>/dev/null)
SDL2_LINKER_DIR := $(if $(findstring /,$(SDL2_LINKER_FILE)),$(dir $(SDL2_LINKER_FILE)),)
SDL2_LINK_DIRS := $(strip $(sort $(patsubst -L%,%,$(filter -L%,$(LDFLAGS) $(SDL2_LIBS) $(LDLIBS)))) $(SDL2_LINKER_DIR))
runtime_path = $(firstword $(foreach d,$(SDL2_LINK_DIRS),$(if $(filter 1,$(shell $(RTOOL) exists "$(d)/$(1)")),$(d)/$(1))))
RUNTIME_MISSING := $(foreach lib,$(RUNTIME_LIBS),$(if $(call runtime_path,$(lib)),,$(lib)))

.PHONY: all clean run generate bundle-runtime-libs gen-icons gen-node-registry gen-node-editor-registry gen-node-archive
all: generate $(TARGET) bundle-runtime-libs
generate: gen-icons gen-node-registry gen-node-editor-registry gen-node-archive

gen-icons: $(ICON_NAMES_HEADER) $(ICON_ATLAS_PNG)
gen-node-registry: $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER)
gen-node-editor-registry: $(NODE_EDITOR_REGISTRY_GENERATED)
gen-node-archive: $(NODE_ARCHIVE)

$(ICON_NAMES_HEADER): $(ICON_SOURCE_FILES) | $(BUILD_DIR)
	$(info ==> Generating $(ICON_NAMES_HEADER) from $(words $(ICON_SOURCE_FILES)) icons)
	@$(RTOOL) icon-names $@ $(ICON_SOURCE_FILES)
$(ICON_ATLAS_PNG): $(ICON_SOURCE_FILES) | $(BUILD_DIR)
	$(info ==> Building icon atlas: $@)
	@$(RTOOL) icon-atlas $@ $(ICON_SOURCE_FILES)
$(NODE_ARCHIVE): $(NODE_SOURCE_ALL_FILES) | $(BUILD_DIR)
	$(info ==> Compressing embedded/nodes ($(words $(NODE_SOURCE_ALL_FILES)) files) into $@)
	@$(RTOOL) pack $@ $(EMBEDDED_DIR) nodes
$(TARGET): $(OBJS) | $(OUTPUT_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
bundle-runtime-libs: $(TARGET)
	@echo "==> Bundling runtime libraries"
	@$(RTOOL) mkdir $(OUTPUT_DIR)/libs
ifneq ($(strip $(RUNTIME_MISSING)),)
	@echo "Warning: runtime library not found: $(RUNTIME_MISSING)"
	@echo "Skipping runtime library bundling."
else
	@$(foreach lib,$(RUNTIME_LIBS),echo "==> Copying $(lib)"; $(RTOOL) cp "$(call runtime_path,$(lib))" "$(OUTPUT_DIR)/libs/$(notdir $(lib))";)
endif
$(BUILD_DIR):
	@$(RTOOL) mkdir $@
$(OUTPUT_DIR):
	@$(RTOOL) mkdir $@
$(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER): $(NODE_SOURCE_FILES) | $(BUILD_DIR)
	$(info ==> Generating node registry headers from $(words $(NODE_SOURCE_FILES)) nodes)
	@$(RTOOL) node-registry $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER) $(NODE_SOURCE_FILES)
$(NODE_EDITOR_REGISTRY_GENERATED): $(NODE_EDITOR_SOURCE_FILES) | $(BUILD_DIR)
	$(info ==> Generating node editor registry from $(words $(NODE_EDITOR_SOURCE_FILES)) files)
	@$(RTOOL) node-editor-registry $@ $(NODE_EDITOR_SOURCE_FILES)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(ICON_NAMES_HEADER) $(NODE_TYPES_HEADER) $(NODE_REGISTRY_GENERATED) $(NODE_EDITOR_REGISTRY_GENERATED)
	@$(RTOOL) mkdir $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
# The bundled "make" binary is named "make" on POSIX and "make.exe" on
# Windows (see TOOLCHAIN_REQUIRED above), but the C code only ever
# declares one fixed symbol, _binary_embedded_toolchains_make_start (see
# src/embedded_resources.h). This explicit rule overrides the generic
# pattern rule below just for that one file, pinning the embedded symbol
# name to "embedded/toolchains/make" regardless of which file backs it
# on the current platform. An explicit rule always takes precedence over
# a pattern rule for the same target, so this only affects this file.
TOOLCHAIN_MAKE_BIN := $(word 2,$(TOOLCHAIN_REQUIRED))
$(OBJ_DIR)/embedded/toolchains/$(TOOLCHAIN_MAKE_BIN).o: $(TOOLCHAINS_DIR)/$(TOOLCHAIN_MAKE_BIN)
	@$(RTOOL) mkdir $(dir $@)
	@$(RTOOL) embed-asm $@.S $< embedded/toolchains/make
	$(CC) -c $@.S -o $@

$(OBJ_DIR)/embedded/%.o: $(EMBEDDED_DIR)/%
	@$(RTOOL) mkdir $(dir $@)
	@$(RTOOL) embed-asm $@.S $<
	$(CC) -c $@.S -o $@
clean:
	@$(RTOOL) rm $(BUILD_DIR) $(NODE_ARCHIVE) $(NODE_EDITOR_REGISTRY_GENERATED) $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER) $(ICON_NAMES_HEADER)
	@$(RTOOL) rm-dir $(EMBEDDED_DIR)/resources/images/icons
run: all
	$(TARGET)

-include $(DEPS)
