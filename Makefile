BUILD_DIR := builddir-linux
PRESET    := debug
LIB       := $(BUILD_DIR)/libtrux_lib.a
PREFIX    := /usr

.PHONY: all build test install uninstall clean

all: build

build:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

install: build
	sudo install -d -m 755 $(PREFIX)/lib
	sudo install -d -m 755 $(PREFIX)/lib/pkgconfig
	sudo install -d -m 755 $(PREFIX)/include/trux
	sudo install -m 644 $(LIB) $(PREFIX)/lib/libtrux_lib.a
	sudo cp -r include/trux/* $(PREFIX)/include/trux/
	sudo find $(PREFIX)/include/trux -type d -exec chmod 755 {} \;
	sudo find $(PREFIX)/include/trux -type f -exec chmod 644 {} \;
	sudo install -m 644 trux.pc $(PREFIX)/lib/pkgconfig/trux.pc

uninstall:
	sudo rm -f $(PREFIX)/lib/libtrux_lib.a
	sudo rm -rf $(PREFIX)/include/trux
	sudo rm -f $(PREFIX)/lib/pkgconfig/trux.pc

clean:
	rm -rf $(BUILD_DIR)
