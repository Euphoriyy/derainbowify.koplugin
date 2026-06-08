CC       ?= gcc
PLATFORM ?= amd64

COMMON_FLAGS = \
    -O3 \
    -fPIC \
    -shared \
    -Wall \
    -fvisibility=hidden \
    -std=gnu99 \
    -D_GNU_SOURCE \
    -D_DEFAULT_SOURCE

ARM_FLAGS = \
    -march=armv7-a \
    -mfpu=neon-vfpv4 \
    -ffast-math \
    -funsafe-math-optimizations \
    -ftree-vectorize

HARD_FLOAT_FLAG = -mfloat-abi=hard
SOFT_FLOAT_FLAG = -mfloat-abi=softfp

ANDROID_FLAGS = \
    -static-libgcc \
    -funwind-tables \
    -fstack-protector \
    -no-canonical-prefixes

ANDROID_ARM32_FLAGS = \
    -march=armv7-a \
    -mfpu=vfpv3-d16 \
    -mthumb

ifneq ($(PLATFORM),amd64)
    ifneq ($(filter android%,$(PLATFORM)),)
        ifeq ($(findstring arm64,$(PLATFORM)),arm64)
            CFLAGS = $(COMMON_FLAGS) $(ANDROID_FLAGS)
        else
            CFLAGS = $(COMMON_FLAGS) $(ANDROID_FLAGS) $(ANDROID_ARM32_FLAGS)
        endif
    else
        ifeq ($(findstring gnueabihf,$(CC)),gnueabihf)
            CFLAGS = $(COMMON_FLAGS) $(ARM_FLAGS) $(HARD_FLOAT_FLAG)
        else
            CFLAGS = $(COMMON_FLAGS) $(ARM_FLAGS) $(SOFT_FLOAT_FLAG)
        endif
    endif
else
    CFLAGS = $(COMMON_FLAGS)
endif

LDFLAGS = \
    -Wl,--export-dynamic \
    -Wl,-rpath,'$$ORIGIN'

SRC = color_detect.c
OUT = ../build/color_detect-$(PLATFORM).so

all: $(OUT)

$(OUT): $(SRC)
	mkdir -p ../build
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)
