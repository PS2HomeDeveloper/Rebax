BUILD_DIR := build
.DEFAULT_GOAL := all
OBJ_DIR := $(BUILD_DIR)/obj
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
RTOOL := $(RTOOL_DIR)/build_tool$(RTOOL_EXE)
RTOOL_C_SOURCES := $(wildcard $(RTOOL_SRC_DIR)/*.c)
RTOOL_SOURCES := $(RTOOL_C_SOURCES) $(wildcard $(RTOOL_SRC_DIR)/*.h)
RBX_TOOL_READY ?= 0

HOSTCC := $(firstword $(foreach c,cc gcc clang,$(if $(shell command -v $(c) 2>/dev/null),$(c))))
HOST_TRIPLE := $(shell $(HOSTCC) -dumpmachine 2>/dev/null)
UNAME_S := $(shell uname -s 2>/dev/null)
UNAME_M := $(shell uname -m 2>/dev/null)
UNAME_O := $(shell uname -o 2>/dev/null)

ifneq ($(OS),Windows_NT)
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
else
  HOST_PLATFORM := windows
endif

HOST_MACHINE := $(if $(UNAME_M),$(UNAME_M),$(HOST_TRIPLE))
HOST_ARCH :=
ifneq ($(findstring aarch64,$(HOST_MACHINE)),)
  HOST_ARCH := arm64
else ifneq ($(findstring arm64,$(HOST_MACHINE)),)
  HOST_ARCH := arm64
else ifneq ($(findstring armv7,$(HOST_MACHINE)),)
  HOST_ARCH := armeabi-v7a
else ifneq ($(findstring armv8l,$(HOST_MACHINE)),)
  HOST_ARCH := armeabi-v7a
else ifneq ($(findstring armv6,$(HOST_MACHINE)),)
  HOST_ARCH := armeabi-v7a
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

TERMUX_PREFIX := $(if $(PREFIX),$(PREFIX),/data/data/com.termux/files/usr)
ENGINE_VERSION := v1.0.0

ALL_TARGET_GOALS := android-arm64 android-arm64-v8a android-armeabi-v7a android-x86 android-x86_64 macos-arm64 macos-x86_64 ios-arm64 ios-x86_64 linux-arm64 linux-x86 linux-x86_64 windows-arm64 windows-x86 windows-x86_64
APK_VARIANT_GOALS := $(foreach g,$(ALL_TARGET_GOALS),$(g).APK $(g)-APK) apk APK
GOALS_RAW := $(MAKECMDGOALS)
WANT_APK := 0
ifneq ($(filter %.APK %-APK apk APK,$(GOALS_RAW)),)
  WANT_APK := 1
endif
GOALS := $(filter-out apk APK,$(GOALS_RAW))
GOALS := $(patsubst %.APK,%,$(GOALS))
GOALS := $(patsubst %-APK,%,$(GOALS))
SELECTED_GOAL := $(filter $(ALL_TARGET_GOALS),$(GOALS))

TARGET_PLATFORM :=
TARGET_ARCH :=
ifneq ($(SELECTED_GOAL),)
  ifeq ($(SELECTED_GOAL),android-arm64)
    TARGET_PLATFORM := android
    TARGET_ARCH := arm64-v8a
  else ifeq ($(SELECTED_GOAL),android-arm64-v8a)
    TARGET_PLATFORM := android
    TARGET_ARCH := arm64-v8a
  else ifeq ($(SELECTED_GOAL),android-armeabi-v7a)
    TARGET_PLATFORM := android
    TARGET_ARCH := armeabi-v7a
  else ifeq ($(SELECTED_GOAL),android-x86)
    TARGET_PLATFORM := android
    TARGET_ARCH := x86
  else ifeq ($(SELECTED_GOAL),android-x86_64)
    TARGET_PLATFORM := android
    TARGET_ARCH := x86_64
  else ifeq ($(SELECTED_GOAL),macos-arm64)
    TARGET_PLATFORM := macos
    TARGET_ARCH := arm64
  else ifeq ($(SELECTED_GOAL),macos-x86_64)
    TARGET_PLATFORM := macos
    TARGET_ARCH := x86_64
  else ifeq ($(SELECTED_GOAL),ios-arm64)
    TARGET_PLATFORM := ios
    TARGET_ARCH := arm64
  else ifeq ($(SELECTED_GOAL),ios-x86_64)
    TARGET_PLATFORM := ios
    TARGET_ARCH := x86_64
  else ifeq ($(SELECTED_GOAL),linux-arm64)
    TARGET_PLATFORM := linux
    TARGET_ARCH := arm64
  else ifeq ($(SELECTED_GOAL),linux-x86)
    TARGET_PLATFORM := linux
    TARGET_ARCH := x86
  else ifeq ($(SELECTED_GOAL),linux-x86_64)
    TARGET_PLATFORM := linux
    TARGET_ARCH := x86_64
  else ifeq ($(SELECTED_GOAL),windows-arm64)
    TARGET_PLATFORM := windows
    TARGET_ARCH := arm64
  else ifeq ($(SELECTED_GOAL),windows-x86)
    TARGET_PLATFORM := windows
    TARGET_ARCH := x86
  else ifeq ($(SELECTED_GOAL),windows-x86_64)
    TARGET_PLATFORM := windows
    TARGET_ARCH := x86_64
  endif
else
  TARGET_PLATFORM := $(HOST_PLATFORM)
  TARGET_ARCH := $(HOST_ARCH)
endif

TARGET_SUFFIX := $(TARGET_PLATFORM)_$(TARGET_ARCH)
TARGET_NAME := Rebax_Engine_$(ENGINE_VERSION)_$(TARGET_SUFFIX)
ifeq ($(TARGET_PLATFORM),windows)
  TARGET_NAME := $(TARGET_NAME).exe
endif
TARGET := $(OUTPUT_DIR)/$(TARGET_NAME)
APK_OUT := $(OUTPUT_DIR)/Rebax_Engine_$(ENGINE_VERSION)_android_$(TARGET_ARCH).apk

EMBED_ASM_FIXUP := :
ifeq ($(TARGET_PLATFORM),windows)
  ifeq ($(TARGET_ARCH),x86)
    EMBED_ASM_FIXUP := sed -i 's/_binary/__binary/g'
  endif
endif

CC_NEEDED := 1
ifeq ($(strip $(filter-out clean generate gen-icons gen-node-registry gen-node-editor-registry gen-node-archive rebax-build-tool,$(or $(MAKECMDGOALS),$(.DEFAULT_GOAL)))),)
  CC_NEEDED := 0
endif
ifeq ($(RBX_TOOL_READY),0)
  CC_NEEDED := 0
endif

ifeq ($(WANT_APK),1)
  ifneq ($(TARGET_PLATFORM),android)
    $(error The APK build exists for Android targets only; use a goal such as android-arm64-v8a.APK)
  endif
endif

MACOS_SDK := $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null)
IOS_SDK := $(shell xcrun --sdk iphoneos --show-sdk-path 2>/dev/null)
IOS_SIM_SDK := $(shell xcrun --sdk iphonesimulator --show-sdk-path 2>/dev/null)
WIN_LLVM_MINGW_ROOT := $(firstword $(sort $(wildcard llvm-mingw-extracted/*)))

ANDROID_NDK_ROOTS := $(ANDROID_NDK_HOME) $(ANDROID_NDK_ROOT) $(ANDROID_NDK) $(ANDROID_HOME)/ndk $(ANDROID_SDK_ROOT)/ndk $(ANDROID_SDK)/ndk $(HOME)/Android/Sdk/ndk $(HOME)/Library/Android/sdk/ndk $(HOME)/.android/sdk/ndk $(TERMUX_PREFIX)/opt/android-ndk $(TERMUX_PREFIX)/opt/android-sdk/ndk $(TERMUX_PREFIX)/share/android-ndk /opt/android-ndk /opt/android-sdk/ndk /opt/android/sdk/ndk /usr/lib/android-ndk /usr/lib/android-sdk/ndk /usr/local/lib/android/sdk/ndk
ANDROID_SDK_ROOTS := $(ANDROID_HOME) $(ANDROID_SDK_ROOT) $(ANDROID_SDK) $(HOME)/Android/Sdk $(HOME)/Library/Android/sdk $(HOME)/.android/sdk $(TERMUX_PREFIX)/opt/android-sdk $(TERMUX_PREFIX)/share/android-sdk /opt/android-sdk /opt/android /usr/lib/android-sdk /usr/local/lib/android/sdk
ANDROID_NDK_ROOT := $(firstword $(foreach r,$(ANDROID_NDK_ROOTS),$(if $(wildcard $(r)/toolchains/llvm/prebuilt/*/bin),$(r))))
ANDROID_NDK_BIN := $(lastword $(sort $(wildcard $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/*/bin)))
ANDROID_SDK_ROOT := $(lastword $(sort $(wildcard $(ANDROID_SDK_ROOTS))))
ANDROID_BUILD_TOOLS := $(lastword $(sort $(wildcard $(ANDROID_SDK_ROOT)/build-tools/*)))
ANDROID_PLATFORM_DIR := $(lastword $(sort $(wildcard $(ANDROID_SDK_ROOT)/platforms/*)))
ANDROID_JAR := $(if $(ANDROID_PLATFORM_DIR),$(ANDROID_PLATFORM_DIR)/android.jar,$(firstword $(wildcard $(ANDROID_SDK_ROOT)/platforms/android-*/android.jar)))
APK_TARGET_SDK := $(if $(ANDROID_PLATFORM_DIR),$(patsubst android-%,%,$(notdir $(ANDROID_PLATFORM_DIR))),34)

