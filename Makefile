SITE_BUILD_DIR ?= build/site
SITE_BOOT_IMAGE ?= boot/mini.raw
SITE_WEB_IMAGE ?= $(SITE_BUILD_DIR)/assets/taijios-web.raw

.PHONY: site clean-site serve-site driver-smoke

site:
	rm -rf "$(SITE_BUILD_DIR)"
	mkdir -p "$(SITE_BUILD_DIR)"
	cp -R site/. "$(SITE_BUILD_DIR)/"
	mkdir -p "$(SITE_BUILD_DIR)/assets"
	python3 scripts/make-web-image.py "$(SITE_BOOT_IMAGE)" site/assets/taijios-web-plan9.ini "$(SITE_WEB_IMAGE)"
	test -f "$(SITE_BUILD_DIR)/index.html"
	test -f "$(SITE_BUILD_DIR)/favicon.ico"
	test -f "$(SITE_BUILD_DIR)/css/style.css"
	test -f "$(SITE_BUILD_DIR)/js/taijios-web.js"
	test -f "$(SITE_BUILD_DIR)/assets/taijios-desktop.png"
	test -f "$(SITE_WEB_IMAGE)"
	test -f "$(SITE_BUILD_DIR)/assets/v86/libv86.js"
	test -f "$(SITE_BUILD_DIR)/assets/v86/v86.wasm"
	test -f "$(SITE_BUILD_DIR)/assets/v86/seabios.bin"
	test -f "$(SITE_BUILD_DIR)/assets/v86/vgabios.bin"

clean-site:
	rm -rf "$(SITE_BUILD_DIR)"

serve-site: site
	cd "$(SITE_BUILD_DIR)" && python3 -m http.server 8000

driver-smoke:
	sh scripts/driver-smoke.sh
