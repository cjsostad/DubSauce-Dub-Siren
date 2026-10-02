#!/bin/bash
# Dub Siren V2 C++ Edition - One-line Installer for Raspberry Pi Zero 2W
# Usage: curl -sSL https://raw.githubusercontent.com/cjsostad/DubSauce-Dub-Siren/main/cpp/install.sh | bash

set -e

REPO_URL="https://github.com/cjsostad/DubSauce-Dub-Siren.git"
INSTALL_DIR="$HOME/dubsaucedubsiren"
BRANCH="${1:-main}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo ""
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Dub Siren V2 - C++ Edition${NC}"
echo -e "${CYAN}  Raspberry Pi Zero 2W + PCM5102 DAC${NC}"
echo -e "${CYAN}  High-Performance Native Build${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo ""

# Check if running on Raspberry Pi
if [ ! -f /proc/cpuinfo ] || ! grep -q "Raspberry Pi" /proc/cpuinfo 2>/dev/null; then
    echo -e "${YELLOW}⚠️  WARNING: Not running on Raspberry Pi!${NC}"
    echo "   This installer is designed for Raspberry Pi Zero 2W"
    echo "   GPIO and I2S features may not work correctly"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r </dev/tty
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check if C++ directory already exists
if [ -d "$INSTALL_DIR/cpp" ]; then
    echo -e "${YELLOW}⚠️  Existing installation found at $INSTALL_DIR${NC}"
    echo "   Options:"
    echo "   1) Update existing installation (git pull)"
    echo "   2) Fresh install (remove and re-clone)"
    echo "   3) Cancel"
    read -p "Choose option [1]: " INSTALL_CHOICE </dev/tty
    INSTALL_CHOICE=${INSTALL_CHOICE:-1}
    
    case $INSTALL_CHOICE in
        1)
            echo ""
            echo -e "${GREEN}📥 Updating existing installation...${NC}"
            cd "$INSTALL_DIR"
            git fetch origin
            git checkout "$BRANCH"
            git pull origin "$BRANCH"
            ;;
        2)
            echo ""
            echo -e "${YELLOW}🗑️  Removing existing installation...${NC}"
            cd "$HOME"
            rm -rf "$INSTALL_DIR"
            echo -e "${GREEN}✓ Old installation removed${NC}"
            
            echo ""
            echo -e "${GREEN}📥 Cloning fresh copy...${NC}"
            git clone -b "$BRANCH" "$REPO_URL" "$INSTALL_DIR" 2>&1 | grep -v "Cloning into" || true
            ;;
        *)
            echo "Installation cancelled."
            exit 0
            ;;
    esac
else
    # Fresh install
    echo -e "${GREEN}📦 Updating system packages...${NC}"
    sudo apt-get update -qq
    
    # Install git if not present
    if ! command -v git &> /dev/null; then
        echo -e "${GREEN}📦 Installing git...${NC}"
        sudo apt-get install -y git >/dev/null 2>&1
    fi
    
    echo -e "${GREEN}✓ System updated${NC}"
    
    # Clone repository
    echo ""
    echo -e "${GREEN}📥 Cloning Dub Siren V2 repository (C++ branch)...${NC}"
    git clone -b "$BRANCH" "$REPO_URL" "$INSTALL_DIR" 2>&1 | grep -v "Cloning into" || true
fi

# Change to C++ directory
cd "$INSTALL_DIR/cpp"

# Make scripts executable
chmod +x setup.sh build.sh

# Run setup
echo ""
echo -e "${GREEN}🔧 Running setup script...${NC}"
echo ""
./setup.sh

echo ""
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✅ Installation Complete!${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "📍 Installation directory: ${CYAN}$INSTALL_DIR/cpp${NC}"
echo ""
echo -e "${YELLOW}Next Steps:${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "1️⃣  Wire up your PCM5102 DAC (see wiring guide below)"
echo "2️⃣  Wire up your rotary encoders and switches"
echo "3️⃣  Reboot your Raspberry Pi:"
echo -e "    ${CYAN}sudo reboot${NC}"
echo ""
echo "4️⃣  After reboot, test in simulation mode:"
echo -e "    ${CYAN}~/dubsaucedubsiren/cpp/build/dubsiren --simulate --interactive${NC}"
echo ""
echo "5️⃣  Run on hardware:"
echo -e "    ${CYAN}~/dubsaucedubsiren/cpp/build/dubsiren${NC}"
echo ""
echo "6️⃣  Start/manage the service:"
echo -e "    ${CYAN}sudo systemctl start dubsiren-cpp.service${NC}"
echo -e "    ${CYAN}sudo systemctl status dubsiren-cpp.service${NC}"
echo ""
echo -e "${YELLOW}PCM5102 Wiring Guide:${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PCM5102 Pin    →  Raspberry Pi Zero 2W"
echo "  ───────────────────────────────────────"
echo "  VIN            →  3.3V (Pin 1)"
echo "  GND            →  GND (Pin 6)"
echo "  LCK            →  GPIO 18 (Pin 12) - I2S LRCLK"
echo "  BCK            →  GPIO 19 (Pin 35) - I2S BCLK"
echo "  DIN            →  GPIO 21 (Pin 40) - I2S DOUT"
echo "  SCK            →  GND (for 48kHz internal clock)"
echo "  FMT            →  GND (I2S format)"
echo "  XSMT           →  GND (soft mute OFF = audio ON)"
echo ""
echo -e "${YELLOW}Documentation:${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  📖 C++ Guide:     ~/dubsaucedubsiren/cpp/README.md"
echo "  🔧 Hardware:      ~/dubsaucedubsiren/HARDWARE.md"
echo "  🎛️  GPIO wiring:  ~/dubsaucedubsiren/GPIO_WIRING_GUIDE.md"
echo ""
echo "Need help? Visit: https://github.com/cjsostad/DubSauce-Dub-Siren"
echo ""
