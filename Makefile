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
	sudo mkdir -p $(PREFIX)/lib
	sudo mkdir -p $(PREFIX)/include/trux
	sudo cp $(LIB) $(PREFIX)/lib/libtrux_lib.a
	sudo cp -r include/trux/* $(PREFIX)/include/trux/

uninstall:
	sudo rm -f $(PREFIX)/lib/libtrux_lib.a
	sudo rm -rf $(PREFIX)/include/trux

clean:
	rm -rf $(BUILD_DIR)
