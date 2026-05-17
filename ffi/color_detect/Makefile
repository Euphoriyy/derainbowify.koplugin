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

ifneq ($(PLATFORM),amd64)
    ifeq ($(findstring gnueabihf,$(CC)),gnueabihf)
        CFLAGS = $(COMMON_FLAGS) $(ARM_FLAGS) $(HARD_FLOAT_FLAG)
    else
        CFLAGS = $(COMMON_FLAGS) $(ARM_FLAGS) $(SOFT_FLOAT_FLAG)
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
