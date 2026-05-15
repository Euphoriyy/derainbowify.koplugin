DIRS := color_detect moire_filter_pffft

.PHONY: amd64 arm kobo kindle kindlehf pocketbook build clean

all: amd64 arm

amd64:
	$(MAKE) build CC=gcc PLATFORM=amd64

arm: kobo kindle kindlehf pocketbook
	@echo "Built for all ARM platforms..."

kobo:
	$(MAKE) build CC=arm-kobo-linux-gnueabihf-gcc PLATFORM=kobo

kindle:
	$(MAKE) build CC=arm-kindle-linux-gnueabi-gcc PLATFORM=kindle

kindlehf:
	$(MAKE) build CC=arm-kindlehf-linux-gnueabihf-gcc PLATFORM=kindlehf

pocketbook:
	$(MAKE) build CC=arm-pocketbook-linux-gnueabi-gcc PLATFORM=pocketbook

build:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir CC="$(CC)" PLATFORM="$(PLATFORM)"; \
	done

clean:
	rm -f ./build/*.so