ANDROID_TRIPLE :=
ifeq ($(TARGET_PLATFORM),android)
  ifeq ($(TARGET_ARCH),armeabi-v7a)
    ANDROID_TRIPLE := armv7a-linux-androideabi
  else ifeq ($(TARGET_ARCH),arm64-v8a)
    ANDROID_TRIPLE := aarch64-linux-android
  else ifeq ($(TARGET_ARCH),x86)
    ANDROID_TRIPLE := i686-linux-android
  else
    ANDROID_TRIPLE := x86_64-linux-android
  endif
endif

CC_CANDIDATES :=
CC_REQUIRE :=
CC_FORBID :=
ifeq ($(TARGET_PLATFORM),android)
  ifeq ($(TARGET_ARCH),arm64-v8a)
    CC_REQUIRE := android aarch64
    CC_CANDIDATES += $(foreach p,$(wildcard $(ANDROID_NDK_BIN)/aarch64-linux-android*-clang),$(p)::dump)
    CC_CANDIDATES += $(foreach p,$(wildcard $(TERMUX_PREFIX)/bin/aarch64-linux-android*-clang),$(p)::dump)
    CC_CANDIDATES += aarch64-linux-android21-clang::dump aarch64-linux-android24-clang::dump aarch64-linux-android29-clang::dump aarch64-linux-android33-clang::dump aarch64-linux-android35-clang::dump aarch64-linux-android-legacy-gcc::dump clang::dump cc::dump gcc::dump
    CC_CANDIDATES += clang:--target=aarch64-linux-android21:link clang:--target=aarch64-linux-android24:link clang:--target=aarch64-linux-android29:link clang:--target=aarch64-linux-android33:link
  else ifeq ($(TARGET_ARCH),armeabi-v7a)
    CC_REQUIRE := android armv7
    CC_CANDIDATES += $(foreach p,$(wildcard $(ANDROID_NDK_BIN)/armv7a-linux-androideabi*-clang),$(p)::dump)
    CC_CANDIDATES += $(foreach p,$(wildcard $(TERMUX_PREFIX)/bin/armv7a-linux-androideabi*-clang),$(p)::dump)
    CC_CANDIDATES += armv7a-linux-androideabi21-clang::dump armv7a-linux-androideabi29-clang::dump arm-linux-androideabi-gcc::dump clang::dump cc::dump
    CC_CANDIDATES += clang:--target=armv7a-linux-androideabi21:link clang:--target=armv7a-linux-androideabi29:link
  else ifeq ($(TARGET_ARCH),x86)
    CC_REQUIRE := android i686
    CC_CANDIDATES += $(foreach p,$(wildcard $(ANDROID_NDK_BIN)/i686-linux-android*-clang),$(p)::dump)
    CC_CANDIDATES += $(foreach p,$(wildcard $(TERMUX_PREFIX)/bin/i686-linux-android*-clang),$(p)::dump)
    CC_CANDIDATES += i686-linux-android21-clang::dump i686-linux-android29-clang::dump clang::dump cc::dump
    CC_CANDIDATES += clang:--target=i686-linux-android21:link clang:--target=i686-linux-android29:link
  else
    CC_REQUIRE := android x86_64
    CC_CANDIDATES += $(foreach p,$(wildcard $(ANDROID_NDK_BIN)/x86_64-linux-android*-clang),$(p)::dump)
    CC_CANDIDATES += $(foreach p,$(wildcard $(TERMUX_PREFIX)/bin/x86_64-linux-android*-clang),$(p)::dump)
    CC_CANDIDATES += x86_64-linux-android21-clang::dump x86_64-linux-android29-clang::dump clang::dump cc::dump
    CC_CANDIDATES += clang:--target=x86_64-linux-android21:link clang:--target=x86_64-linux-android29:link
  endif
else ifeq ($(TARGET_PLATFORM),windows)
  ifeq ($(TARGET_ARCH),x86)
    CC_REQUIRE := mingw i686
    CC_CANDIDATES += i686-w64-mingw32-gcc::dump $(WIN_LLVM_MINGW_ROOT)/bin/i686-w64-mingw32-clang::dump clang:--target=i686-w64-mingw32:link
  else ifeq ($(TARGET_ARCH),arm64)
    CC_REQUIRE := mingw aarch64
    CC_CANDIDATES += aarch64-w64-mingw32-gcc::dump $(WIN_LLVM_MINGW_ROOT)/bin/aarch64-w64-mingw32-clang::dump clang:--target=aarch64-w64-mingw32:link
  else
    CC_REQUIRE := mingw x86_64
    CC_CANDIDATES += x86_64-w64-mingw32-gcc::dump $(WIN_LLVM_MINGW_ROOT)/bin/x86_64-w64-mingw32-clang::dump gcc::dump cc::dump clang:--target=x86_64-w64-mingw32:link
  endif
else ifeq ($(TARGET_PLATFORM),macos)
  CC_CANDIDATES += clang:-arch~$(TARGET_ARCH)~-isysroot~$(MACOS_SDK)~-mmacosx-version-min=11.0:link cc:-arch~$(TARGET_ARCH)~-isysroot~$(MACOS_SDK)~-mmacosx-version-min=11.0:link
else ifeq ($(TARGET_PLATFORM),ios)
  ifeq ($(TARGET_ARCH),x86_64)
    CC_CANDIDATES += clang:-arch~x86_64~-isysroot~$(IOS_SIM_SDK)~-mios-simulator-version-min=13.0:link cc:-arch~x86_64~-isysroot~$(IOS_SIM_SDK)~-mios-simulator-version-min=13.0:link
  else
    CC_CANDIDATES += clang:-arch~arm64~-isysroot~$(IOS_SDK)~-mios-version-min=13.0:link cc:-arch~arm64~-isysroot~$(IOS_SDK)~-mios-version-min=13.0:link
  endif
