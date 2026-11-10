BUILD_DIR := build
.DEFAULT_GOAL := all
OUTPUT_DIR := $(BUILD_DIR)/output
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
CACHE_DIR := build-cache
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
  BOOT_CLANG_ROOT := toolchains/$(HOST_PLATFORM)/$(HOST_ARCH)
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
TARGET_NAME := Rebax_Engine_$(ENGINE_VERSION)_$(TARGET_SUFFIX)
ifeq ($(TARGET_PLATFORM),windows)
  TARGET_NAME := $(TARGET_NAME).exe
endif
OBJ_DIR := $(BUILD_DIR)/obj/$(TARGET_PLATFORM)-$(TARGET_ARCH)
ENV_MK := $(BUILD_DIR)/env-$(TARGET_PLATFORM)-$(TARGET_ARCH).mk
TARGET := $(OUTPUT_DIR)/$(TARGET_NAME)
ifeq ($(TARGET_PLATFORM),ios)
  IOS_DIR := $(BUILD_DIR)/ios
  IOS_APP := $(IOS_DIR)/Payload/Rebax.app
  IOS_PKG_EXT := $(if $(filter arm64,$(TARGET_ARCH)),ipa,zip)
  IOS_PKG := $(OUTPUT_DIR)/Rebax_Engine_$(ENGINE_VERSION)_ios_$(TARGET_ARCH).$(IOS_PKG_EXT)
  IOS_PLATFORM := $(if $(filter arm64,$(TARGET_ARCH)),iPhoneOS,iPhoneSimulator)
  TARGET := $(IOS_DIR)/$(TARGET_NAME)
endif
APK_OUT := $(OUTPUT_DIR)/Rebax_Engine_$(ENGINE_VERSION)_android_$(TARGET_ARCH).apk

ifeq ($(WANT_APK),1)
  ifneq ($(TARGET_PLATFORM),android)
    $(error The APK build exists for Android targets only; use a goal such as android-arm64-v8a.APK)
  endif
endif

