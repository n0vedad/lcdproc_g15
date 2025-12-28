#!/bin/bash
# Development test helper for lcdproc-g15
# Automatically builds, installs, restarts services and monitors debug output

set -e

# Cleanup function to uninstall debug binaries
cleanup() {
    echo ""
    echo ""
    printf "${YELLOW}📊 Monitoring stopped${NC}\n"
    echo ""
    printf "${YELLOW}🧹 Stopping debug processes...${NC}\n"

    # Kill running LCDd and lcdproc processes
    cd "$REPO_DIR" 2> /dev/null || cd "$(dirname "$(dirname "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")")"
    killall -9 LCDd 2> /dev/null || true
    killall -9 lcdproc 2> /dev/null || true

    printf "${GREEN}✓ Debug processes stopped${NC}\n"

    echo ""
    printf "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
    printf "${YELLOW}⚠  Development mode still active${NC}\n"
    printf "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
    echo ""
    printf "${GREEN}  ▸ Resume debugging:${NC}    make debug\n"
    printf "${RED}  ▸ Production build:${NC}    make distclean && make\n"
    printf "${YELLOW}  ▸ New dev build:${NC}    make distclean && make dev\n"
    echo ""

    exit 0
}

# Trap SIGINT (Ctrl+C) and exit cleanly with cleanup
trap cleanup INT

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Parse command line arguments for report level
# LCDd report levels: 0=CRIT, 1=ERR, 2=WARNING, 3=NOTICE, 4=INFO, 5=DEBUG
REPORT_LEVEL="5" # Default: -r 5 (debug, shows all)
LEVEL_NAME="DEBUG (all messages)"
case "$1" in
    --critical | --crit | -c)
        REPORT_LEVEL="0"
        LEVEL_NAME="CRITICAL only"
        ;;
    --error | --errors | -e)
        REPORT_LEVEL="1"
        LEVEL_NAME="ERROR and CRITICAL"
        ;;
    --warning | --warnings | -w)
        REPORT_LEVEL="2"
        LEVEL_NAME="WARNING, ERROR and CRITICAL"
        ;;
    --info | -i)
        REPORT_LEVEL="4"
        LEVEL_NAME="INFO, WARNING, ERROR and CRITICAL"
        ;;
    --debug | -d | --all | -a | "")
        REPORT_LEVEL="5"
        LEVEL_NAME="DEBUG (all messages)"
        ;;
    *)
        echo "Unknown option: $1"
        echo "Usage: $0 [--critical|--error|--warning|--info|--debug]"
        echo "  --critical = Only critical errors (LCDd -r 0)"
        echo "  --error    = Errors and critical (LCDd -r 1)"
        echo "  --warning  = Warnings, errors and critical (LCDd -r 2)"
        echo "  --info     = Info and above (LCDd -r 4)"
        echo "  --debug    = All debug messages (LCDd -r 5, default)"
        exit 1
        ;;
esac

# Configuration
# Script is located in tests/debug/ subdirectory, so repo root is two levels up
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
LOG_FILE="/tmp/lcdproc-dev-test.log"

# Check if running in development mode
if [ ! -f "$REPO_DIR/config.h" ] || ! grep -q "^#define DEBUG" "$REPO_DIR/config.h" 2> /dev/null; then
    printf "${RED}❌ Development build required${NC}\n"
    printf "${YELLOW}💡 Run 'make distclean && make dev' first${NC}\n"
    exit 1
fi

cd "$REPO_DIR"

printf "${BLUE}=== Development Build & Test ===${NC}\n"
echo

# Step 1: Build
printf "${YELLOW}📦 Building project...${NC}\n"
# Use 'make -f Makefile' to bypass GNUmakefile's configuration checks
make -f Makefile -j$(nproc) 2>&1 | tee "$LOG_FILE.build"
if [ ${PIPESTATUS[0]} -eq 0 ]; then
    printf "${GREEN}✓ Build successful${NC}\n"
else
    printf "${RED}✗ Build failed - check $LOG_FILE.build${NC}\n"
    exit 1
fi
echo