else ifeq ($(TARGET_ARCH),arm64)
  CC_REQUIRE := linux aarch64
  CC_FORBID := android
  CC_CANDIDATES += aarch64-linux-gnu-gcc::dump aarch64-unknown-linux-gnu-gcc::dump aarch64-linux-musl-gcc::dump gcc::dump cc::dump clang::dump clang:--target=aarch64-linux-gnu:dump
else ifeq ($(TARGET_ARCH),x86)
  CC_REQUIRE := linux
  CC_FORBID := android x86_64 aarch64
  CC_CANDIDATES += i686-linux-gnu-gcc::dump i386-linux-gnu-gcc::dump gcc:-m32:link clang:-m32:link clang:--target=i686-linux-gnu:dump
else
  CC_REQUIRE := linux x86_64
  CC_FORBID := android
  CC_CANDIDATES += gcc::dump cc::dump clang::dump x86_64-linux-gnu-gcc::dump clang:--target=x86_64-linux-gnu:dump
endif

define rbx_try_cc
$(shell IFS=' '; for cand in $(1); do IFS=':'; set -- $$cand; IFS=' '; name=$$1; ftilde=$$2; mode=$$3; flags=$$(printf '%s' "$$ftilde" | tr '~' ' '); command -v "$$name" >/dev/null 2>&1 || continue; if [ "$$mode" = link ]; then mkdir -p $(BUILD_DIR)/.ccprobe; printf 'int main(){return 0;}\n' > $(BUILD_DIR)/.ccprobe/probe.c; "$$name" $$flags $(BUILD_DIR)/.ccprobe/probe.c -o $(BUILD_DIR)/.ccprobe/probe.bin >/dev/null 2>&1 && { printf '%s' "$$name|$$ftilde"; break; }; else t=$$("$$name" $$flags -dumpmachine 2>/dev/null); ok=1; for need in $(2); do [ "$${t%%*$$need*}" = "$$t" ] && ok=0; done; for bad in $(3); do [ "$${t%%*$$bad*}" != "$$t" ] && ok=0; done; [ "$$ok" = 1 ] && { printf '%s' "$$name|$$ftilde"; break; }; fi; done)
endef

CC_SPEC := $(call rbx_try_cc,$(CC_CANDIDATES),$(CC_REQUIRE),$(CC_FORBID))
TARGET_CC := $(firstword $(subst |, ,$(CC_SPEC)))
TARGET_CC_FLAGS := $(subst ~, ,$(word 2,$(subst |, ,$(CC_SPEC))))
USER_CC_SPEC := $(if $(filter environment command line,$(origin REBAX_CC)),$(REBAX_CC),$(if $(filter environment command line,$(origin CC)),$(CC),))
ifneq ($(strip $(USER_CC_SPEC)),)
  TARGET_CC := $(firstword $(USER_CC_SPEC))
  TARGET_CC_FLAGS := $(wordlist 2,99,$(USER_CC_SPEC))
endif
TARGET_TRIPLE := $(ANDROID_TRIPLE)
ifeq ($(TARGET_PLATFORM),windows)
  ifeq ($(TARGET_ARCH),x86)
    TARGET_TRIPLE := i686-w64-mingw32
  else ifeq ($(TARGET_ARCH),x86_64)
    TARGET_TRIPLE := x86_64-w64-mingw32
  else
    TARGET_TRIPLE := aarch64-w64-mingw32
  endif
endif

define rbx_cc_check
$(shell mkdir -p $(BUILD_DIR)/.ccprobe 2>/dev/null; printf 'int main(){return 0;}\n' > $(BUILD_DIR)/.ccprobe/check.c 2>/dev/null; $(TARGET_CC) $(TARGET_CC_FLAGS) $(BUILD_DIR)/.ccprobe/check.c -o $(BUILD_DIR)/.ccprobe/check.bin 2>&1; printf ' rbax_cc_exit=%s' $$?)
endef

CC_CHECK_RESULT := $(call rbx_cc_check)
CC_CHECK_STATUS := $(patsubst rbax_cc_exit=%,%,$(lastword $(CC_CHECK_RESULT)))
CC_CHECK_LOG := $(filter-out $(lastword $(CC_CHECK_RESULT)),$(CC_CHECK_RESULT))
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

define rbx_file_arch
$(shell f="$(1)"; if [ ! -f "$$f" ]; then printf none; exit 0; fi; h=$$(od -An -tx1 -N 4 "$$f" 2>/dev/null | tr -d ' \n'); if [ "$$h" = "7f454c46" ]; then em=$$(od -An -tx1 -j 18 -N 2 "$$f" 2>/dev/null | tr -d ' \n'); if [ "$$em" = "3e00" ]; then printf x86_64; elif [ "$$em" = "0300" ]; then printf x86; elif [ "$$em" = "b700" ]; then printf arm64; elif [ "$$em" = "2800" ]; then printf arm; else printf unknown; fi; exit 0; fi; mz=$$(od -An -tx1 -N 2 "$$f" 2>/dev/null | tr -d ' \n'); if [ "$$mz" = "4d5a" ]; then b0=$$(od -An -tx1 -j 60 -N 1 "$$f" 2>/dev/null | tr -d ' \n'); b1=$$(od -An -tx1 -j 61 -N 1 "$$f" 2>/dev/null | tr -d ' \n'); b2=$$(od -An -tx1 -j 62 -N 1 "$$f" 2>/dev/null | tr -d ' \n'); b3=$$(od -An -tx1 -j 63 -N 1 "$$f" 2>/dev/null | tr -d ' \n'); off=$$(printf '%d' "0x$$b3$$b2$$b1$$b0" 2>/dev/null); [ -n "$$off" ] || off=0; pm=$$(od -An -tx1 -j $$((off + 4)) -N 2 "$$f" 2>/dev/null | tr -d ' \n'); if [ "$$pm" = "6486" ]; then printf x86_64; elif [ "$$pm" = "4c01" ]; then printf x86; elif [ "$$pm" = "64aa" ]; then printf arm64; elif [ "$$pm" = "c001" ]; then printf arm; elif [ "$$pm" = "c401" ]; then printf arm; else printf unknown; fi; exit 0; fi; if [ "$$h" = "cffaedfe" ] || [ "$$h" = "cefaedfe" ] || [ "$$h" = "feedface" ] || [ "$$h" = "feedfacf" ] || [ "$$h" = "cafebabe" ] || [ "$$h" = "bebafeca" ]; then cm=$$(od -An -tx1 -j 4 -N 4 "$$f" 2>/dev/null | tr -d ' \n'); if [ "$$cm" = "0c000001" ] || [ "$$cm" = "0100000c" ]; then printf arm64; elif [ "$$cm" = "07000001" ] || [ "$$cm" = "01000007" ]; then printf x86_64; elif [ "$$cm" = "0c000000" ] || [ "$$cm" = "0000000c" ]; then printf arm; elif [ "$$cm" = "07000000" ] || [ "$$cm" = "00000007" ]; then printf x86; else printf unknown; fi; exit 0; fi; printf unknown)
endef

SDL2_PKG_NAME :=
ifeq ($(RBX_TOOL_READY),1)
  SDL2_PKG_NAME := $(firstword $(foreach p,sdl2 SDL2,$(if $(filter 1,$(shell $(RTOOL) pkg-config --exists $(p) 2>/dev/null)),$(p))))