ANDROID_SDK_ROOTS := $(ANDROID_HOME) $(ANDROID_SDK_ROOT) $(ANDROID_SDK) $(HOME)/Android/Sdk $(HOME)/Library/Android/sdk $(HOME)/.android/sdk $(TERMUX_PREFIX)/opt/android-sdk $(TERMUX_PREFIX)/share/android-sdk /opt/android-sdk /opt/android /usr/lib/android-sdk /usr/local/lib/android/sdk
ANDROID_SDK_ROOT := $(lastword $(sort $(wildcard $(ANDROID_SDK_ROOTS))))
ANDROID_BUILD_TOOLS := $(lastword $(sort $(wildcard $(ANDROID_SDK_ROOT)/build-tools/*)))
ANDROID_PLATFORM_DIR := $(lastword $(sort $(wildcard $(ANDROID_SDK_ROOT)/platforms/*)))
ANDROID_JAR := $(if $(ANDROID_PLATFORM_DIR),$(ANDROID_PLATFORM_DIR)/android.jar,$(firstword $(wildcard $(ANDROID_SDK_ROOT)/platforms/android-*/android.jar)))
APK_TARGET_SDK := 28

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
RTOOL_THIRD_PARTY := third_party/xz_embedded.c third_party/lzma2_encoder.c $(MBEDTLS_SRCS) $(wildcard src/network/*.c) src/filesystem/rebax_archive.c src/filesystem/rebax_fs_tree.c
RTOOL_HEADERS := $(wildcard src/network/*.h) $(wildcard src/filesystem/*.h)
RTOOL_INCLUDES := -Ithird_party -Ithird_party/mbedtls/include -Isrc/network -Isrc/filesystem
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

$(BOOT_CLANG_OK):
	@echo No C compiler found - downloading the compiler for $(BOOT_ID)
	$(call BOOT_GET,$(BOOT_CLANG_ROOT)/clang.tar.xz,$(TOOLCHAINS_URL)/rebax-clang-$(BOOT_ID).tar.xz)
	tar -xf $(BOOT_CLANG_ROOT)/clang.tar.xz -C $(BOOT_CLANG_ROOT)
	$(call BOOT_DEL,$(BOOT_CLANG_ROOT)/clang.tar.xz)
	@echo ok> $@

$(BOOT_SDK_OK):
	@echo Downloading the SDK for $(BOOT_ID)
	$(call BOOT_GET,sdks/sdk.tar.xz,$(SDKS_URL)/rebax-sdk-$(BOOT_ID).tar.xz)
	tar -xf sdks/sdk.tar.xz -C sdks
	$(call BOOT_DEL,sdks/sdk.tar.xz)
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
NODE_SRC_DIR := sdks/rebax/ps2/src/nodes
NODE_EDITOR_SRC_DIR := $(SRC_DIR)/editor/nodes
NODE_SOURCE_FILES := $(sort $(wildcard $(NODE_SRC_DIR)/*.c))
NODE_EDITOR_SOURCE_FILES := $(sort $(wildcard $(NODE_EDITOR_SRC_DIR)/*.c))
NODE_REGISTRY_GENERATED := $(GEN_DIR)/node_registry_generated.h
NODE_EDITOR_REGISTRY_GENERATED := $(GEN_DIR)/node_editor_registry_generated.h
NODE_TYPES_HEADER := $(GEN_DIR)/node_types.h
SRCS := $(call rwildcard,$(SRC_DIR)/,*.c)
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
THIRD_PARTY_SRCS := third_party/xz_embedded.c $(MBEDTLS_SRCS)
CACHE_OBJ := $(CACHE_DIR)/obj/$(TARGET_PLATFORM)-$(TARGET_ARCH)
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

all: $(RTOOL) generate
	@$(RTOOL) env $(TARGET_PLATFORM) $(TARGET_ARCH) $(HOST_PLATFORM) $(HOST_ARCH) $(ENV_MK)
	+$(MAKE) --no-print-directory RBX_TOOL_READY=1 HOST_PLATFORM=$(HOST_PLATFORM) HOST_ARCH=$(HOST_ARCH) HOST_TRIPLE=$(HOST_TRIPLE) TARGET_PLATFORM=$(TARGET_PLATFORM) TARGET_ARCH=$(TARGET_ARCH) WANT_APK=$(WANT_APK) $(SUB_GOAL)

build-selected: $(TARGET) bundle-runtime-libs bundle-resources bundle-packages $(if $(filter ios,$(TARGET_PLATFORM)),ios-package)

bundle-packages: $(TARGET) | $(RTOOL)
	$(if $(filter ios,$(TARGET_PLATFORM)),,@$(RTOOL) copy-tree $(SDK_DIR) $(OUTPUT_DIR)/sdks/$(notdir $(SDK_DIR)))
	$(if $(filter ios,$(TARGET_PLATFORM)),,@$(RTOOL) copy-tree sdks/rebax $(OUTPUT_DIR)/sdks/rebax)
	$(if $(filter ios,$(TARGET_PLATFORM)),,@$(RTOOL) copy-tree $(CLANG_DIR) $(OUTPUT_DIR)/toolchains/clang)

ios-package: $(TARGET) | $(RTOOL)
	@$(RTOOL) rm $(IOS_DIR)/Payload
	@$(RTOOL) cp $(TARGET) $(IOS_APP)/$(TARGET_NAME)
	@$(RTOOL) copy-dir $(RESOURCES_DIR) $(IOS_APP)/resources --skip icons_src
	@$(RTOOL) ios-plist $(IOS_APP)/Info.plist $(ENGINE_VERSION) $(TARGET_NAME) $(IOS_PLATFORM)
	@$(RTOOL) zip-dir $(IOS_PKG) $(IOS_DIR) Payload --exec $(TARGET_NAME)

apk-selected: $(TARGET) apk-package

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
	$(if $(filter windows,$(TARGET_PLATFORM)),@$(RTOOL) cp $(SDK_DIR)/graphics/bin/SDL2.dll $(OUTPUT_DIR)/SDL2.dll)

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

define rbx_which
$(if $(filter 1,$(RBX_TOOL_READY)),$(shell $(RTOOL) which $(1)))
endef

KEYSTORE ?= $(CURDIR)/rebax-debug.keystore
KEYSTORE_PASS ?= android
KEY_ALIAS ?= rebax
KEY_PASS ?= $(KEYSTORE_PASS)
KEY_DNAME := CN=Rebax,O=Rebax,C=US
APK_LIB_DIR := $(APK_STAGE)/lib/$(TARGET_ARCH)
APK_ASSETS := $(APK_STAGE)/assets
SDL2_LIB_FILE := $(SDK_DIR)/graphics/lib/libSDL2.so
ICONV_LIB_FILE := $(SDK_DIR)/graphics/lib/libiconv.so
CXX_SHARED_LIB_FILE := $(SDK_DIR)/runtime/lib/$(TARGET_LIBDIR)/libc++_shared.so

ifeq ($(WANT_APK),1)
  SDL2_JAVA_SOURCES := $(sort $(wildcard $(SDK_DIR)/graphics/java/org/libsdl/app/*.java))
  AAPT2 := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/aapt2 $(ANDROID_BUILD_TOOLS)/aapt2.exe $(ANDROID_BUILD_TOOLS)/aapt2.bat) $(call rbx_which,aapt2))
  AAPT := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/aapt $(ANDROID_BUILD_TOOLS)/aapt.exe $(ANDROID_BUILD_TOOLS)/aapt.bat) $(call rbx_which,aapt))
  ZIPALIGN := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/zipalign $(ANDROID_BUILD_TOOLS)/zipalign.exe) $(call rbx_which,zipalign))
  APKSIGNER := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/apksigner $(ANDROID_BUILD_TOOLS)/apksigner.bat) $(call rbx_which,apksigner))
  D8 := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/d8 $(ANDROID_BUILD_TOOLS)/d8.bat) $(call rbx_which,d8))
  DX := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/dx $(ANDROID_BUILD_TOOLS)/dx.bat) $(call rbx_which,dx))
  D8_JAR := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/lib/d8.jar))
  JAVAC := $(firstword $(wildcard $(JAVA_HOME)/bin/javac $(JAVA_HOME)/bin/javac.exe $(TERMUX_PREFIX)/opt/openjdk/bin/javac) $(call rbx_which,javac))
  JAVA := $(firstword $(wildcard $(JAVA_HOME)/bin/java $(JAVA_HOME)/bin/java.exe $(TERMUX_PREFIX)/opt/openjdk/bin/java) $(call rbx_which,java))
  JAR := $(firstword $(wildcard $(JAVA_HOME)/bin/jar $(JAVA_HOME)/bin/jar.exe $(TERMUX_PREFIX)/opt/openjdk/bin/jar) $(call rbx_which,jar))
  KEYTOOL := $(firstword $(wildcard $(JAVA_HOME)/bin/keytool $(JAVA_HOME)/bin/keytool.exe $(TERMUX_PREFIX)/opt/openjdk/bin/keytool) $(call rbx_which,keytool))
  JARSIGNER := $(firstword $(wildcard $(JAVA_HOME)/bin/jarsigner $(JAVA_HOME)/bin/jarsigner.exe $(TERMUX_PREFIX)/opt/openjdk/bin/jarsigner) $(call rbx_which,jarsigner))
  KS_MISSING := $(if $(wildcard $(KEYSTORE)),,1)
  CAN_SIGN := $(if $(or $(wildcard $(KEYSTORE)),$(KEYTOOL)),1)
  APK_SIGNER_KIND := $(if $(CAN_SIGN),$(if $(APKSIGNER),apksigner,$(if $(JARSIGNER),jarsigner)))
endif

apk-package: $(OBJS)
	@$(RTOOL) mkdir $(APK_DIR) $(OUTPUT_DIR)
	@$(RTOOL) rm $(APK_STAGE)
	@$(RTOOL) mkdir $(APK_STAGE) $(APK_LIB_DIR) $(APK_STAGE)/classes $(APK_STAGE)/dex
	@echo Packaging $(APK_OUT)
	@$(RTOOL) require-lib SDL2 $(SDL2_EXPECT_ARCH) $(SDL2_LIB_FILE)
	@$(RTOOL) require-lib libiconv $(SDL2_EXPECT_ARCH) $(ICONV_LIB_FILE)
	@$(RTOOL) require-lib libc++_shared $(SDL2_EXPECT_ARCH) $(CXX_SHARED_LIB_FILE)
	$(if $(SDL2_JAVA_SOURCES),,$(error The SDL2 Java sources were not found in $(SDK_DIR)/graphics/java - rebuild the Android SDK package))
	$(if $(JAVAC),,$(error javac was not found - install a JDK to build the APK))
	$(if $(JAR),,$(error jar was not found - install a JDK to build the APK))
	$(if $(wildcard $(ANDROID_JAR)),,$(error android.jar was not found - install an Android SDK platform to build the APK))
	@$(RTOOL) cp $(SDL2_LIB_FILE) $(APK_LIB_DIR)/libSDL2.so
	@$(RTOOL) cp $(ICONV_LIB_FILE) $(APK_LIB_DIR)/libiconv.so
	@$(RTOOL) cp $(CXX_SHARED_LIB_FILE) $(APK_LIB_DIR)/libc++_shared.so
	@echo Copying assets
	@$(RTOOL) copy-dir $(RESOURCES_DIR) $(APK_ASSETS)/resources --skip icons_src
	@$(RTOOL) package-archive toolchains rebax-clang-android-$(TARGET_ARCH).tar.xz $(BUILD_DIR)/packages/rebax-clang-android-$(TARGET_ARCH).tar.xz
	@$(RTOOL) package-archive sdks rebax-sdk-$(notdir $(SDK_DIR)).tar.xz $(BUILD_DIR)/packages/rebax-sdk-$(notdir $(SDK_DIR)).tar.xz
	@$(RTOOL) cp $(BUILD_DIR)/packages/rebax-clang-android-$(TARGET_ARCH).tar.xz $(APK_ASSETS)/packages/clang.tar.xz
	@$(RTOOL) cp $(BUILD_DIR)/packages/rebax-sdk-$(notdir $(SDK_DIR)).tar.xz $(APK_ASSETS)/packages/sdk-$(notdir $(SDK_DIR)).tar.xz
	@$(RTOOL) copy-dir sdks/rebax $(APK_ASSETS)/sdks/rebax
	@$(RTOOL) write-text $(APK_ASSETS)/packages/stamp.txt $(ENGINE_VERSION)-@time@
	@$(RTOOL) asset-manifest $(APK_ASSETS) $(APK_ASSETS)/rebax_assets.txt
	@echo Linking $(APK_LIB_DIR)/libmain.so
	@$(RTOOL) android-entry $(APK_DIR)/rebax_android_entry.c
	$(TARGET_CC) $(TARGET_CFLAGS) -c $(APK_DIR)/rebax_android_entry.c -o $(APK_DIR)/rebax_android_entry.o
	$(TARGET_CC) $(TARGET_LDPRE) -shared -o $(APK_LIB_DIR)/libmain.so $(OBJS) $(APK_DIR)/rebax_android_entry.o $(TARGET_LDPOST)
	@echo Compiling the Android Java layer
	$(JAVAC) -nowarn -encoding UTF-8 -source 8 -target 8 -classpath $(ANDROID_JAR) -d $(APK_STAGE)/classes $(SDL2_JAVA_SOURCES)
	$(JAR) cf $(APK_STAGE)/classes.jar -C $(APK_STAGE)/classes .
	@echo Generating classes.dex
	$(if $(D8),$(D8) --release --min-api 21 --lib $(ANDROID_JAR) --output $(APK_STAGE)/dex $(APK_STAGE)/classes.jar,$(if $(and $(D8_JAR),$(JAVA)),$(JAVA) -cp $(D8_JAR) com.android.tools.r8.D8 --release --min-api 21 --lib $(ANDROID_JAR) --output $(APK_STAGE)/dex $(APK_STAGE)/classes.jar,$(if $(DX),$(DX) --dex --output=$(APK_STAGE)/dex/classes.dex $(APK_STAGE)/classes.jar,$(error neither d8 nor dx was found - install the Android SDK build-tools))))
	@$(RTOOL) cp $(APK_STAGE)/dex/classes.dex $(APK_STAGE)/classes.dex
	@echo Writing AndroidManifest.xml
	@$(RTOOL) android-manifest $(APK_STAGE)/AndroidManifest.xml $(patsubst v%,%,$(ENGINE_VERSION)) $(APK_TARGET_SDK)
	@echo Linking resources
	$(if $(AAPT2),$(AAPT2) link -o $(APK_STAGE)/base.apk --manifest $(APK_STAGE)/AndroidManifest.xml -I $(ANDROID_JAR) --min-sdk-version 21 --target-sdk-version $(APK_TARGET_SDK) --auto-add-overlay,$(if $(AAPT),$(AAPT) package -f -M $(APK_STAGE)/AndroidManifest.xml -I $(ANDROID_JAR) -F $(APK_STAGE)/base.apk,$(error neither aapt2 nor aapt was found - install the Android SDK build-tools)))
	@echo Adding classes.dex and the native libraries
	$(JAR) ufM0 $(APK_STAGE)/base.apk -C $(APK_STAGE) classes.dex -C $(APK_STAGE) lib/$(TARGET_ARCH)/libmain.so -C $(APK_STAGE) lib/$(TARGET_ARCH)/libSDL2.so -C $(APK_STAGE) lib/$(TARGET_ARCH)/libiconv.so -C $(APK_STAGE) lib/$(TARGET_ARCH)/libc++_shared.so -C $(APK_STAGE) assets
	@echo Aligning
	$(if $(ZIPALIGN),$(ZIPALIGN) -f 4 $(APK_STAGE)/base.apk $(APK_STAGE)/aligned.apk,$(RTOOL) cp $(APK_STAGE)/base.apk $(APK_STAGE)/aligned.apk)
	@echo Signing
	$(if $(and $(KEYTOOL),$(KS_MISSING)),$(KEYTOOL) -genkeypair -keystore $(KEYSTORE) -storepass $(KEYSTORE_PASS) -alias $(KEY_ALIAS) -keypass $(KEY_PASS) -keyalg RSA -keysize 2048 -validity 10000 -dname $(KEY_DNAME))
	$(if $(filter apksigner,$(APK_SIGNER_KIND)),$(APKSIGNER) sign --ks $(KEYSTORE) --ks-pass pass:$(KEYSTORE_PASS) --key-pass pass:$(KEY_PASS) --ks-key-alias $(KEY_ALIAS) --out $(APK_STAGE)/final.apk $(APK_STAGE)/aligned.apk)
	$(if $(filter jarsigner,$(APK_SIGNER_KIND)),$(RTOOL) cp $(APK_STAGE)/aligned.apk $(APK_STAGE)/final.apk)
	$(if $(filter jarsigner,$(APK_SIGNER_KIND)),$(JARSIGNER) -keystore $(KEYSTORE) -storepass $(KEYSTORE_PASS) -keypass $(KEY_PASS) -sigalg SHA256withRSA -digestalg SHA-256 $(APK_STAGE)/final.apk $(KEY_ALIAS))
	$(if $(APK_SIGNER_KIND),,@echo Warning no signing tool was found so the APK stays unsigned)
	$(if $(APK_SIGNER_KIND),,$(RTOOL) cp $(APK_STAGE)/aligned.apk $(APK_STAGE)/final.apk)
	@$(RTOOL) cp $(APK_STAGE)/final.apk $(APK_OUT)
	@$(RTOOL) rm $(APK_STAGE)
	@echo APK ready: $(APK_OUT)

CFLAGS := -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L $(addprefix -I,$(sort $(dir $(call rwildcard,$(SRC_DIR)/,*.h)))) $(addprefix -I,$(sort $(dir $(call rwildcard,sdks/rebax/ps2/include/,*.h)))) -Ithird_party -Ithird_party/mbedtls/include -I$(GEN_DIR) -MMD -MP
ifeq ($(TARGET_PLATFORM),android)
  CFLAGS += -fPIC
endif
$(CACHE_OBJ)/third_party/xz_embedded.o: CFLAGS += -O3
$(CACHE_OBJ)/third_party/mbedtls/%.o: CFLAGS += -w -O2
$(OBJ_DIR)/filesystem/rebax_fs.o: CFLAGS += -O2
