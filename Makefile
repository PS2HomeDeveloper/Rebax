BUILD_DIR := build
.DEFAULT_GOAL := all
OUTPUT_ROOT := $(BUILD_DIR)/output
APK_DIR := $(BUILD_DIR)/apk
APK_STAGE := $(APK_DIR)/stage

RTOOL_DIR := rebax_build_tool
RTOOL_SRC_DIR := $(RTOOL_DIR)/src
ifeq ($(OS),Windows_NT)
  RTOOL_EXE := .exe
else
  RTOOL_EXE :=
endif
RTOOL_OBJ_DIR := $(RTOOL_DIR)/obj
RTOOL_BIN_DIR := $(RTOOL_DIR)/bin
RTOOL := $(RTOOL_BIN_DIR)/build_tool$(RTOOL_EXE)
CACHE_DIR := build_cache
ifeq ($(OS),Windows_NT)
  NATIVE = $(subst /,\,$(1))
  RMTREE = if exist $(call NATIVE,$(1)) rmdir /s /q $(call NATIVE,$(1))
  MKTREE = if not exist $(call NATIVE,$(1)) mkdir $(call NATIVE,$(1))
else
  NATIVE = $(1)
  RMTREE = rm -rf $(1)
  MKTREE = mkdir -p $(1)