endif
SDL2_PKG_CFLAGS := $(if $(SDL2_PKG_NAME),$(shell $(RTOOL) pkg-config --cflags $(SDL2_PKG_NAME) 2>/dev/null),)
SDL2_PKG_LIBS := $(if $(SDL2_PKG_NAME),$(shell $(RTOOL) pkg-config --libs $(SDL2_PKG_NAME) 2>/dev/null),)
SDL2_PKG_INCLUDEDIR := $(if $(SDL2_PKG_NAME),$(shell $(RTOOL) pkg-config --variable=includedir $(SDL2_PKG_NAME) 2>/dev/null),)
SDL2_PKG_LIBDIR := $(if $(SDL2_PKG_NAME),$(shell $(RTOOL) pkg-config --variable=libdir $(SDL2_PKG_NAME) 2>/dev/null),)
SDL2_INCLUDE_CANDIDATES := $(SDL2_PKG_INCLUDEDIR)/SDL2 $(SDL2_PKG_INCLUDEDIR) sdl2-$(TARGET_PLATFORM)-$(TARGET_ARCH)/include/SDL2 sdl2-$(TARGET_PLATFORM)-$(TARGET_ARCH)/include sdl2-$(TARGET_PLATFORM)/include/SDL2 sdl2-$(TARGET_PLATFORM)/include $(TERMUX_PREFIX)/include/SDL2 /usr/include/SDL2 /usr/local/include/SDL2 /opt/homebrew/include/SDL2 /opt/local/include/SDL2
SDL2_INCLUDE_DIR := $(firstword $(foreach d,$(SDL2_INCLUDE_CANDIDATES),$(if $(wildcard $(d)/SDL.h),$(d))))

SDL2_LIB_NAMES := libSDL2.so libSDL2.a libSDL2.dylib libSDL2.dll.a libSDL2.lib libSDL2.so.0 libSDL2-2.0.so libSDL2-2.0.so.0 libSDL2-2.0.0.dylib libSDL2.dll
SDL2_LINK_NAMES := libSDL2.so libSDL2.a libSDL2.dylib libSDL2.dll.a libSDL2.lib

ifeq ($(TARGET_PLATFORM),android)
  SDL2_LIB_CANDIDATES := $(SDL2_PKG_LIBDIR) sdl2-android-$(TARGET_ARCH)/lib sdl2-android/lib $(TERMUX_PREFIX)/lib
else ifeq ($(TARGET_PLATFORM),windows)
  SDL2_LIB_CANDIDATES := $(SDL2_PKG_LIBDIR) sdl2-windows-$(TARGET_ARCH)/lib sdl2-windows/lib $(WIN_LLVM_MINGW_ROOT)/$(TARGET_TRIPLE)/lib /usr/$(TARGET_TRIPLE)/lib /usr/lib/$(TARGET_TRIPLE) $(TERMUX_PREFIX)/lib
else ifeq ($(TARGET_PLATFORM),macos)
  SDL2_LIB_CANDIDATES := $(SDL2_PKG_LIBDIR) sdl2-macos-$(TARGET_ARCH)/lib sdl2-macos/lib /opt/homebrew/lib /usr/local/lib /opt/local/lib $(TERMUX_PREFIX)/lib
else ifeq ($(TARGET_PLATFORM),ios)
  SDL2_LIB_CANDIDATES := $(SDL2_PKG_LIBDIR) sdl2-ios-$(TARGET_ARCH)/lib sdl2-ios/lib $(TERMUX_PREFIX)/lib
else
  SDL2_LIB_CANDIDATES := $(SDL2_PKG_LIBDIR) sdl2-linux-$(TARGET_ARCH)/lib sdl2-linux/lib sdl2-linux-$(TARGET_ARCH)/lib64 sdl2-linux/lib64 $(TERMUX_PREFIX)/lib/$(TARGET_TRIPLE) $(TERMUX_PREFIX)/lib /usr/lib/$(TARGET_TRIPLE) /usr/$(TARGET_TRIPLE)/lib /usr/lib/$(HOST_TRIPLE) /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /usr/lib/i386-linux-gnu /usr/lib/arm-linux-gnueabihf /usr/lib64 /usr/lib /usr/local/lib
endif