# Step 2: USB reset (before starting processes)
if true; then
    printf "${YELLOW}🔌 Resetting Logitech G-series keyboard...${NC}\n"

    # Find Logitech G-series keyboard automatically
    # Supported models: G13 (c21c), G15 v1 (c222), G15 v2 (c227), G510 (c22d), G510s (c22e), G19 (c229)
    find_logitech_g_keyboard() {
        local g_products="c21c c222 c227 c22d c22e c229"

        for dev in /sys/bus/usb/devices/*; do
            if [ -f "$dev/idVendor" ] && [ -f "$dev/idProduct" ]; then
                local vendor=$(cat "$dev/idVendor")
                local product=$(cat "$dev/idProduct")

                # Check if it's a Logitech device (046d) with a G-series product ID
                if [ "$vendor" = "046d" ]; then
                    for g_product in $g_products; do
                        if [ "$product" = "$g_product" ]; then
                            echo "$(basename "$dev"):$product"
                            return 0
                        fi
                    done
                fi
            fi
        done
        return 1
    }

    # Find device
    DEVICE_INFO=$(find_logitech_g_keyboard)

    if [ -z "$DEVICE_INFO" ]; then
        printf "${RED}✗ No Logitech G-series keyboard found${NC}\n"
        printf "${YELLOW}  Supported: G13, G15 v1/v2, G510/G510s, G19${NC}\n"
        exit 1
    fi

    BUS_ID="${DEVICE_INFO%:*}"
    PRODUCT_ID="${DEVICE_INFO#*:}"

    # Map product ID to model name
    case "$PRODUCT_ID" in
        c21c) MODEL="G13" ;;
        c222) MODEL="G15 v1" ;;
        c227) MODEL="G15 v2" ;;
        c22d) MODEL="G510" ;;
        c22e) MODEL="G510s" ;;
        c229) MODEL="G19" ;;
        *) MODEL="Unknown" ;;
    esac

    printf "${BLUE}  Found $MODEL (046d:$PRODUCT_ID) at USB bus ID: $BUS_ID${NC}\n"
    echo

    # Unbind device
    printf "${YELLOW}  Unbinding USB device...${NC}\n"
    if sudo bash -c "echo '$BUS_ID' > /sys/bus/usb/drivers/usb/unbind" 2> /dev/null; then
        printf "${GREEN}  ✓ Unbind successful${NC}\n"
    else
        printf "${RED}  ✗ Unbind failed${NC}\n"
    fi

    sleep 2

    # Rebind device
    echo
    printf "${YELLOW}  Rebinding USB device...${NC}\n"
    if sudo bash -c "echo '$BUS_ID' > /sys/bus/usb/drivers/usb/bind" 2> /dev/null; then
        printf "${GREEN}  ✓ Bind successful${NC}\n"
    else
        printf "${RED}  ✗ Bind failed${NC}\n"
    fi

    # Wait for device initialization
    echo
    printf "${YELLOW}  Waiting for hardware initialization...${NC}\n"
    sleep 2
    udevadm settle 2> /dev/null || true

    # Wait for hidraw device to be ready (max 10 seconds)
    printf "${YELLOW}  Waiting for hidraw device...${NC}\n"
    TIMEOUT=10
    ELAPSED=0
    HIDRAW_READY=0

    while [ $ELAPSED -lt $TIMEOUT ]; do
        # Check if any hidraw device for G510 exists and is accessible
        for hidraw in /dev/hidraw*; do
            if [ -c "$hidraw" ]; then
                # Check if this hidraw belongs to our G510
                HIDRAW_NUM=$(basename "$hidraw" | sed 's/hidraw//')
                if [ -f "/sys/class/hidraw/hidraw${HIDRAW_NUM}/device/uevent" ]; then
                    if grep -q "HID_NAME=Logitech.*G.*510" "/sys/class/hidraw/hidraw${HIDRAW_NUM}/device/uevent" 2> /dev/null; then
                        HIDRAW_READY=1
                        break
                    fi
                fi
            fi
        done

        if [ $HIDRAW_READY -eq 1 ]; then
            break
        fi

        sleep 0.5
        ELAPSED=$((ELAPSED + 1))
    done

    if [ $HIDRAW_READY -eq 1 ]; then
        printf "${GREEN}  ✓ Hidraw device ready${NC}\n"
    else
        printf "${YELLOW}  ⚠ Timeout waiting for hidraw device${NC}\n"
    fi

    # Additional settling time
    sleep 2

    # Verify device is back
    if [ -d "/sys/bus/usb/devices/$BUS_ID" ]; then
        printf "${GREEN}✓ USB reset complete${NC}\n"
    else
        printf "${YELLOW}⚠ Device not found after rebind${NC}\n"
    fi

    echo
fi

# Step 3: Kill any existing LCDd/lcdproc processes before starting
printf "${YELLOW}🧹 Cleaning up old processes...${NC}\n"
killall -9 LCDd 2> /dev/null || true
killall -9 lcdproc 2> /dev/null || true
sleep 1
printf "${GREEN}✓ Old processes cleaned${NC}\n"
echo

# Clear old log files to ensure fresh output
rm -f /tmp/lcdd.log /tmp/lcdproc.log

# Step 4: Create temporary LCDd config with correct DriverPath
printf "${YELLOW}🚀 Starting LCDd server (Report Level: $LEVEL_NAME)...${NC}\n"
# Create temp config with DriverPath pointing to build directory
TEMP_CONF="/tmp/lcdd-debug.conf"
sed "s|^DriverPath=.*|DriverPath=$REPO_DIR/server/drivers/|" "$REPO_DIR/server/LCDd.conf" > "$TEMP_CONF"

# Start LCDd in foreground mode (-f) with selected report level (-r $REPORT_LEVEL)
# Note: Runs as current user - hidraw permissions should be set via udev rules
"$REPO_DIR/server/LCDd" -c "$TEMP_CONF" -f -r "$REPORT_LEVEL" > /tmp/lcdd.log 2>&1 &
LCDD_PID=$!
sleep 2

if ps -p $LCDD_PID > /dev/null 2>&1; then
    printf "${GREEN}✓ LCDd started (PID: $LCDD_PID)${NC}\n"
else
    printf "${RED}✗ LCDd failed to start${NC}\n"
    cat /tmp/lcdd.log
    exit 1
fi
echo

# Step 5: Start lcdproc client directly from build directory
printf "${YELLOW}🚀 Starting lcdproc client (Report Level: $LEVEL_NAME)...${NC}\n"
# Use -f to run in foreground (no daemon) and -r for report level
"$REPO_DIR/clients/lcdproc/lcdproc" -c "$REPO_DIR/clients/lcdproc/lcdproc.conf" -f -r "$REPORT_LEVEL" > /tmp/lcdproc.log 2>&1 &
LCDPROC_PID=$!
sleep 2

if ps -p $LCDPROC_PID > /dev/null 2>&1; then
    printf "${GREEN}✓ lcdproc started (PID: $LCDPROC_PID)${NC}\n"
else
    printf "${RED}✗ lcdproc failed to start${NC}\n"
    cat /tmp/lcdproc.log
    exit 1
fi
echo

# Step 6: Monitor output
printf "${GREEN}=== Debug Processes Running ===${NC}\n"
printf "${BLUE}📊 Monitoring $LEVEL_NAME (Ctrl+C to stop)...${NC}\n"
printf "${YELLOW}  LCDd log: /tmp/lcdd.log (LCDd -r $REPORT_LEVEL)${NC}\n"
printf "${YELLOW}  lcdproc log: /tmp/lcdproc.log (lcdproc -r $REPORT_LEVEL)${NC}\n"
echo

# Wait for log files to have content (give processes time to start logging)
sleep 1

# Show initial content (including GPL banner), then follow new lines
(
    # Show existing content first, then follow
    cat /tmp/lcdd.log 2> /dev/null
    cat /tmp/lcdproc.log 2> /dev/null
    # Now follow both files for new content
    tail -f -n 0 /tmp/lcdd.log &
    tail -f -n 0 /tmp/lcdproc.log
) 2> /dev/null | tee "$LOG_FILE"
