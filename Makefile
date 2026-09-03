BUILD_DIR := builddir-linux
PRESET    := release
LIB       := $(BUILD_DIR)/libtrux_lib.a
DOCS_HTML := $(BUILD_DIR)/docs/html
PREFIX    := /usr

.PHONY: all build docs test install uninstall clean

all: build

build:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

docs: build
	-cmake --build --preset $(PRESET) --target trux_docs

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

install: docs
	sudo install -d -m 755 $(PREFIX)/lib
	sudo install -d -m 755 $(PREFIX)/lib/pkgconfig
	sudo install -d -m 755 $(PREFIX)/include/trux
	sudo install -m 644 $(LIB) $(PREFIX)/lib/libtrux_lib.a
	sudo cp -r include/trux/* $(PREFIX)/include/trux/
	sudo find $(PREFIX)/include/trux -type d -exec chmod 755 {} \;
	sudo find $(PREFIX)/include/trux -type f -exec chmod 644 {} \;
	sudo install -m 644 trux.pc $(PREFIX)/lib/pkgconfig/trux.pc
	if [ -d "$(DOCS_HTML)" ]; then \
		sudo install -d -m 755 $(PREFIX)/share/doc/trux/html; \
		sudo cp -r $(DOCS_HTML)/* $(PREFIX)/share/doc/trux/html/; \
		sudo find $(PREFIX)/share/doc/trux/html -type d -exec chmod 755 {} \;; \
		sudo find $(PREFIX)/share/doc/trux/html -type f -exec chmod 644 {} \;; \
	else \
		echo "docs: $(DOCS_HTML) not found (doxygen missing?) - skipping doc install"; \
	fi

uninstall:
	sudo rm -f $(PREFIX)/lib/libtrux_lib.a
	sudo rm -rf $(PREFIX)/include/trux
	sudo rm -f $(PREFIX)/lib/pkgconfig/trux.pc
	sudo rm -rf $(PREFIX)/share/doc/trux

clean:
	rm -rf $(BUILD_DIR)
