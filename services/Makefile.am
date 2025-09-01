## Process this file with automake to produce Makefile.in

# Define the systemd service directory
systemdservicedir = /usr/lib/systemd/system

# Install system service files with 644 permissions
systemdservice_DATA = lcdd.service lcdproc.service ydotoold.service

# Make sure service files are included in distribution
EXTRA_DIST = $(systemdservice_DATA)

# Post-install hook to reload systemd and enable/start services
install-data-hook:
	@if [ -z "$(DESTDIR)" ] && [ -d "/usr/lib/systemd/system" ] && [ "$$(id -u)" = "0" ]; then \
		echo "Reloading systemd daemon..."; \
		systemctl daemon-reload; \
		echo "Enabling and starting lcdd service..."; \
		systemctl enable lcdd.service; \
		systemctl start lcdd.service; \
		echo "Enabling and starting lcdproc service..."; \
		systemctl enable lcdproc.service; \
		systemctl start lcdproc.service; \
		echo "Enabling and starting ydotoold service..."; \
		systemctl enable ydotoold.service; \
		systemctl start ydotoold.service; \
		echo "Checking service status..."; \
		failed_services=""; \
		if ! systemctl is-active --quiet lcdd.service; then \
			failed_services="$$failed_services lcdd.service"; \
		fi; \
		if ! systemctl is-active --quiet lcdproc.service; then \
			failed_services="$$failed_services lcdproc.service"; \
		fi; \
		if ! systemctl is-active --quiet ydotoold.service; then \
			failed_services="$$failed_services ydotoold.service"; \
		fi; \
		if [ -n "$$failed_services" ]; then \
			echo "Warning: Some services failed to start:$$failed_services"; \
			echo "Check with: systemctl status <service-name>"; \
		else \
			echo "Services installed and started successfully."; \
		fi; \
	elif [ -n "$(DESTDIR)" ]; then \
		echo "Installing to staging directory, services not started."; \
	elif [ "$$(id -u)" != "0" ]; then \
		echo "Not running as root, services installed but not started."; \
		echo "Run 'sudo systemctl daemon-reload && sudo systemctl enable --now lcdd.service lcdproc.service ydotoold.service' to start all services."; \
	else \
		echo "Systemd directory not found, services installed but not started."; \
	fi

# Pre-uninstall hook to stop and disable services before file removal
uninstall-hook:
	@if [ -z "$(DESTDIR)" ] && [ -d "/usr/lib/systemd/system" ] && [ "$$(id -u)" = "0" ]; then \
		echo "Stopping services..."; \
		systemctl stop lcdproc.service 2>/dev/null || true; \
		systemctl stop lcdd.service 2>/dev/null || true; \
		systemctl stop ydotoold.service 2>/dev/null || true; \
		echo "Disabling services..."; \
		systemctl disable lcdproc.service 2>/dev/null || true; \
		systemctl disable lcdd.service 2>/dev/null || true; \
		systemctl disable ydotoold.service 2>/dev/null || true; \
		echo "Services stopped and disabled."; \
	elif [ -n "$(DESTDIR)" ]; then \
		echo "Uninstalling from staging directory, services not touched."; \
	elif [ "$$(id -u)" != "0" ]; then \
		echo "Not running as root, services not stopped."; \
		echo "Run 'sudo systemctl stop lcdproc.service lcdd.service ydotoold.service && sudo systemctl disable lcdproc.service lcdd.service ydotoold.service' manually."; \
	else \
		echo "Systemd directory not found, no services to stop."; \
	fi

# Post-uninstall hook to reload systemd after file removal
uninstall-local:
	@if [ -z "$(DESTDIR)" ] && [ -d "/usr/lib/systemd/system" ] && [ "$$(id -u)" = "0" ]; then \
		echo "Reloading systemd daemon after service file removal..."; \
		systemctl daemon-reload; \
		echo "Service files removed and systemd reloaded."; \
	fi

## EOF