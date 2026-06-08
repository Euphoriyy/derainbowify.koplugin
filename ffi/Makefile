BACKEND := pffft
DIRS    := color_detect moire_filter_$(BACKEND)

ANDROID_TOOLCHAIN      := "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin"
ANDROID_API            ?= 21
ANDROID_ARM_COMPILER   := "$(ANDROID_TOOLCHAIN)/armv7a-linux-androideabi$(ANDROID_API)"
ANDROID_ARM64_COMPILER := "$(ANDROID_TOOLCHAIN)/aarch64-linux-android$(ANDROID_API)"

GREEN   := $(shell tput setaf 2)
NC      := $(shell tput sgr0)

.PHONY: amd64 arm kobo kindle kindlehf pocketbook android check-ndk android-arm android-arm64 build clean

ifdef ANDROID_NDK_HOME
all: amd64 arm android
else
all: amd64 arm
	@echo "$(GREEN)>> Skipping Android builds... (ANDROID_NDK_HOME not set)$(NC)"
endif

amd64:
	@echo "$(GREEN)>> Building for Desktop (Linux)...$(NC)"
	$(MAKE) build CC=gcc CXX=g++ PLATFORM=amd64

arm: kobo kindle kindlehf pocketbook
	@echo "$(GREEN)>> Built for all ARM platforms...$(NC)"

kobo:
	@echo "$(GREEN)>> Building for Kobo...$(NC)"
	$(MAKE) build CC=arm-kobo-linux-gnueabihf-gcc CXX=arm-kobo-linux-gnueabihf-g++ PLATFORM=kobo

kindle:
	@echo "$(GREEN)>> Building for Kindle...$(NC)"
	$(MAKE) build CC=arm-kindle5-linux-gnueabi-gcc CXX=arm-kindle5-linux-gnueabi-g++ PLATFORM=kindle

kindlehf:
	@echo "$(GREEN)>> Building for Kindle (hard float)...$(NC)"
	$(MAKE) build CC=arm-kindlehf-linux-gnueabihf-gcc CXX=arm-kindlehf-linux-gnueabihf-g++ PLATFORM=kindlehf

pocketbook:
	@echo "$(GREEN)>> Building for Pocketbook...$(NC)"
	$(MAKE) build CC=arm-pocketbook-linux-gnueabi-gcc CXX=arm-pocketbook-linux-gnueabi-g++ PLATFORM=pocketbook

android: android-arm android-arm64
	@echo "$(GREEN)>> Built for all Android platforms...$(NC)"

android-arm:
	@echo "$(GREEN)>> Building for Android (32-bit)...$(NC)"
	$(MAKE) build CC="$(ANDROID_ARM_COMPILER)-clang" CXX="$(ANDROID_ARM_COMPILER)-clang++" PLATFORM=android-arm

android-arm64:
	@echo "$(GREEN)>> Building for Android (64-bit)...$(NC)"
	$(MAKE) build CC="$(ANDROID_ARM64_COMPILER)-clang" CXX="$(ANDROID_ARM64_COMPILER)-clang++" PLATFORM=android-arm64

build:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir CC="$(CC)" PLATFORM="$(PLATFORM)"; \
	done

clean:
	rm -f ./build/*.so
