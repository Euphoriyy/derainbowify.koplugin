CXX      ?= g++
PLATFORM ?= amd64

COMMON_FLAGS = \
    -O3 \
    -fPIC \
    -shared \
    -Wall \
    -fvisibility=hidden \
    -std=c++11 \
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
    ifeq ($(findstring gnueabihf,$(CXX)),gnueabihf)
        CXXFLAGS = $(COMMON_FLAGS) $(ARM_FLAGS) $(HARD_FLOAT_FLAG)
    else
        CXXFLAGS = $(COMMON_FLAGS) $(ARM_FLAGS) $(SOFT_FLOAT_FLAG)
    endif
else
    CXXFLAGS = $(COMMON_FLAGS)
endif

LDFLAGS = \
    -Wl,--export-dynamic \
    -Wl,-rpath,'$$ORIGIN' \
    -lm \
	-lpthread

SRC = moire_filter_pocketfft.cpp

OUT = ../build/moire_filter-$(PLATFORM).so

all: $(OUT)

$(OUT): $(SRC)
	mkdir -p ../build
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)
