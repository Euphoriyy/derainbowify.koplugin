DIRS := color_detect moire_filter_pffft

.PHONY: amd64 armv7 build clean

amd64:
	$(MAKE) build CC=gcc

armv7:
	$(MAKE) build CC=arm-kobo-linux-gnueabihf-gcc

build:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir CC="$(CC)"; \
	done

clean:
	rm -f ./build/*.so