SDL2_LIB_FILE := $(firstword $(foreach d,$(SDL2_LIB_CANDIDATES),$(foreach n,$(SDL2_LIB_NAMES),$(if $(wildcard $(d)/$(n)),$(if $(filter $(SDL2_EXPECT_ARCH) unknown,$(call rbx_file_arch,$(d)/$(n))),$(d)/$(n))))))
SDL2_LIB_DIR := $(patsubst %/,%,$(dir $(SDL2_LIB_FILE)))
SDL2_LIBS := $(if $(SDL2_PKG_LIBS),$(SDL2_PKG_LIBS),$(if $(SDL2_LIB_FILE),$(if $(filter $(notdir $(SDL2_LIB_FILE)),$(SDL2_LINK_NAMES)),-L$(SDL2_LIB_DIR) -lSDL2,$(SDL2_LIB_FILE))))
ICONV_LIB_NAMES := libiconv.so libiconv.so.2 libiconv.so.3
ICONV_LIB_CANDIDATES := $(SDL2_LIB_DIR) $(SDL2_LIB_CANDIDATES)
ICONV_LIB_FILE := $(firstword $(foreach d,$(ICONV_LIB_CANDIDATES),$(foreach n,$(ICONV_LIB_NAMES),$(if $(wildcard $(d)/$(n)),$(if $(filter $(SDL2_EXPECT_ARCH) unknown,$(call rbx_file_arch,$(d)/$(n))),$(d)/$(n))))))
CXX_SHARED_LIB_CANDIDATES := $(SDL2_LIB_DIR) $(SDL2_LIB_CANDIDATES) $(foreach d,$(wildcard $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/*/sysroot/usr/lib/$(ANDROID_TRIPLE)),$d) $(TERMUX_PREFIX)/lib
CXX_SHARED_LIB_FILE := $(firstword $(foreach d,$(CXX_SHARED_LIB_CANDIDATES),$(if $(wildcard $(d)/libc++_shared.so),$(if $(filter $(SDL2_EXPECT_ARCH) unknown,$(call rbx_file_arch,$(d)/libc++_shared.so)),$(d)/libc++_shared.so))))

ifeq ($(CC_NEEDED),1)
  ifeq ($(strip $(TARGET_CC)),)
    $(error No C compiler targeting $(TARGET_PLATFORM)-$(TARGET_ARCH) was found; install a toolchain for it and run make again)
  endif
  ifneq ($(CC_CHECK_STATUS),0)
    $(error $(TARGET_CC) cannot compile and link a test program for $(TARGET_PLATFORM)-$(TARGET_ARCH): $(CC_CHECK_LOG))
  endif
  ifeq ($(strip $(SDL2_INCLUDE_DIR)),)
    $(error SDL2 headers were not found for $(TARGET_PLATFORM)-$(TARGET_ARCH); install SDL2 with pkg-config support or place it in sdl2-$(TARGET_PLATFORM)/include)
  endif
  ifeq ($(strip $(SDL2_LIB_FILE)),)
    $(error No SDL2 library matching $(TARGET_PLATFORM)-$(TARGET_ARCH) was found; install or provide SDL2 for that target)
  endif
endif

SDL2_CFLAGS := $(SDL2_PKG_CFLAGS) -I$(SDL2_INCLUDE_DIR)

ifeq ($(TARGET_PLATFORM),windows)
  RUNTIME_LIBS := SDL2.dll
  RUNTIME_DIRS := $(SDL2_LIB_DIR) sdl2-windows/bin sdl2-windows/lib
else ifeq ($(TARGET_PLATFORM),android)
  RUNTIME_LIBS := libSDL2.so
  RUNTIME_DIRS := $(SDL2_LIB_DIR) sdl2-android/lib
else ifeq ($(TARGET_PLATFORM),linux)
  RUNTIME_LIBS := libSDL2-2.0.so.0
  RUNTIME_DIRS := $(SDL2_LIB_DIR) sdl2-linux/lib /usr/lib/$(HOST_TRIPLE) /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
else ifeq ($(TARGET_PLATFORM),macos)
  RUNTIME_LIBS := libSDL2-2.0.0.dylib
  RUNTIME_DIRS := $(SDL2_LIB_DIR) sdl2-macos/lib /opt/homebrew/lib /usr/local/lib
else
  RUNTIME_LIBS :=
  RUNTIME_DIRS :=
endif

$(RTOOL): $(RTOOL_SOURCES)
	@echo "==> Building $(RTOOL) with $(HOSTCC)"
	$(HOSTCC) -std=c99 -O2 -DZ7_ST -D_7ZIP_ST -D_POSIX_C_SOURCE=200809L $(RTOOL_PLATFORM_CFLAGS) -o $@ $(RTOOL_C_SOURCES) -lm

.PHONY: rebax-build-tool
rebax-build-tool: $(RTOOL)
PS2_TOOLCHAIN_RELEASE_BASE := https://github.com/PS2HomeDeveloper/Rebax-PS2-Toolchains/releases/download/1.0.0
ENGINE_TOOLCHAIN_RELEASE_BASE := https://github.com/PS2HomeDeveloper/Rebax-Toolchains/releases/download/1.0.0
PS2_TOOLCHAINS_DIR := embedded/ps2/toolchains
ENGINE_TOOLCHAINS_DIR := embedded/toolchains
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
PS2_TOOLCHAIN_ASSET := $(subst rebax-toolchains-,rebax-PS2-toolchains-,$(TOOLCHAIN_ASSET))
ifeq ($(TARGET_PLATFORM),windows)
  TOOLCHAIN_MAKE_BIN := make.exe
else
  TOOLCHAIN_MAKE_BIN := make
endif
PS2_TOOLCHAIN_REQUIRED := $(PS2_TOOLCHAINS_DIR)/ps2dev.tar.xz
ENGINE_TOOLCHAIN_REQUIRED := $(ENGINE_TOOLCHAINS_DIR)/$(TOOLCHAIN_MAKE_BIN)
TOOLCHAIN_REQUIRED := $(PS2_TOOLCHAIN_REQUIRED) $(ENGINE_TOOLCHAIN_REQUIRED)
PS2_TOOLCHAIN_ASSET_URL := $(PS2_TOOLCHAIN_RELEASE_BASE)/$(PS2_TOOLCHAIN_ASSET)
ENGINE_TOOLCHAIN_ASSET_URL := $(ENGINE_TOOLCHAIN_RELEASE_BASE)/$(TOOLCHAIN_ASSET)
PS2_TOOLCHAIN_TMP := $(BUILD_DIR)/.$(PS2_TOOLCHAIN_ASSET).part
ENGINE_TOOLCHAIN_TMP := $(BUILD_DIR)/.$(TOOLCHAIN_ASSET).part

.PHONY: prepare-ps2-toolchain prepare-engine-toolchain
prepare-ps2-toolchain: $(RTOOL) | $(BUILD_DIR)
	@set -eu; \
	dir_exists=$$($(RTOOL) exists $(PS2_TOOLCHAINS_DIR)); \
	if [ "$$dir_exists" != 1 ]; then \
		$(RTOOL) mkdir $(PS2_TOOLCHAINS_DIR); \
	else \
		dir_empty=$$($(RTOOL) dir-empty $(PS2_TOOLCHAINS_DIR)); \
		if [ "$$dir_empty" != 1 ]; then \
			missing=0; \
			for item in $(PS2_TOOLCHAIN_REQUIRED); do \
				if [ "$$($(RTOOL) exists "$$item")" != 1 ]; then echo "Warning: missing required PS2 toolchain file: $$item"; missing=1; fi; \
			done; \
			if [ "$$missing" = 0 ]; then exit 0; fi; \
			echo "Warning: PS2 toolchain directory already contains files; not downloading its package."; \
			exit 0; \
		fi; \
	fi; \
	echo "==> Downloading PS2 toolchain $(PS2_TOOLCHAIN_ASSET)"; \
	$(RTOOL) rm $(PS2_TOOLCHAIN_TMP); \
	trap '$(RTOOL) rm $(PS2_TOOLCHAIN_TMP)' EXIT; \
	if curl --version >/dev/null 2>&1; then \
		curl -fL --retry 3 --connect-timeout 15 -o $(PS2_TOOLCHAIN_TMP) $(PS2_TOOLCHAIN_ASSET_URL); \
	elif wget --version >/dev/null 2>&1; then \
		wget -O $(PS2_TOOLCHAIN_TMP) $(PS2_TOOLCHAIN_ASSET_URL); \
	else echo "curl or wget is required"; exit 1; fi; \
	$(RTOOL) extract $(PS2_TOOLCHAIN_TMP) $(PS2_TOOLCHAINS_DIR); \
	$(RTOOL) rm $(PS2_TOOLCHAIN_TMP)

prepare-engine-toolchain: $(RTOOL) | $(BUILD_DIR)
	@set -eu; \
	dir_exists=$$($(RTOOL) exists $(ENGINE_TOOLCHAINS_DIR)); \
	if [ "$$dir_exists" != 1 ]; then \
		$(RTOOL) mkdir $(ENGINE_TOOLCHAINS_DIR); \
	else \
		dir_empty=$$($(RTOOL) dir-empty $(ENGINE_TOOLCHAINS_DIR)); \
		if [ "$$dir_empty" != 1 ]; then \
			missing=0; \
			for item in $(ENGINE_TOOLCHAIN_REQUIRED); do \
				if [ "$$($(RTOOL) exists "$$item")" != 1 ]; then echo "Warning: missing required engine tool: $$item"; missing=1; fi; \
			done; \
			if [ "$$missing" = 0 ]; then exit 0; fi; \
			echo "Warning: engine toolchain directory already contains files; not downloading its package."; \
			exit 0; \
		fi; \
	fi; \
	echo "==> Downloading engine toolchain $(TOOLCHAIN_ASSET)"; \
	$(RTOOL) rm $(ENGINE_TOOLCHAIN_TMP); \
	trap '$(RTOOL) rm $(ENGINE_TOOLCHAIN_TMP)' EXIT; \
	if curl --version >/dev/null 2>&1; then \
		curl -fL --retry 3 --connect-timeout 15 -o $(ENGINE_TOOLCHAIN_TMP) $(ENGINE_TOOLCHAIN_ASSET_URL); \
	elif wget --version >/dev/null 2>&1; then \
		wget -O $(ENGINE_TOOLCHAIN_TMP) $(ENGINE_TOOLCHAIN_ASSET_URL); \
	else echo "curl or wget is required"; exit 1; fi; \
	$(RTOOL) extract $(ENGINE_TOOLCHAIN_TMP) $(ENGINE_TOOLCHAINS_DIR); \
	$(RTOOL) rm $(ENGINE_TOOLCHAIN_TMP)

$(PS2_TOOLCHAIN_REQUIRED): prepare-ps2-toolchain
$(ENGINE_TOOLCHAIN_REQUIRED): prepare-engine-toolchain

SRC_DIR := src
EMBEDDED_DIR := embedded
EMBEDDED_PS2_DIR := $(EMBEDDED_DIR)/ps2
ICON_SRC_DIR := $(EMBEDDED_DIR)/resources/images/icons/icons_src
ICON_ATLAS_DIR := $(EMBEDDED_DIR)/resources/images/icons
ICON_ATLAS_PNGS := $(sort $(wildcard $(ICON_ATLAS_DIR)/icons[0-9]*.png))
ICON_NAMES_HEADER := $(SRC_DIR)/icon_names.h
ICON_ATLAS_HEADER := $(SRC_DIR)/icon_atlas_pages.h
ICON_SOURCE_FILES := $(sort $(wildcard $(ICON_SRC_DIR)/*.png))
rwildcard = $(filter-out $(patsubst %/,%,$(wildcard $1*/)),$(wildcard $1$2)) $(foreach d,$(wildcard $1*/),$(call rwildcard,$d,$2))
NODE_SRC_DIR := $(EMBEDDED_PS2_DIR)/sdk/nodes
NODE_EDITOR_SRC_DIR := $(SRC_DIR)/nodes_editor
NODE_SOURCE_FILES := $(sort $(wildcard $(NODE_SRC_DIR)/*.c))
NODE_SOURCE_ALL_FILES := $(call rwildcard,$(NODE_SRC_DIR)/,*)
NODE_EDITOR_SOURCE_FILES := $(sort $(wildcard $(NODE_EDITOR_SRC_DIR)/*.c))
NODE_REGISTRY_GENERATED := $(SRC_DIR)/node_registry_generated.h
NODE_EDITOR_REGISTRY_GENERATED := $(SRC_DIR)/node_editor_registry_generated.h
NODE_TYPES_HEADER := $(SRC_DIR)/node_types.h
NODE_ARCHIVE := $(EMBEDDED_PS2_DIR)/sdk/nodes.tar.xz
SRCS := $(call rwildcard,$(SRC_DIR)/,*.c)
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
EMBEDDED_RESOURCE_FILES := $(sort $(call rwildcard,$(EMBEDDED_DIR)/resources/,*))
EMBEDDED_FILES := $(sort $(filter-out $(ICON_SRC_DIR)/%,$(EMBEDDED_RESOURCE_FILES)) $(TOOLCHAIN_REQUIRED) $(ICON_ATLAS_PNGS) $(NODE_ARCHIVE))
EMBEDDED_OBJS := $(patsubst $(EMBEDDED_DIR)/%,$(OBJ_DIR)/embedded/%.o,$(EMBEDDED_FILES))
OBJS := $(SRC_OBJS) $(EMBEDDED_OBJS)
DEPS := $(SRC_OBJS:.o=.d)

.PHONY: all clean run generate bundle-runtime-libs gen-icons gen-node-registry gen-node-editor-registry gen-node-archive \
  build-selected apk-selected apk-package \
  $(ALL_TARGET_GOALS) $(APK_VARIANT_GOALS)

$(ALL_TARGET_GOALS) $(APK_VARIANT_GOALS): all

all: $(RTOOL) $(TOOLCHAIN_REQUIRED) generate
		+$(MAKE) --no-print-directory RBX_TOOL_READY=1 TARGET_PLATFORM=$(TARGET_PLATFORM) TARGET_ARCH=$(TARGET_ARCH) WANT_APK=$(WANT_APK) $(if $(filter 1,$(WANT_APK)),apk-selected,build-selected)

build-selected: $(TARGET) bundle-runtime-libs

apk-selected: $(TARGET) bundle-runtime-libs apk-package

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
	@$(RTOOL) pack $@ $(EMBEDDED_PS2_DIR)/sdk nodes
$(TARGET): $(OBJS) | $(RTOOL) $(OUTPUT_DIR)
	$(TARGET_CC) $(TARGET_CC_FLAGS) $(OBJS) -o $@ $(TARGET_LDFLAGS) $(LDFLAGS) $(LDLIBS)

bundle-runtime-libs: $(TARGET) | $(RTOOL)
	@$(RTOOL) mkdir $(OUTPUT_DIR)/libs
	@for lib in $(RUNTIME_LIBS); do for dir in $(RUNTIME_DIRS); do if [ "$$($(RTOOL) exists "$$dir/$$lib")" = 1 ]; then $(RTOOL) cp "$$dir/$$lib" "$(OUTPUT_DIR)/libs/$$lib"; break; fi; done; done

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
	$(TARGET_CC) $(TARGET_CC_FLAGS) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/embedded/toolchains/$(TOOLCHAIN_MAKE_BIN).o: $(ENGINE_TOOLCHAINS_DIR)/$(TOOLCHAIN_MAKE_BIN) | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	@$(RTOOL) embed-asm $@.S $< embedded/toolchains/make
	@$(EMBED_ASM_FIXUP) $@.S
	$(TARGET_CC) $(TARGET_CC_FLAGS) -c $@.S -o $@
$(OBJ_DIR)/embedded/%.o: $(EMBEDDED_DIR)/% | $(RTOOL)
	@$(RTOOL) mkdir $(dir $@)
	@$(RTOOL) embed-asm $@.S $<
	@$(EMBED_ASM_FIXUP) $@.S
	$(TARGET_CC) $(TARGET_CC_FLAGS) -c $@.S -o $@
clean: | $(RTOOL)
	@$(RTOOL) rm $(BUILD_DIR) $(NODE_ARCHIVE) $(NODE_EDITOR_REGISTRY_GENERATED) $(NODE_REGISTRY_GENERATED) $(NODE_TYPES_HEADER) $(ICON_NAMES_HEADER) $(ICON_ATLAS_HEADER)
	@for atlas in $(ICON_ATLAS_DIR)/icons[0-9]*.png; do if [ -f "$$atlas" ]; then $(RTOOL) rm "$$atlas"; fi; done
run: all
	$(TARGET)
-include $(DEPS)
ANDROID_PAYLOAD_LIBS := -llog -landroid -lOpenSLES
SDL2_JAVA_SOURCES :=
ifeq ($(WANT_APK),1)
  SDL2_JAVA_SOURCE_ROOTS := $(strip $(SDL2_ANDROID_JAVA_DIR) $(SDL2_JAVA_DIR) $(SDL2_SOURCE_DIR) $(SDL2_SRC_DIR) $(SDL2_ANDROID_PROJECT) $(SDL2_ROOT) $(SDL2_DIR) $(SDL2_PREFIX) $(SDL2_INSTALL_PREFIX) $(CURDIR) $(wildcard SDL2-*) $(wildcard sdl2-*) $(wildcard third_party/SDL2*) $(wildcard external/SDL2*) $(wildcard vendor/SDL2*) $(wildcard $(HOME)/.local/SDL2*) $(wildcard $(HOME)/src/SDL2*) $(wildcard $(HOME)/SDL2*) $(wildcard $(TERMUX_PREFIX)/opt/SDL2*) $(wildcard $(TERMUX_PREFIX)/share/SDL2*) $(wildcard /usr/local/src/SDL2*) $(wildcard /usr/local/SDL2*) $(wildcard /opt/SDL2*))
  define rbx_sdl2_java_files
  $(foreach file,$(call rwildcard,$(1)/,*),$(if $(findstring /org/libsdl/app/,$(file)),$(if $(filter %.java,$(file)),$(file))))
  endef
  ifneq ($(strip $(SDL2_JAVA_SOURCES)),)
    SDL2_JAVA_SOURCES := $(sort $(SDL2_JAVA_SOURCES))
  else
    SDL2_JAVA_SOURCES := $(sort $(foreach root,$(SDL2_JAVA_SOURCE_ROOTS),$(if $(wildcard $(root)/.),$(call rbx_sdl2_java_files,$(root)))))
  endif
endif

ifeq ($(WANT_APK),1)
  AAPT2 := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/aapt2 $(ANDROID_BUILD_TOOLS)/aapt2.exe $(ANDROID_BUILD_TOOLS)/aapt2.bat) $(shell command -v aapt2 2>/dev/null))
  AAPT := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/aapt $(ANDROID_BUILD_TOOLS)/aapt.exe $(ANDROID_BUILD_TOOLS)/aapt.bat) $(shell command -v aapt 2>/dev/null))
  ZIPALIGN := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/zipalign $(ANDROID_BUILD_TOOLS)/zipalign.exe) $(shell command -v zipalign 2>/dev/null))
  APKSIGNER := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/apksigner $(ANDROID_BUILD_TOOLS)/apksigner.bat) $(shell command -v apksigner 2>/dev/null))
  D8 := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/d8 $(ANDROID_BUILD_TOOLS)/d8.bat) $(shell command -v d8 2>/dev/null))
  DX := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/dx $(ANDROID_BUILD_TOOLS)/dx.bat) $(shell command -v dx 2>/dev/null))
  D8_JAR := $(firstword $(wildcard $(ANDROID_BUILD_TOOLS)/lib/d8.jar))
  JAVAC := $(firstword $(wildcard $(JAVA_HOME)/bin/javac $(JAVA_HOME)/bin/javac.exe $(TERMUX_PREFIX)/opt/openjdk/bin/javac) $(shell command -v javac 2>/dev/null))
  JAVA := $(firstword $(wildcard $(JAVA_HOME)/bin/java $(JAVA_HOME)/bin/java.exe $(TERMUX_PREFIX)/opt/openjdk/bin/java) $(shell command -v java 2>/dev/null))
  JAR := $(firstword $(wildcard $(JAVA_HOME)/bin/jar $(JAVA_HOME)/bin/jar.exe $(TERMUX_PREFIX)/opt/openjdk/bin/jar) $(shell command -v jar 2>/dev/null))
  KEYTOOL := $(firstword $(wildcard $(JAVA_HOME)/bin/keytool $(JAVA_HOME)/bin/keytool.exe $(TERMUX_PREFIX)/opt/openjdk/bin/keytool) $(shell command -v keytool 2>/dev/null))
  JARSIGNER := $(firstword $(wildcard $(JAVA_HOME)/bin/jarsigner $(JAVA_HOME)/bin/jarsigner.exe $(TERMUX_PREFIX)/opt/openjdk/bin/jarsigner) $(shell command -v jarsigner 2>/dev/null))
  ZIP := $(firstword $(shell command -v zip 2>/dev/null))
  PYTHON := $(firstword $(shell command -v python3 2>/dev/null) $(shell command -v python 2>/dev/null))
endif

apk-package: $(TARGET) bundle-runtime-libs
	@set -eu; \
	stage="$(APK_STAGE)"; \
	abi="$(TARGET_ARCH)"; \
	$(RTOOL) mkdir $(APK_DIR) $(OUTPUT_DIR); \
	$(RTOOL) rm "$$stage"; \
	$(RTOOL) mkdir "$$stage" "$$stage/lib/$$abi" "$$stage/classes" "$$stage/dex"; \
	echo "==> Packaging $(APK_OUT)"; \
	if [ "$(SDL2_LIB_FILE)" = "" ]; then echo "ERROR: no SDL2 shared library matching $$abi was found; build SDL2 for Android first"; exit 1; fi; \
	if [ "$(call rbx_file_arch,$(SDL2_LIB_FILE))" != "unknown" ] && [ "$(call rbx_file_arch,$(SDL2_LIB_FILE))" != "$(SDL2_EXPECT_ARCH)" ]; then echo "ERROR: $(SDL2_LIB_FILE) is $(call rbx_file_arch,$(SDL2_LIB_FILE)) but the target is $(TARGET_ARCH)"; exit 1; fi; \
	if [ "$$($(RTOOL) exists "$(SDL2_LIB_FILE)")" != "1" ]; then echo "ERROR: $(SDL2_LIB_FILE) was not found on disk; nothing was packaged"; exit 1; fi; \
	if [ "$(ICONV_LIB_FILE)" = "" ]; then echo "ERROR: libiconv.so was not found for $(TARGET_ARCH); build or provide libiconv for Android"; exit 1; fi; \
	if [ "$(call rbx_file_arch,$(ICONV_LIB_FILE))" != "unknown" ] && [ "$(call rbx_file_arch,$(ICONV_LIB_FILE))" != "$(SDL2_EXPECT_ARCH)" ]; then echo "ERROR: $(ICONV_LIB_FILE) is $(call rbx_file_arch,$(ICONV_LIB_FILE)) but the target is $(TARGET_ARCH)"; exit 1; fi; \
	if [ "$$($(RTOOL) exists "$(ICONV_LIB_FILE)")" != "1" ]; then echo "ERROR: $(ICONV_LIB_FILE) was not found on disk; nothing was packaged"; exit 1; fi; \
	if [ "$(CXX_SHARED_LIB_FILE)" = "" ]; then echo "ERROR: libc++_shared.so was not found for $(TARGET_ARCH); provide the Android NDK C++ shared runtime"; exit 1; fi; \
	if [ "$(call rbx_file_arch,$(CXX_SHARED_LIB_FILE))" != "unknown" ] && [ "$(call rbx_file_arch,$(CXX_SHARED_LIB_FILE))" != "$(SDL2_EXPECT_ARCH)" ]; then echo "ERROR: $(CXX_SHARED_LIB_FILE) is $(call rbx_file_arch,$(CXX_SHARED_LIB_FILE)) but the target is $(TARGET_ARCH)"; exit 1; fi; \
	if [ "$$($(RTOOL) exists "$(CXX_SHARED_LIB_FILE)")" != "1" ]; then echo "ERROR: $(CXX_SHARED_LIB_FILE) was not found on disk; nothing was packaged"; exit 1; fi; \
	$(RTOOL) cp "$(SDL2_LIB_FILE)" "$$stage/lib/$$abi/libSDL2.so"; \
	$(RTOOL) cp "$(ICONV_LIB_FILE)" "$$stage/lib/$$abi/libiconv.so"; \
	$(RTOOL) cp "$(CXX_SHARED_LIB_FILE)" "$$stage/lib/$$abi/libc++_shared.so"; \
	echo "==> Linking $$stage/lib/$$abi/libmain.so"; \
	printf 'extern int main(void);\nint SDL_main(int argc, char **argv) { (void)argc; (void)argv; return main(); }\n' > $(APK_DIR)/rebax_android_entry.c; \
	$(TARGET_CC) $(TARGET_CC_FLAGS) -fPIC -c $(APK_DIR)/rebax_android_entry.c -o $(APK_DIR)/rebax_android_entry.o; \
	$(TARGET_CC) $(TARGET_CC_FLAGS) -shared -o "$$stage/lib/$$abi/libmain.so" $(OBJS) $(APK_DIR)/rebax_android_entry.o $(TARGET_LDFLAGS) $(LDLIBS) $(ANDROID_PAYLOAD_LIBS); \
	if [ "$(SDL2_JAVA_SOURCES)" = "" ]; then echo "ERROR: SDL2 Android Java sources (org/libsdl/app/*.java) were not found automatically in the project, SDL2 source, or standard SDL2 roots; provide SDL2_SOURCE_DIR or SDL2_JAVA_SOURCES only if the SDL2 source is stored in a non-standard location"; exit 1; fi; \
	if [ "$(JAVAC)" = "" ]; then echo "ERROR: javac was not found; install a JDK to build the APK"; exit 1; fi; \
	if [ "$(ANDROID_JAR)" = "" ] || [ ! -f "$(ANDROID_JAR)" ]; then echo "ERROR: android.jar was not found; install an Android SDK platform to build the APK"; exit 1; fi; \
	echo "==> Compiling the Android Java layer"; \
	"$(JAVAC)" -nowarn -encoding UTF-8 -source 8 -target 8 -classpath "$(ANDROID_JAR)" -d "$$stage/classes" $(SDL2_JAVA_SOURCES); \
	echo "==> Generating classes.dex"; \
	classes="$$stage"/classes/org/libsdl/app/*.class; \
	if [ "$(D8)" != "" ]; then "$(D8)" --release --min-api 21 --lib "$(ANDROID_JAR)" --output "$$stage/dex" $$classes; \
	elif [ "$(D8_JAR)" != "" ] && [ "$(JAVA)" != "" ]; then "$(JAVA)" -cp "$(D8_JAR)" com.android.tools.r8.D8 --release --min-api 21 --lib "$(ANDROID_JAR)" --output "$$stage/dex" $$classes; \
	elif [ "$(DX)" != "" ]; then "$(DX)" --dex --output="$$stage/dex/classes.dex" $$classes; \
	else echo "ERROR: neither d8 nor dx was found; install the Android SDK build-tools"; exit 1; fi; \
	$(RTOOL) cp "$$stage/dex/classes.dex" "$$stage/classes.dex"; \
	echo "==> Writing AndroidManifest.xml"; \
	printf '%s' '<?xml version="1.0" encoding="utf-8"?>' > "$$stage/AndroidManifest.xml"; \
	printf '%s' '<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="org.rebax.engine" android:versionCode="1" android:versionName="$(patsubst v%,%,$(ENGINE_VERSION))">' >> "$$stage/AndroidManifest.xml"; \
	printf '%s' '<uses-sdk android:minSdkVersion="21" android:targetSdkVersion="$(APK_TARGET_SDK)"/>' >> "$$stage/AndroidManifest.xml"; \
	printf '%s' '<uses-feature android:glEsVersion="0x00020000" android:required="true"/>' >> "$$stage/AndroidManifest.xml"; \
	printf '%s' '<application android:label="Rebax" android:allowBackup="true" android:extractNativeLibs="true" android:hasCode="true">' >> "$$stage/AndroidManifest.xml"; \
	printf '%s' '<activity android:name="org.libsdl.app.SDLActivity" android:exported="true" android:launchMode="singleTop" android:screenOrientation="landscape" android:configChanges="keyboard|keyboardHidden|orientation|screenSize|screenLayout|uiMode" android:theme="@android:style/Theme.NoTitleBar.Fullscreen">' >> "$$stage/AndroidManifest.xml"; \
	printf '%s' '<intent-filter><action android:name="android.intent.action.MAIN"/><category android:name="android.intent.category.LAUNCHER"/></intent-filter>' >> "$$stage/AndroidManifest.xml"; \
	printf '%s' '<meta-data android:name="android.app.lib_name" android:value="main"/>' >> "$$stage/AndroidManifest.xml"; \
	printf '%s' '</activity></application></manifest>' >> "$$stage/AndroidManifest.xml"; \
	echo "==> Linking resources"; \
	if [ "$(AAPT2)" != "" ]; then "$(AAPT2)" link -o "$$stage/base.apk" --manifest "$$stage/AndroidManifest.xml" -I "$(ANDROID_JAR)" --min-sdk-version 21 --target-sdk-version $(APK_TARGET_SDK) --auto-add-overlay; \
	elif [ "$(AAPT)" != "" ]; then "$(AAPT)" package -f -M "$$stage/AndroidManifest.xml" -I "$(ANDROID_JAR)" -F "$$stage/base.apk"; \
	else echo "ERROR: neither aapt2 nor aapt was found; install the Android SDK build-tools"; exit 1; fi; \
	echo "==> Adding classes.dex and the native libraries"; \
	if [ "$(ZIP)" != "" ]; then ( cd "$$stage" && "$(ZIP)" -q -X base.apk classes.dex "lib/$$abi/libmain.so" "lib/$$abi/libSDL2.so" "lib/$$abi/libiconv.so" "lib/$$abi/libc++_shared.so" ); \
	elif [ "$(JAR)" != "" ]; then "$(JAR)" uf "$$stage/base.apk" -C "$$stage" classes.dex -C "$$stage" "lib/$$abi/libmain.so" -C "$$stage" "lib/$$abi/libSDL2.so" -C "$$stage" "lib/$$abi/libiconv.so" -C "$$stage" "lib/$$abi/libc++_shared.so"; \
	elif [ "$(PYTHON)" != "" ]; then "$(PYTHON)" -c "import sys,zipfile; z=zipfile.ZipFile(sys.argv[1],'a',zipfile.ZIP_DEFLATED); [z.write(p,p) for p in sys.argv[2:]]; z.close()" "$$stage/base.apk" "$$stage/classes.dex" "$$stage/lib/$$abi/libmain.so" "$$stage/lib/$$abi/libSDL2.so" "$$stage/lib/$$abi/libiconv.so" "$$stage/lib/$$abi/libc++_shared.so"; \
	else echo "ERROR: no zip, jar or python tool was found to place files inside the APK"; exit 1; fi; \
	echo "==> Aligning"; \
	if [ "$(ZIPALIGN)" != "" ]; then "$(ZIPALIGN)" -f 4 "$$stage/base.apk" "$$stage/aligned.apk"; else $(RTOOL) cp "$$stage/base.apk" "$$stage/aligned.apk"; fi; \
	echo "==> Signing"; \
	ks="$(CURDIR)/rebax-debug.keystore"; \
if [ ! -f "$$ks" ]; then \
    "$(KEYTOOL)" -genkeypair \
        -keystore "$$ks" \
        -storepass android \
        -alias rebax \
        -keypass android \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Rebax,O=Rebax,C=US"; \
fi; \
	if [ "$(KEYTOOL)" != "" ] && [ ! -f "$$ks" ]; then "$(KEYTOOL)" -genkeypair -keystore "$$ks" -storepass android -alias rebax -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Rebax,O=Rebax,C=US"; fi; \
	if [ "$(APKSIGNER)" != "" ] && [ -f "$$ks" ]; then "$(APKSIGNER)" sign --ks "$$ks" --ks-pass pass:android --key-pass pass:android --ks-key-alias rebax --out "$$stage/final.apk" "$$stage/aligned.apk"; \
	elif [ "$(JARSIGNER)" != "" ] && [ -f "$$ks" ]; then $(RTOOL) cp "$$stage/aligned.apk" "$$stage/final.apk"; "$(JARSIGNER)" -keystore "$$ks" -storepass android -keypass android -sigalg SHA256withRSA -digestalg SHA-256 "$$stage/final.apk" rebax >/dev/null; \
	else echo "Warning: no signing tool was found; the APK stays unsigned"; $(RTOOL) cp "$$stage/aligned.apk" "$$stage/final.apk"; fi; \
	$(RTOOL) cp "$$stage/final.apk" "$(APK_OUT)"; \
	$(RTOOL) rm "$$stage"; \
	echo "==> APK ready: $(APK_OUT)"

CFLAGS := -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I$(SRC_DIR) -MMD -MP $(SDL2_CFLAGS) $(ARCHIVE_CFLAGS)
ifeq ($(TARGET_PLATFORM),android)
  CFLAGS += -fPIC
endif
$(OBJ_DIR)/xz_embedded.o: CFLAGS += -O3
$(OBJ_DIR)/rebax_fs.o: CFLAGS += -O2
LDLIBS := -lm $(SDL2_LIBS)
