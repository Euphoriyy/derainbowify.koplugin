CC ?= gcc

COMMON_FLAGS = \
    -O3 \
    -fPIC \
    -shared \
    -Wall \
    -fvisibility=hidden \
    -std=gnu99 \
    -D_GNU_SOURCE \
    -D_DEFAULT_SOURCE \
	-Ikissfft

ARM_FLAGS = \
    -march=armv7-a \
    -mfpu=neon-vfpv4 \
    -mfloat-abi=hard \
	-ffast-math \
	-funsafe-math-optimizations \
	-ftree-vectorize

ifeq ($(findstring arm-kobo-linux-gnueabihf,$(CC)),arm-kobo-linux-gnueabihf)
    CFLAGS = $(COMMON_FLAGS) $(ARM_FLAGS)
else
    CFLAGS = $(COMMON_FLAGS)
endif

LDFLAGS = \
    -Wl,--export-dynamic \
    -Wl,-rpath,'$$ORIGIN' \
    -lm

SRC = \
    moire_filter_kissfft.c \
    kissfft/kiss_fft.c \
    kissfft/kiss_fftnd.c

OUT = ../build/moire_filter.so

all: $(OUT)

$(OUT): $(SRC)
	mkdir -p ../build
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)
