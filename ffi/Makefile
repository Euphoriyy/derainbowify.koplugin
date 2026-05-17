BACKEND := pffft
DIRS    := color_detect moire_filter_$(BACKEND)

GREEN   := $(shell tput setaf 2)
NC      := $(shell tput sgr0)

.PHONY: amd64 arm kobo kindle kindlehf pocketbook build clean

all: amd64 arm

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

build:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir CC="$(CC)" PLATFORM="$(PLATFORM)"; \
	done

clean:
	rm -f ./build/*.so
