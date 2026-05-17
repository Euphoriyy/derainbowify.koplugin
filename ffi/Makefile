DIRS := color_detect moire_filter_pffft

.PHONY: amd64 arm kobo kindle kindlehf pocketbook build clean

all: amd64 arm

amd64:
	$(MAKE) build CC=gcc CXX=g++ PLATFORM=amd64

arm: kobo kindle kindlehf pocketbook
	@echo "Built for all ARM platforms..."

kobo:
	$(MAKE) build CC=arm-kobo-linux-gnueabihf-gcc CXX=arm-kobo-linux-gnueabihf-g++ PLATFORM=kobo

kindle:
	$(MAKE) build CC=arm-kindle5-linux-gnueabi-gcc CXX=arm-kindle5-linux-gnueabi-g++ PLATFORM=kindle

kindlehf:
	$(MAKE) build CC=arm-kindlehf-linux-gnueabihf-gcc CXX=arm-kindlehf-linux-gnueabihf-g++ PLATFORM=kindlehf

pocketbook:
	$(MAKE) build CC=arm-pocketbook-linux-gnueabi-gcc CXX=arm-pocketbook-linux-gnueabi-g++ PLATFORM=pocketbook

build:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir CC="$(CC)" PLATFORM="$(PLATFORM)"; \
	done

clean:
	rm -f ./build/*.so
