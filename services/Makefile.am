## Process this file with automake to produce Makefile.in

# Define the systemd service directory
systemdservicedir = /usr/lib/systemd/system

# Install system service files with 644 permissions
systemdservice_DATA = lcdd.service lcdproc.service ydotoold.service

# Make sure service files are included in distribution
EXTRA_DIST = $(systemdservice_DATA)

# ANSI color codes for colored terminal output
COLOR_RESET = \033[0m
COLOR_RED = \033[1;31m
COLOR_GREEN = \033[1;32m
COLOR_YELLOW = \033[1;33m
COLOR_BLUE = \033[1;34m
COLOR_CYAN = \033[1;36m

# Post-install hook to reload systemd and enable/start services
install-data-hook:
	@if [ -z "$(DESTDIR)" ] && [ -d "/usr/lib/systemd/system" ] && [ "$$(id -u)" = "0" ]; then \
		printf "$(COLOR_CYAN)Reloading systemd daemon...$(COLOR_RESET)\n"; \
		systemctl daemon-reload; \
		printf "$(COLOR_GREEN)✓ Systemd daemon reloaded$(COLOR_RESET)\n"; \
		printf "\n"; \
		printf "$(COLOR_BLUE)Enabling and starting lcdd service...$(COLOR_RESET)\n"; \
		if systemctl enable lcdd.service 2>&1; then \
			printf "$(COLOR_GREEN)✓ lcdd.service enabled$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ lcdd.service could not be enabled$(COLOR_RESET)\n"; \
		fi; \
		if systemctl start lcdd.service 2>&1; then \
			printf "$(COLOR_GREEN)✓ lcdd.service started successfully$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_RED)✗ lcdd.service failed to start$(COLOR_RESET)\n"; \
			printf "$(COLOR_YELLOW)Last log entries:$(COLOR_RESET)\n"; \
			journalctl -u lcdd.service --lines=5 --no-pager 2>/dev/null || true; \
		fi; \
		printf "\n"; \
		printf "$(COLOR_BLUE)Enabling and starting lcdproc service...$(COLOR_RESET)\n"; \
		if systemctl enable lcdproc.service 2>&1; then \
			printf "$(COLOR_GREEN)✓ lcdproc.service enabled$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ lcdproc.service could not be enabled$(COLOR_RESET)\n"; \
		fi; \
		if systemctl start lcdproc.service 2>&1; then \
			printf "$(COLOR_GREEN)✓ lcdproc.service started successfully$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_RED)✗ lcdproc.service failed to start$(COLOR_RESET)\n"; \
			printf "$(COLOR_YELLOW)Last log entries:$(COLOR_RESET)\n"; \
			journalctl -u lcdproc.service --lines=5 --no-pager 2>/dev/null || true; \
		fi; \
		printf "\n"; \
		printf "$(COLOR_BLUE)Enabling and starting ydotoold service...$(COLOR_RESET)\n"; \
		if systemctl enable ydotoold.service 2>&1; then \
			printf "$(COLOR_GREEN)✓ ydotoold.service enabled$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ ydotoold.service could not be enabled$(COLOR_RESET)\n"; \
		fi; \
		if systemctl start ydotoold.service 2>&1; then \
			printf "$(COLOR_GREEN)✓ ydotoold.service started successfully$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_RED)✗ ydotoold.service failed to start$(COLOR_RESET)\n"; \
			printf "$(COLOR_YELLOW)Last log entries:$(COLOR_RESET)\n"; \
			journalctl -u ydotoold.service --lines=5 --no-pager 2>/dev/null || true; \
		fi; \
		printf "\n"; \
		printf "$(COLOR_CYAN)Checking final service status...$(COLOR_RESET)\n"; \
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
			printf "$(COLOR_RED)⚠ Warning: Some services failed to start:$$failed_services$(COLOR_RESET)\n"; \
			printf "$(COLOR_YELLOW)Check status with: systemctl status <service-name>$(COLOR_RESET)\n"; \
			printf "$(COLOR_YELLOW)Check logs with: journalctl -u <service-name> -e$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_GREEN)✓ All services installed and started successfully.$(COLOR_RESET)\n"; \
		fi; \
	elif [ -n "$(DESTDIR)" ]; then \
		printf "$(COLOR_YELLOW)Installing to staging directory, services not started.$(COLOR_RESET)\n"; \
	elif [ "$$(id -u)" != "0" ]; then \
		printf "$(COLOR_YELLOW)Not running as root, services installed but not started.$(COLOR_RESET)\n"; \
		printf "$(COLOR_CYAN)Run 'sudo systemctl daemon-reload && sudo systemctl enable --now lcdd.service lcdproc.service ydotoold.service' to start all services.$(COLOR_RESET)\n"; \
	else \
		printf "$(COLOR_YELLOW)Systemd directory not found, services installed but not started.$(COLOR_RESET)\n"; \
	fi

# Pre-uninstall hook to stop and disable services before file removal
uninstall-hook:
	@if [ -z "$(DESTDIR)" ] && [ -d "/usr/lib/systemd/system" ] && [ "$$(id -u)" = "0" ]; then \
		printf "$(COLOR_CYAN)Stopping services...$(COLOR_RESET)\n"; \
		if systemctl stop lcdproc.service 2>/dev/null; then \
			printf "$(COLOR_GREEN)✓ lcdproc.service stopped$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ lcdproc.service was not running$(COLOR_RESET)\n"; \
		fi; \
		if systemctl stop lcdd.service 2>/dev/null; then \
			printf "$(COLOR_GREEN)✓ lcdd.service stopped$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ lcdd.service was not running$(COLOR_RESET)\n"; \
		fi; \
		if systemctl stop ydotoold.service 2>/dev/null; then \
			printf "$(COLOR_GREEN)✓ ydotoold.service stopped$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ ydotoold.service was not running$(COLOR_RESET)\n"; \
		fi; \
		printf "\n"; \
		printf "$(COLOR_CYAN)Disabling services...$(COLOR_RESET)\n"; \
		if systemctl disable lcdproc.service 2>/dev/null; then \
			printf "$(COLOR_GREEN)✓ lcdproc.service disabled$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ lcdproc.service was not enabled$(COLOR_RESET)\n"; \
		fi; \
		if systemctl disable lcdd.service 2>/dev/null; then \
			printf "$(COLOR_GREEN)✓ lcdd.service disabled$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ lcdd.service was not enabled$(COLOR_RESET)\n"; \
		fi; \
		if systemctl disable ydotoold.service 2>/dev/null; then \
			printf "$(COLOR_GREEN)✓ ydotoold.service disabled$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_YELLOW)⚠ ydotoold.service was not enabled$(COLOR_RESET)\n"; \
		fi; \
		printf "\n"; \
		printf "$(COLOR_GREEN)✓ All services stopped and disabled.$(COLOR_RESET)\n"; \
	elif [ -n "$(DESTDIR)" ]; then \
		printf "$(COLOR_YELLOW)Uninstalling from staging directory, services not touched.$(COLOR_RESET)\n"; \
	elif [ "$$(id -u)" != "0" ]; then \
		printf "$(COLOR_YELLOW)Not running as root, services not stopped.$(COLOR_RESET)\n"; \
		printf "$(COLOR_CYAN)Run 'sudo systemctl stop lcdproc.service lcdd.service ydotoold.service && sudo systemctl disable lcdproc.service lcdd.service ydotoold.service' manually.$(COLOR_RESET)\n"; \
	else \
		printf "$(COLOR_YELLOW)Systemd directory not found, no services to stop.$(COLOR_RESET)\n"; \
	fi

# Post-uninstall hook to reload systemd after file removal
uninstall-local:
	@if [ -z "$(DESTDIR)" ] && [ -d "/usr/lib/systemd/system" ] && [ "$$(id -u)" = "0" ]; then \
		printf "$(COLOR_CYAN)Reloading systemd daemon after service file removal...$(COLOR_RESET)\n"; \
		systemctl daemon-reload; \
		printf "$(COLOR_GREEN)✓ Service files removed and systemd reloaded.$(COLOR_RESET)\n"; \
	fi

## EOF
