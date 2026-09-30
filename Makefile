BUILD_DIR := build
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
RTOOL_SOURCES := $(wildcard $(RTOOL_SRC_DIR)/*.c)

HOSTCC ?= cc
ifeq ($(origin CC),default)
  CC := gcc
endif
ifneq ($(strip $(REBAX_CC)),)
  CC := $(REBAX_CC)
endif

RTOOL_PLATFORM_CFLAGS :=
ifeq ($(REBAX_TARGET_PLATFORM),macos)
  RTOOL_PLATFORM_CFLAGS := -D_DARWIN_C_SOURCE
else ifeq ($(REBAX_TARGET_PLATFORM),ios)
  RTOOL_PLATFORM_CFLAGS := -D_DARWIN_C_SOURCE
endif

$(RTOOL): $(RTOOL_SOURCES)
	@echo "==> Building $(RTOOL) with $(HOSTCC)"
	$(HOSTCC) -std=c99 -O2 -DZ7_ST -D_7ZIP_ST -D_POSIX_C_SOURCE=200809L $(RTOOL_PLATFORM_CFLAGS) -o $@ $(RTOOL_SOURCES) -lm

.PHONY: rebax-build-tool
rebax-build-tool: $(RTOOL)

ENGINE_VERSION := v0.0.1
TARGET_PLATFORM ?= linux
TARGET_ARCH ?= x86_64
ifneq ($(strip $(REBAX_TARGET_PLATFORM)),)
  TARGET_PLATFORM := $(REBAX_TARGET_PLATFORM)
endif
ifneq ($(strip $(REBAX_TARGET_ARCH)),)
  TARGET_ARCH := $(REBAX_TARGET_ARCH)
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
TARGET := $(OUTPUT_DIR)/$(TARGET_NAME)

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
TOOLCHAIN_STAMP := $(TOOLCHAINS_DIR)/.rebax-$(TARGET_PLATFORM)-$(TARGET_ARCH).ready
TOOLCHAIN_TMP := $(BUILD_DIR)/.$(TOOLCHAIN_ASSET).part
TOOLCHAIN_ASSET_URL := $(TOOLCHAIN_RELEASE_BASE)/$(TOOLCHAIN_ASSET)

$(TOOLCHAIN_STAMP): $(RTOOL) | $(BUILD_DIR)
	@echo "==> Preparing PS2 toolchain $(TOOLCHAIN_ASSET)"
	@$(RTOOL) mkdir $(TOOLCHAINS_DIR)
	@$(RTOOL) rm $(TOOLCHAIN_TMP)
	@curl -fL --retry 3 --connect-timeout 15 -o $(TOOLCHAIN_TMP) $(TOOLCHAIN_ASSET_URL) || wget -O $(TOOLCHAIN_TMP) $(TOOLCHAIN_ASSET_URL)
	@$(RTOOL) extract $(TOOLCHAIN_TMP) $(TOOLCHAINS_DIR)
	@$(RTOOL) rm $(TOOLCHAIN_TMP)
	@$(RTOOL) cp $(word 1,$(TOOLCHAIN_REQUIRED)) $@

$(TOOLCHAIN_REQUIRED): $(TOOLCHAIN_STAMP)

SRC_DIR := src
EMBEDDED_DIR := embedded
ICON_SRC_DIR := $(EMBEDDED_DIR)/resources/images/icons/icons_src
ICON_ATLAS_PNG := $(EMBEDDED_DIR)/resources/images/icons/icons.png
ICON_NAMES_HEADER := $(SRC_DIR)/icon_names.h
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
EMBEDDED_FILES := $(sort $(filter-out $(ICON_SRC_DIR)/%,$(EMBEDDED_RESOURCE_FILES)) $(TOOLCHAIN_REQUIRED) $(ICON_ATLAS_PNG) $(NODE_ARCHIVE))
EMBEDDED_OBJS := $(patsubst $(EMBEDDED_DIR)/%,$(OBJ_DIR)/embedded/%.o,$(EMBEDDED_FILES))
OBJS := $(SRC_OBJS) $(EMBEDDED_OBJS)
DEPS := $(SRC_OBJS:.o=.d)

SDL2_ROOT ?=
ifneq ($(strip $(SDL2_ROOT)),)
  SDL2_CFLAGS := -I$(SDL2_ROOT)/include -I$(SDL2_ROOT)/include/SDL2
  SDL2_LIBS := -L$(SDL2_ROOT)/lib -lSDL2
else
  SDL2_CFLAGS := -I/usr/include/SDL2
  SDL2_LIBS := -lSDL2
endif

CFLAGS := -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I$(SRC_DIR) -MMD -MP $(SDL2_CFLAGS) $(ARCHIVE_CFLAGS)
$(OBJ_DIR)/xz_embedded.o: CFLAGS += -O3
$(OBJ_DIR)/rebax_fs.o: CFLAGS += -O2
LDLIBS := -lm $(SDL2_LIBS)

WINDOWS_RUNTIME_LIBS := SDL2.dll
ANDROID_RUNTIME_LIBS := libSDL2.so
LINUX_RUNTIME_LIBS := libSDL2-2.0.so.0
MACOS_RUNTIME_LIBS := libSDL2-2.0.0.dylib
ifeq ($(TARGET_PLATFORM),windows)
  RUNTIME_LIBS := $(WINDOWS_RUNTIME_LIBS)
else ifeq ($(TARGET_PLATFORM),android)
  RUNTIME_LIBS := $(ANDROID_RUNTIME_LIBS)
else ifeq ($(TARGET_PLATFORM),linux)
  RUNTIME_LIBS := $(LINUX_RUNTIME_LIBS)
else ifeq ($(TARGET_PLATFORM),macos)
  RUNTIME_LIBS := $(MACOS_RUNTIME_LIBS)
else
  RUNTIME_LIBS :=
endif
ifeq ($(TARGET_PLATFORM),windows)
  SDL2_RUNTIME_DIR ?= $(SDL2_ROOT)/bin
else
  SDL2_RUNTIME_DIR ?= $(SDL2_ROOT)/lib
endif

.PHONY: all clean run generate bundle-runtime-libs gen-icons gen-node-registry gen-node-editor-registry gen-node-archive
all: $(RTOOL) $(TOOLCHAIN_REQUIRED) generate $(TARGET) bundle-runtime-libs

generate: gen-icons gen-node-registry gen-node-editor-registry gen-node-archive
gen-icons: $(ICON_NAMES_HEADER) $(ICON_ATLAS_PNG)
gen-node-registry: $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER)
gen-node-editor-registry: $(NODE_EDITOR_REGISTRY_GENERATED)
gen-node-archive: $(NODE_ARCHIVE)

$(ICON_NAMES_HEADER): $(ICON_SOURCE_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) icon-names $@ $(ICON_SOURCE_FILES)
$(ICON_ATLAS_PNG): $(ICON_SOURCE_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) icon-atlas $@ $(ICON_SOURCE_FILES)
$(NODE_ARCHIVE): $(NODE_SOURCE_ALL_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) pack $@ $(EMBEDDED_DIR) nodes

$(TARGET): $(OBJS) | $(RTOOL) $(OUTPUT_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

bundle-runtime-libs: $(TARGET) | $(RTOOL)
	@$(RTOOL) mkdir $(OUTPUT_DIR)/libs
	@for lib in $(RUNTIME_LIBS); do \
		if $(RTOOL) exists "$(SDL2_RUNTIME_DIR)/$$lib"; then \
			echo "==> Copying $$lib"; \
			$(RTOOL) cp "$(SDL2_RUNTIME_DIR)/$$lib" "$(OUTPUT_DIR)/libs/$$lib"; \
		elif $(RTOOL) exists "$(SDL2_ROOT)/$$lib"; then \
			echo "==> Copying $$lib"; \
			$(RTOOL) cp "$(SDL2_ROOT)/$$lib" "$(OUTPUT_DIR)/libs/$$lib"; \
		else \
			echo "Warning: runtime library not found: $$lib"; \
		fi; \
	done

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
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/embedded/toolchains/$(TOOLCHAIN_MAKE_BIN).o: $(TOOLCHAINS_DIR)/$(TOOLCHAIN_MAKE_BIN) | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	@$(RTOOL) embed-asm $@.S $< embedded/toolchains/make
	$(CC) -c $@.S -o $@

$(OBJ_DIR)/embedded/%.o: $(EMBEDDED_DIR)/% | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	@$(RTOOL) embed-asm $@.S $<
	$(CC) -c $@.S -o $@

clean: | $(RTOOL)
	@$(RTOOL) rm $(BUILD_DIR) $(NODE_ARCHIVE) $(NODE_EDITOR_REGISTRY_GENERATED) $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER) $(ICON_NAMES_HEADER)
	@$(RTOOL) rm-dir $(EMBEDDED_DIR)/resources/images/icons
run: all
	$(TARGET)

-include $(DEPS)