endif
RTOOL_C_SOURCES := $(wildcard $(RTOOL_SRC_DIR)/*.c)
RTOOL_SOURCES := $(RTOOL_C_SOURCES) $(wildcard $(RTOOL_SRC_DIR)/*.h)
RBX_TOOL_READY ?= 0
RBX_BOOT := 0

ifeq ($(OS),Windows_NT)
  NULL_DEV := nul
else
  NULL_DEV := /dev/null
endif

ifeq ($(RBX_TOOL_READY),0)
  HOSTCC := $(firstword $(foreach c,cc gcc clang,$(if $(shell $(c) -dumpmachine 2>$(NULL_DEV)),$(c))))
endif

ifeq ($(origin HOST_PLATFORM),undefined)
HOST_TRIPLE := $(if $(HOSTCC),$(shell $(HOSTCC) -dumpmachine 2>$(NULL_DEV)))

ifeq ($(OS),Windows_NT)
  HOST_PLATFORM := windows
  WIN_MACHINE := $(or $(PROCESSOR_ARCHITEW6432),$(PROCESSOR_ARCHITECTURE))
  ifneq ($(filter AMD64 amd64 x86_64,$(WIN_MACHINE)),)
    HOST_MACHINE := x86_64
  else ifneq ($(filter ARM64 arm64,$(WIN_MACHINE)),)
    HOST_MACHINE := arm64
  else
    HOST_MACHINE := i686
  endif
else
  UNAME_S := $(shell uname -s 2>$(NULL_DEV))
  UNAME_M := $(shell uname -m 2>$(NULL_DEV))
  UNAME_O := $(shell uname -o 2>$(NULL_DEV))
  ifneq ($(findstring MINGW,$(UNAME_S)),)
    HOST_PLATFORM := windows
  else ifneq ($(findstring MSYS,$(UNAME_S)),)
    HOST_PLATFORM := windows
  else ifneq ($(findstring CYGWIN,$(UNAME_S)),)
    HOST_PLATFORM := windows
  else ifneq ($(findstring Darwin,$(UNAME_S)),)
    ifneq ($(findstring apple-ios,$(HOST_TRIPLE)),)
      HOST_PLATFORM := ios
    else
      HOST_PLATFORM := macos
    endif
  else ifneq ($(findstring Android,$(UNAME_O)),)
    HOST_PLATFORM := android
  else ifneq ($(findstring android,$(HOST_TRIPLE)),)
    HOST_PLATFORM := android
  else ifneq ($(findstring com.termux,$(PREFIX)),)
    HOST_PLATFORM := android
  else
    HOST_PLATFORM := linux
  endif
  HOST_MACHINE := $(if $(UNAME_M),$(UNAME_M),$(HOST_TRIPLE))
endif

HOST_ARCH :=
ifneq ($(findstring aarch64,$(HOST_MACHINE)),)
  HOST_ARCH := arm64
else ifneq ($(findstring arm64,$(HOST_MACHINE)),)
  HOST_ARCH := arm64
else ifneq ($(findstring armv7,$(HOST_MACHINE)),)
  HOST_ARCH := arm
else ifneq ($(findstring armv8l,$(HOST_MACHINE)),)
  HOST_ARCH := arm
else ifneq ($(findstring armv6,$(HOST_MACHINE)),)
  HOST_ARCH := arm
else ifneq ($(findstring x86_64,$(HOST_MACHINE)),)
  HOST_ARCH := x86_64
else ifneq ($(findstring amd64,$(HOST_MACHINE)),)
  HOST_ARCH := x86_64
else ifneq ($(findstring i686,$(HOST_MACHINE)),)
  HOST_ARCH := x86
else ifneq ($(findstring i386,$(HOST_MACHINE)),)
  HOST_ARCH := x86
else ifneq ($(findstring i586,$(HOST_MACHINE)),)
  HOST_ARCH := x86
else ifneq ($(findstring i486,$(HOST_MACHINE)),)
  HOST_ARCH := x86
else
  HOST_ARCH := x86_64
endif

ifeq ($(HOST_PLATFORM),android)
  ifeq ($(HOST_ARCH),arm64)
    HOST_ARCH := arm64-v8a
  endif
endif
endif

ifeq ($(HOST_PLATFORM),android)
  ifeq ($(HOST_ARCH),arm)
    HOST_ARCH := armeabi-v7a
  endif
endif
ifeq ($(HOST_PLATFORM),ios)
  ifeq ($(HOST_ARCH),x86_64)
    HOST_ARCH := x86_64
  endif
endif

ifeq ($(RBX_TOOL_READY),0)
  ifeq ($(HOSTCC),)
    RBX_BOOT := 1
  endif
endif

ifeq ($(RBX_BOOT),1)
  ifneq ($(filter $(HOST_PLATFORM),linux windows android),$(HOST_PLATFORM))
    $(error No C compiler was found. On $(HOST_PLATFORM) install the system compiler first)
  endif
  BOOT_ID := $(HOST_PLATFORM)-$(HOST_ARCH)
  BOOT_SDK := sdks/$(BOOT_ID)
  BOOT_CLANG_ROOT := toolchains/$(HOST_PLATFORM)-$(HOST_ARCH)
  HOSTCC := $(BOOT_CLANG_ROOT)/clang/bin/clang$(RTOOL_EXE)
  BOOT_CLANG_OK := $(BOOT_CLANG_ROOT)/.clang-ok
  BOOT_SDK_OK := $(BOOT_SDK)/.sdk-ok
  BOOT_TRIPLE_linux-x86_64 := x86_64-linux-musl
  BOOT_TRIPLE_linux-arm64 := aarch64-linux-musl
  BOOT_TRIPLE_linux-x86 := i686-linux-musl
  BOOT_TRIPLE_linux-arm := armv7-linux-musleabihf
  BOOT_TRIPLE_windows-x86_64 := x86_64-w64-mingw32
  BOOT_TRIPLE_windows-x86 := i686-w64-mingw32
  BOOT_TRIPLE_windows-arm64 := aarch64-w64-mingw32
  BOOT_TRIPLE_windows-arm := armv7-w64-mingw32
  BOOT_TRIPLE_android-arm64-v8a := aarch64-linux-android21
  BOOT_TRIPLE_android-armeabi-v7a := armv7a-linux-androideabi21
  BOOT_TRIPLE_android-x86 := i686-linux-android21
  BOOT_TRIPLE_android-x86_64 := x86_64-linux-android21
  BOOT_LIBDIR_android-arm64-v8a := aarch64-linux-android
  BOOT_LIBDIR_android-armeabi-v7a := arm-linux-androideabi
  BOOT_LIBDIR_android-x86 := i686-linux-android
  BOOT_LIBDIR_android-x86_64 := x86_64-linux-android
  BOOT_TRIPLE := $(BOOT_TRIPLE_$(BOOT_ID))
  BOOT_LIBDIR := $(BOOT_LIBDIR_$(BOOT_ID))
  ifeq ($(HOST_PLATFORM),linux)
    BOOT_CFLAGS := --target=$(BOOT_TRIPLE) -nostdlibinc -isystem $(BOOT_SDK)/runtime/include -isystem $(BOOT_SDK)/standard/include
    BOOT_LDPRE := --target=$(BOOT_TRIPLE) -nostdlib -static -fuse-ld=lld -Wl,--eh-frame-hdr $(BOOT_SDK)/standard/lib/crt1.o $(BOOT_SDK)/standard/lib/crti.o
    BOOT_LDPOST := -L$(BOOT_SDK)/runtime/lib -L$(BOOT_SDK)/standard/lib -lc -lgcc $(BOOT_SDK)/standard/lib/crtn.o
  else ifeq ($(HOST_PLATFORM),windows)
    BOOT_CFLAGS := --target=$(BOOT_TRIPLE) --sysroot=$(BOOT_SDK)/standard
    BOOT_LDPRE := --target=$(BOOT_TRIPLE) --sysroot=$(BOOT_SDK)/standard -fuse-ld=lld -static -nodefaultlibs
    BOOT_LDPOST = -L$(BOOT_SDK)/runtime/lib $(wildcard $(BOOT_SDK)/runtime/lib/libclang_rt.builtins-*.a) -lmingw32 -lmingwex -lmoldname -lmsvcrt -lkernel32 -ladvapi32 -lshell32 -luser32
  else
    BOOT_CFLAGS := --target=$(BOOT_TRIPLE) --sysroot=$(BOOT_SDK)/standard -fPIC
    BOOT_LDPRE := --target=$(BOOT_TRIPLE) --sysroot=$(BOOT_SDK)/standard -fuse-ld=lld -nodefaultlibs
    BOOT_LDPOST = -L$(BOOT_SDK)/standard/usr/lib/$(BOOT_LIBDIR)/21 -L$(BOOT_SDK)/standard/usr/lib/$(BOOT_LIBDIR) $(wildcard $(BOOT_SDK)/runtime/lib/$(BOOT_LIBDIR)/libclang_rt.builtins-*.a) -lc -ldl
  endif
endif

ifeq ($(RBX_TOOL_READY),0)
  ifeq ($(filter -j%,$(MAKEFLAGS))$(findstring jobserver,$(MAKEFLAGS)),)
    RBX_JOBS := $(or $(NUMBER_OF_PROCESSORS),$(shell nproc 2>$(NULL_DEV)),2)
    MAKEFLAGS += -j$(RBX_JOBS)
  endif
endif

ifeq ($(RBX_TOOL_READY),0)
  ifneq ($(wildcard $(RTOOL)),)
    ifneq ($(strip $(shell $(call NATIVE,$(RTOOL)) ok 2>$(NULL_DEV))),ok)
      $(info The build tool does not run on this machine - rebuilding it)
      $(shell $(call RMTREE,$(RTOOL_OBJ_DIR)))
      $(shell $(call RMTREE,$(RTOOL_BIN_DIR)))
    endif
  endif
endif

TERMUX_PREFIX := $(if $(PREFIX),$(PREFIX),/data/data/com.termux/files/usr)
ENGINE_VERSION := v1.0.0

TARGET_GOALS := linux-x86_64 linux-x86 linux-arm64 linux-arm windows-x86_64 windows-x86 windows-arm64 windows-arm android-arm64-v8a android-armeabi-v7a android-x86 android-x86_64 macos-arm64 macos-x86_64 ios-arm64 ios-x86_64
ALL_TARGET_GOALS := $(TARGET_GOALS) android-arm64
APK_VARIANT_GOALS := $(foreach g,$(ALL_TARGET_GOALS),$(g).APK $(g)-APK) apk APK
GOALS_RAW := $(MAKECMDGOALS)
WANT_APK := 0
ifneq ($(filter %.APK %-APK apk APK,$(GOALS_RAW)),)
  WANT_APK := 1
endif
GOALS := $(filter-out apk APK,$(GOALS_RAW))
GOALS := $(patsubst %.APK,%,$(GOALS))
GOALS := $(patsubst %-APK,%,$(GOALS))
SELECTED_GOAL := $(firstword $(filter $(ALL_TARGET_GOALS),$(GOALS)))
ifeq ($(SELECTED_GOAL),android-arm64)
  SELECTED_GOAL := android-arm64-v8a
endif

TARGET_PLATFORM := $(HOST_PLATFORM)
TARGET_ARCH := $(HOST_ARCH)
ifneq ($(SELECTED_GOAL),)
  TARGET_PLATFORM := $(firstword $(subst -, ,$(SELECTED_GOAL)))
  TARGET_ARCH := $(patsubst $(TARGET_PLATFORM)-%,%,$(SELECTED_GOAL))
endif

TARGET_SUFFIX := $(TARGET_PLATFORM)_$(TARGET_ARCH)
WANT_APK_FORCE := $(filter android,$(TARGET_PLATFORM))
ifneq ($(WANT_APK_FORCE),)
  WANT_APK := 1
endif
PRODUCT := Rebax-Engine-$(ENGINE_VERSION)-$(TARGET_PLATFORM)-$(TARGET_ARCH)
OUTPUT_DIR := $(OUTPUT_ROOT)/$(PRODUCT)
TARGET_NAME := $(PRODUCT)
ifeq ($(TARGET_PLATFORM),windows)
  TARGET_NAME := $(TARGET_NAME).exe
endif
OBJ_DIR := $(BUILD_DIR)/obj/$(TARGET_PLATFORM)_$(TARGET_ARCH)
ENV_DIR := $(BUILD_DIR)/env
ENV_MK := $(ENV_DIR)/$(TARGET_PLATFORM)_$(TARGET_ARCH).mk
TARGET := $(OUTPUT_DIR)/$(TARGET_NAME)
ifeq ($(TARGET_PLATFORM),ios)
  IOS_DIR := $(BUILD_DIR)/ios
  IOS_APP := $(IOS_DIR)/Payload/Rebax.app
  IOS_PKG_EXT := $(if $(filter arm64,$(TARGET_ARCH)),ipa,zip)
  IOS_PKG := $(OUTPUT_ROOT)/$(PRODUCT).$(IOS_PKG_EXT)
  IOS_PLATFORM := $(if $(filter arm64,$(TARGET_ARCH)),iPhoneOS,iPhoneSimulator)
  TARGET := $(IOS_DIR)/$(TARGET_NAME)
endif
APK_OUT := $(OUTPUT_ROOT)/$(PRODUCT).apk

ifeq ($(WANT_APK),1)
  ifneq ($(TARGET_PLATFORM),android)
    $(error The APK build exists for Android targets only; use a goal such as android-arm64-v8a.APK)
  endif
endif

APK_TARGET_SDK := 28
APK_MIN_SDK := 24
APK_PACKAGE := org.rebax.engine
APK_LABEL := Rebax
KEYS_DIR := keystore
TOOLS_FILES := $(wildcard tools/*)

RTOOL_PLATFORM_CFLAGS :=
ifneq ($(filter macos ios,$(HOST_PLATFORM)),)
  RTOOL_PLATFORM_CFLAGS := -D_DARWIN_C_SOURCE
endif

SDL2_EXPECT_ARCH := arm
ifeq ($(TARGET_ARCH),x86_64)
  SDL2_EXPECT_ARCH := x86_64
else ifeq ($(TARGET_ARCH),x86)
  SDL2_EXPECT_ARCH := x86
else ifeq ($(TARGET_ARCH),arm64)
  SDL2_EXPECT_ARCH := arm64
else ifeq ($(TARGET_ARCH),arm64-v8a)
  SDL2_EXPECT_ARCH := arm64
endif

ifeq ($(RBX_TOOL_READY),1)
  include $(ENV_MK)
endif

MBEDTLS_SRCS := $(sort $(wildcard third_party/mbedtls/library/*.c))
RTOOL_THIRD_PARTY := third_party/xz_embedded.c third_party/lzma2_encoder.c $(MBEDTLS_SRCS) $(wildcard src/network/*.c) src/filesystem/rebax_archive.c src/filesystem/rebax_fs_tree.c $(wildcard src/libraries/apk/*.c)
RTOOL_HEADERS := $(wildcard src/network/*.h) $(wildcard src/filesystem/*.h) $(wildcard src/libraries/apk/*.h)
RTOOL_INCLUDES := -Ithird_party -Ithird_party/mbedtls/include -Isrc
RTOOL_LIBS := -lm
ifeq ($(HOST_PLATFORM),windows)
  RTOOL_LIBS += -lws2_32 -lbcrypt
endif
RTOOL_SRC_HEADERS := $(wildcard $(RTOOL_SRC_DIR)/*.h)
RTOOL_ALL_SRCS := $(RTOOL_C_SOURCES) $(RTOOL_THIRD_PARTY)
rtool_obj = $(RTOOL_OBJ_DIR)/o-$(subst /,-,$(1:.c=.o))
RTOOL_OBJS := $(foreach s,$(RTOOL_ALL_SRCS),$(call rtool_obj,$(s)))
RTOOL_CCFLAGS := -std=c99 -O1 -w -DZ7_ST -D_7ZIP_ST -D_POSIX_C_SOURCE=200809L $(RTOOL_PLATFORM_CFLAGS) $(RTOOL_INCLUDES) $(BOOT_CFLAGS)
RTOOL_BOOT_NEED := $(if $(filter 1,$(RBX_BOOT)),$(BOOT_CLANG_OK) $(BOOT_SDK_OK))

define RTOOL_OBJ_RULE
$(call rtool_obj,$(1)): $(1) $(if $(findstring third_party/,$(1)),,$(RTOOL_HEADERS) $(RTOOL_SRC_HEADERS)) | $(RTOOL_OBJ_DIR) $(RTOOL_BOOT_NEED)
	@echo [tool] $(1)
	@$(HOSTCC) $(RTOOL_CCFLAGS) -c $(1) -o $$@
endef
$(foreach s,$(RTOOL_ALL_SRCS),$(eval $(call RTOOL_OBJ_RULE,$(s))))

$(RTOOL_OBJ_DIR) $(RTOOL_BIN_DIR):
	@$(call MKTREE,$@)

$(RTOOL): $(RTOOL_OBJS) | $(RTOOL_BIN_DIR)
	@echo Linking $(RTOOL)
	@$(HOSTCC) $(BOOT_LDPRE) $(RTOOL_OBJS) $(RTOOL_LIBS) $(BOOT_LDPOST) -o $@

ifeq ($(RBX_BOOT),1)
TOOLCHAINS_URL := https://github.com/PS2HomeDeveloper/Rebax-Toolchains/releases/latest/download
SDKS_URL := https://github.com/PS2HomeDeveloper/Rebax-SDKs/releases/latest/download
ifeq ($(OS),Windows_NT)
  BOOT_GET = curl.exe -L --fail -# --create-dirs -o $(1) $(2)
  BOOT_DEL = -del /q $(subst /,\,$(1))
else
  BOOT_GET = curl -L --fail -# --create-dirs -o $(1) $(2)
  BOOT_DEL = -rm -f $(1)
endif

BOOT_CLANG_ARCHIVE := $(firstword $(wildcard $(BOOT_CLANG_ROOT)/rebax-clang-$(BOOT_ID).tar.xz $(BOOT_CLANG_ROOT)/clang.tar.xz))
ifeq ($(BOOT_CLANG_ARCHIVE),)
  BOOT_CLANG_ARCHIVE := $(BOOT_CLANG_ROOT)/clang.tar.xz
  BOOT_CLANG_DOWNLOAD := 1
endif

BOOT_SDK_ARCHIVE := $(firstword $(wildcard sdks/rebax-sdk-$(BOOT_ID).tar.xz sdks/sdk.tar.xz))
ifeq ($(BOOT_SDK_ARCHIVE),)
  BOOT_SDK_ARCHIVE := sdks/sdk.tar.xz
  BOOT_SDK_DOWNLOAD := 1
endif

$(BOOT_CLANG_OK):
	@echo Installing compiler for $(BOOT_ID)
ifeq ($(BOOT_CLANG_DOWNLOAD),1)
	$(call BOOT_GET,$(BOOT_CLANG_ARCHIVE),$(TOOLCHAINS_URL)/rebax-clang-$(BOOT_ID).tar.xz)
endif
	tar -xf $(BOOT_CLANG_ARCHIVE) -C $(BOOT_CLANG_ROOT)
	$(call BOOT_DEL,$(BOOT_CLANG_ARCHIVE))
	@echo ok> $@

$(BOOT_SDK_OK):
	@echo Installing SDK for $(BOOT_ID)
ifeq ($(BOOT_SDK_DOWNLOAD),1)
	$(call BOOT_GET,$(BOOT_SDK_ARCHIVE),$(SDKS_URL)/rebax-sdk-$(BOOT_ID).tar.xz)
endif
	tar -xf $(BOOT_SDK_ARCHIVE) -C sdks
	$(call BOOT_DEL,$(BOOT_SDK_ARCHIVE))
	@echo ok> $@
endif

.PHONY: rebax-build-tool tool
rebax-build-tool tool: $(RTOOL)

SRC_DIR := src
GEN_DIR := $(BUILD_DIR)/generated
RESOURCES_DIR := resources
ICON_SRC_DIR := $(RESOURCES_DIR)/images/icons/icons_src
ICON_ATLAS_DIR := $(RESOURCES_DIR)/images/icons
ICON_NAMES_HEADER := $(GEN_DIR)/icon_names.h
ICON_ATLAS_HEADER := $(GEN_DIR)/icon_atlas_pages.h
ICON_ATLAS_PNGS := $(sort $(wildcard $(ICON_ATLAS_DIR)/icons[0-9]*.png))
ICON_SOURCE_FILES := $(sort $(wildcard $(ICON_SRC_DIR)/*.png))
rwildcard = $(filter-out $(patsubst %/,%,$(wildcard $1*/)),$(wildcard $1$2)) $(foreach d,$(wildcard $1*/),$(call rwildcard,$d,$2))
NODE_SDK_DIR := sdks/rebax/ps2
NODE_SRC_DIR := $(NODE_SDK_DIR)/nodes/src
NODE_EDITOR_SRC_DIR := $(SRC_DIR)/dev_systems/nodes/editor
NODE_SOURCE_FILES := $(sort $(wildcard $(NODE_SRC_DIR)/*.c))
NODE_EDITOR_SOURCE_FILES := $(sort $(wildcard $(NODE_EDITOR_SRC_DIR)/*.c))
NODE_REGISTRY_GENERATED := $(GEN_DIR)/node_registry_generated.h
NODE_EDITOR_REGISTRY_GENERATED := $(GEN_DIR)/node_editor_registry_generated.h
NODE_TYPES_HEADER := $(GEN_DIR)/node_types.h
SRCS := $(call rwildcard,$(SRC_DIR)/,*.c)
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
THIRD_PARTY_SRCS := third_party/xz_embedded.c $(MBEDTLS_SRCS)
CACHE_OBJ := $(CACHE_DIR)/obj/$(TARGET_PLATFORM)_$(TARGET_ARCH)
THIRD_PARTY_OBJS := $(patsubst third_party/%.c,$(CACHE_OBJ)/third_party/%.o,$(THIRD_PARTY_SRCS))
OBJS := $(SRC_OBJS) $(THIRD_PARTY_OBJS)
DEPS := $(SRC_OBJS:.o=.d) $(THIRD_PARTY_OBJS:.o=.d)

.PHONY: all clean clean-tool clean-cache clean-all objects libs engine-objs third-party-objs run generate bundle-runtime-libs bundle-resources bundle-packages gen-icons gen-node-registry gen-node-editor-registry \
  build-selected apk-selected apk-package ios-package \
  $(ALL_TARGET_GOALS) $(APK_VARIANT_GOALS)

$(ALL_TARGET_GOALS) $(APK_VARIANT_GOALS) objects libs: all

SUB_GOAL := $(if $(filter 1,$(WANT_APK)),apk-selected,build-selected)
ifneq ($(filter objects,$(MAKECMDGOALS)),)
  SUB_GOAL := engine-objs
endif
ifneq ($(filter libs,$(MAKECMDGOALS)),)
  SUB_GOAL := third-party-objs
endif

engine-objs: $(SRC_OBJS)
third-party-objs: $(THIRD_PARTY_OBJS)

ifeq ($(OS),Windows_NT)
.PHONY: install-local-packages
install-local-packages:
	@if exist toolchains\\$(TARGET_PLATFORM)-$(TARGET_ARCH)\\rebax-clang-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz (tar -xf toolchains\\$(TARGET_PLATFORM)-$(TARGET_ARCH)\\rebax-clang-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz -C toolchains\\$(TARGET_PLATFORM)-$(TARGET_ARCH) && if not exist $(CACHE_DIR)\\packages mkdir $(CACHE_DIR)\\packages && move /y toolchains\\$(TARGET_PLATFORM $(CACHE_DIR)\\packages\\rebax-clang-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz)-$(TARGET_ARCH)\\rebax-clang-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz) else if exist toolchains\\$(TARGET_PLATFORM)-$(TARGET_ARCH)\\clang.tar.xz (tar -xf toolchains\\$(TARGET_PLATFORM)-$(TARGET_ARCH)\\clang.tar.xz -C toolchains\\$(TARGET_PLATFORM)-$(TARGET_ARCH) && if not exist $(CACHE_DIR)\\packages mkdir $(CACHE_DIR)\\packages && move /y toolchains\\$(TARGET_PLATFORM $(CACHE_DIR)\\packages\\rebax-clang-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz)-$(TARGET_ARCH)\\clang.tar.xz)
	@if exist sdks\\rebax-sdk-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz (tar -xf sdks\\rebax-sdk-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz -C sdks && if not exist $(CACHE_DIR)\\packages mkdir $(CACHE_DIR)\\packages && move /y sdks\\rebax-sdk-$(TARGET_PLATFORM $(CACHE_DIR)\\packages\\rebax-sdk-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz)-$(TARGET_ARCH).tar.xz) else if exist sdks\\$(TARGET_PLATFORM)-$(TARGET_ARCH)\\sdk.tar.xz (tar -xf sdks\\$(TARGET_PLATFORM)-$(TARGET_ARCH)\\sdk.tar.xz -C sdks && if not exist $(CACHE_DIR)\\packages mkdir $(CACHE_DIR)\\packages && move /y sdks\\$(TARGET_PLATFORM $(CACHE_DIR)\\packages\\rebax-sdk-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz)-$(TARGET_ARCH)\\sdk.tar.xz) else if exist sdks\\sdk.tar.xz (tar -xf sdks\\sdk.tar.xz -C sdks && if not exist $(CACHE_DIR)\\packages mkdir $(CACHE_DIR)\\packages && move /y sdks\\sdk.tar.xz $(CACHE_DIR)\\packages\\rebax-sdk-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz)
else
.PHONY: install-local-packages
install-local-packages:
	@clang_archive=""; for f in "toolchains/$(TARGET_PLATFORM)-$(TARGET_ARCH)/rebax-clang-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz" "toolchains/$(TARGET_PLATFORM)-$(TARGET_ARCH)/clang.tar.xz"; do if [ -f "$$f" ]; then clang_archive="$$f"; break; fi; done; if [ -n "$$clang_archive" ]; then echo "Installing local Clang archive: $$clang_archive"; tar -xf "$$clang_archive" -C "toolchains/$(TARGET_PLATFORM)-$(TARGET_ARCH)" && mkdir -p $(CACHE_DIR)/packages && mv -f "$$clang_archive" $(CACHE_DIR)/packages/rebax-clang-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz; fi
	@sdk_archive=""; for f in "sdks/rebax-sdk-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz" "sdks/$(TARGET_PLATFORM)-$(TARGET_ARCH)/sdk.tar.xz" "sdks/sdk.tar.xz"; do if [ -f "$$f" ]; then sdk_archive="$$f"; break; fi; done; if [ -n "$$sdk_archive" ]; then echo "Installing local SDK archive: $$sdk_archive"; tar -xf "$$sdk_archive" -C sdks && mkdir -p $(CACHE_DIR)/packages && mv -f "$$sdk_archive" $(CACHE_DIR)/packages/rebax-sdk-$(TARGET_PLATFORM)-$(TARGET_ARCH).tar.xz; fi
endif

all: $(RTOOL) generate install-local-packages
	@$(RTOOL) mkdir $(ENV_DIR)
	@$(RTOOL) env $(TARGET_PLATFORM) $(TARGET_ARCH) $(HOST_PLATFORM) $(HOST_ARCH) $(ENV_MK)
	+$(MAKE) --no-print-directory RBX_TOOL_READY=1 HOST_PLATFORM=$(HOST_PLATFORM) HOST_ARCH=$(HOST_ARCH) HOST_TRIPLE=$(HOST_TRIPLE) TARGET_PLATFORM=$(TARGET_PLATFORM) TARGET_ARCH=$(TARGET_ARCH) WANT_APK=$(WANT_APK) $(SUB_GOAL)

build-selected: $(TARGET) bundle-runtime-libs bundle-resources bundle-packages $(if $(filter ios,$(TARGET_PLATFORM)),ios-package)

bundle-packages: $(TARGET) | $(RTOOL)
	$(if $(filter ios,$(TARGET_PLATFORM)),,@$(RTOOL) copy-tree $(SDK_DIR) $(OUTPUT_DIR)/sdks/$(notdir $(SDK_DIR)))
	$(if $(filter ios,$(TARGET_PLATFORM)),,@$(RTOOL) copy-tree sdks/rebax $(OUTPUT_DIR)/sdks/rebax)
	$(if $(filter ios,$(TARGET_PLATFORM)),,@$(RTOOL) copy-tree $(CLANG_DIR) $(OUTPUT_DIR)/toolchains/clang)
	$(if $(or $(filter ios,$(TARGET_PLATFORM)),$(TOOLS_FILES)),,@$(RTOOL) mkdir $(OUTPUT_DIR)/tools)
	$(if $(and $(TOOLS_FILES),$(filter-out ios,$(TARGET_PLATFORM))),@$(RTOOL) copy-tree tools $(OUTPUT_DIR)/tools)

ios-package: $(TARGET) | $(RTOOL)
	@$(RTOOL) rm $(IOS_DIR)/Payload
	@$(RTOOL) cp $(TARGET) $(IOS_APP)/$(TARGET_NAME)
	@$(RTOOL) copy-dir $(RESOURCES_DIR) $(IOS_APP)/resources --skip icons_src
	@$(RTOOL) ios-plist $(IOS_APP)/Info.plist $(ENGINE_VERSION) $(TARGET_NAME) $(IOS_PLATFORM)
	@$(RTOOL) zip-dir $(IOS_PKG) $(IOS_DIR) Payload --exec $(TARGET_NAME)

apk-selected: apk-package

generate: gen-icons gen-node-registry gen-node-editor-registry
gen-icons: | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) mkdir $(GEN_DIR)
	@$(RTOOL) icon-names $(ICON_NAMES_HEADER) $(ICON_SOURCE_FILES)
	@$(RTOOL) icon-atlas-pages $(ICON_ATLAS_DIR) $(ICON_ATLAS_HEADER) $(ICON_SOURCE_FILES)
gen-node-registry: $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER)
gen-node-editor-registry: $(NODE_EDITOR_REGISTRY_GENERATED)

$(ICON_NAMES_HEADER): $(ICON_SOURCE_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) mkdir $(GEN_DIR)
	@$(RTOOL) icon-names $@ $(ICON_SOURCE_FILES)

$(TARGET): $(OBJS) | $(RTOOL) $(OUTPUT_DIR)
	$(if $(filter ios,$(TARGET_PLATFORM)),@$(RTOOL) mkdir $(IOS_DIR))
	$(TARGET_CC) $(TARGET_LDPRE) $(OBJS) $(TARGET_LDPOST) -o $@

bundle-runtime-libs: $(TARGET) | $(RTOOL)
	$(if $(and $(filter windows,$(TARGET_PLATFORM)),$(if $(wildcard $(SDK_DIR)/graphics/lib/libSDL2.a),,1)),@$(RTOOL) cp $(SDK_DIR)/graphics/bin/SDL2.dll $(OUTPUT_DIR)/SDL2.dll)

bundle-resources: $(TARGET) | $(RTOOL)
	@$(RTOOL) copy-dir $(RESOURCES_DIR) $(OUTPUT_DIR)/resources --skip icons_src

$(BUILD_DIR): | $(RTOOL)
	@$(RTOOL) mkdir $@
$(OUTPUT_DIR): | $(RTOOL)
	@$(RTOOL) mkdir $@
$(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER): $(NODE_SOURCE_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) mkdir $(GEN_DIR)
	@$(RTOOL) node-registry $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER) $(NODE_SOURCE_FILES)
$(NODE_EDITOR_REGISTRY_GENERATED): $(NODE_EDITOR_SOURCE_FILES) | $(RTOOL) $(BUILD_DIR)
	@$(RTOOL) mkdir $(GEN_DIR)
	@$(RTOOL) node-editor-registry $@ $(NODE_EDITOR_SOURCE_FILES)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(ICON_NAMES_HEADER) $(NODE_TYPES_HEADER) $(NODE_REGISTRY_GENERATED) $(NODE_EDITOR_REGISTRY_GENERATED) | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	$(TARGET_CC) $(TARGET_CFLAGS) $(CFLAGS) -c $< -o $@
$(CACHE_OBJ)/third_party/%.o: third_party/%.c | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	$(TARGET_CC) $(TARGET_CFLAGS) $(CFLAGS) -c $< -o $@
clean:
	$(if $(wildcard $(RTOOL)),$(RTOOL) rm $(BUILD_DIR) $(NODE_EDITOR_REGISTRY_GENERATED) $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER) $(ICON_NAMES_HEADER) $(ICON_ATLAS_HEADER) $(ICON_ATLAS_PNGS))
	@$(call RMTREE,$(BUILD_DIR))
clean-tool:
	@$(call RMTREE,$(RTOOL_OBJ_DIR))
	@$(call RMTREE,$(RTOOL_BIN_DIR))
clean-cache:
	@$(call RMTREE,$(CACHE_DIR))
clean-all: clean clean-cache clean-tool
run: all
	$(TARGET)
-include $(DEPS)

APK_LIB_DIR := $(APK_STAGE)/lib/$(TARGET_ARCH)
APK_ASSETS := $(APK_STAGE)/assets
SDL2_LIB_FILE := $(SDK_DIR)/graphics/lib/libSDL2.so
ICONV_LIB_FILE := $(SDK_DIR)/graphics/lib/libiconv.so
CXX_SHARED_LIB_FILE := $(SDK_DIR)/runtime/lib/$(TARGET_LIBDIR)/libc++_shared.so

DEX_FILE := $(SDK_DIR)/graphics/dex/classes.dex

apk-package: $(OBJS)
	@$(RTOOL) mkdir $(APK_DIR) $(OUTPUT_ROOT)
	@$(RTOOL) rm $(APK_STAGE)
	@$(RTOOL) mkdir $(APK_STAGE) $(APK_LIB_DIR)
	@echo Packaging $(APK_OUT)
	@$(RTOOL) require-lib SDL2 $(SDL2_EXPECT_ARCH) $(SDL2_LIB_FILE)
	@$(RTOOL) require-lib libiconv $(SDL2_EXPECT_ARCH) $(ICONV_LIB_FILE)
	@$(RTOOL) require-lib libc++_shared $(SDL2_EXPECT_ARCH) $(CXX_SHARED_LIB_FILE)
	$(if $(wildcard $(DEX_FILE)),,$(error classes.dex was not found in $(SDK_DIR)/graphics/dex - download the latest Android SDK package))
	@$(RTOOL) cp $(SDL2_LIB_FILE) $(APK_LIB_DIR)/libSDL2.so
	@$(RTOOL) cp $(ICONV_LIB_FILE) $(APK_LIB_DIR)/libiconv.so
	@$(RTOOL) cp $(CXX_SHARED_LIB_FILE) $(APK_LIB_DIR)/libc++_shared.so
	@$(RTOOL) cp $(DEX_FILE) $(APK_STAGE)/classes.dex
	@echo Copying assets
	@$(RTOOL) copy-dir $(RESOURCES_DIR) $(APK_ASSETS)/resources --skip icons_src
	@$(RTOOL) package-archive toolchains rebax-clang-android-$(TARGET_ARCH).tar.xz $(CACHE_DIR)/packages/rebax-clang-android-$(TARGET_ARCH).tar.xz
	@$(RTOOL) package-archive sdks rebax-sdk-$(notdir $(SDK_DIR)).tar.xz $(CACHE_DIR)/packages/rebax-sdk-$(notdir $(SDK_DIR)).tar.xz
	@$(RTOOL) cp $(CACHE_DIR)/packages/rebax-clang-android-$(TARGET_ARCH).tar.xz $(APK_ASSETS)/packages/clang.tar.xz
	@$(RTOOL) cp $(CACHE_DIR)/packages/rebax-sdk-$(notdir $(SDK_DIR)).tar.xz $(APK_ASSETS)/packages/sdk-$(notdir $(SDK_DIR)).tar.xz
	@$(RTOOL) copy-dir sdks/rebax $(APK_ASSETS)/sdks/rebax
	$(if $(TOOLS_FILES),@$(RTOOL) copy-dir tools $(APK_ASSETS)/tools)
	@$(RTOOL) write-text $(APK_ASSETS)/packages/stamp.txt $(ENGINE_VERSION)-@time@
	@$(RTOOL) asset-manifest $(APK_ASSETS) $(APK_ASSETS)/rebax_assets.txt
	@echo Linking $(APK_LIB_DIR)/libmain.so
	@$(RTOOL) android-entry $(APK_DIR)/rebax_android_entry.c
	$(TARGET_CC) $(TARGET_CFLAGS) -c $(APK_DIR)/rebax_android_entry.c -o $(APK_DIR)/rebax_android_entry.o
	$(TARGET_CC) $(TARGET_LDPRE) -shared -o $(APK_LIB_DIR)/libmain.so $(OBJS) $(APK_DIR)/rebax_android_entry.o $(TARGET_LDPOST)
	@$(RTOOL) apk-pack $(APK_STAGE) $(APK_OUT) $(KEYS_DIR) $(APK_PACKAGE) $(APK_LABEL) $(patsubst v%,%,$(ENGINE_VERSION)) 1 $(APK_MIN_SDK) $(APK_TARGET_SDK)
	@$(RTOOL) rm $(APK_STAGE)
	@echo APK ready: $(APK_OUT)

CFLAGS := -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I$(SRC_DIR) $(addprefix -I,$(sort $(dir $(call rwildcard,$(NODE_SDK_DIR)/,*.h)))) -Ithird_party -Ithird_party/mbedtls/include -I$(GEN_DIR) -MMD -MP
ifeq ($(TARGET_PLATFORM),android)
  CFLAGS += -fPIC
endif
$(CACHE_OBJ)/third_party/xz_embedded.o: CFLAGS += -O3
$(CACHE_OBJ)/third_party/mbedtls/%.o: CFLAGS += -w -O2
$(OBJ_DIR)/filesystem/rebax_fs.o: CFLAGS += -O2
