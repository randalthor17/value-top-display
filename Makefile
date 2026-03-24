PREFIX ?= /usr/local
SYSTEMD_USER_DIR ?= $(HOME)/.config/systemd/user

value-top-display: src/main.c
	$(CC) -O2 -o value-top-display src/main.c \
		$(shell pkg-config --cflags --libs libusb-1.0) \
		$(shell pkg-config --cflags --libs libsensors)

install: value-top-display
	install -Dm755 value-top-display $(DESTDIR)$(PREFIX)/bin/value-top-display
	install -Dm644 udev/99-value-top-display.rules \
		$(DESTDIR)/etc/udev/rules.d/99-value-top-display.rules
	install -Dm644 systemd/value-top-display.service \
		$(SYSTEMD_USER_DIR)/value-top-display.service

uninstall:
	rm -f $(PREFIX)/bin/value-top-display
	rm -f /etc/udev/rules.d/99-value-top-display.rules
	rm -f $(SYSTEMD_USER_DIR)/value-top-display.service

clean:
	rm -f value-top-display
