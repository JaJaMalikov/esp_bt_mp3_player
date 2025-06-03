#!/usr/bin/env bash
set -euo pipefail

# Update the package list and install system packages
#sudo apt-get update -y
#sudo apt-get install -y \
 #   git wget flex bison gperf cmake ninja-build ccache \
  #  libffi-dev libssl-dev python3 python3-pip python3-venv

# Directory where the ESP frameworks will be installed
mkdir -p "$HOME/esp"

# --- Install ESP-IDF ---------------------------------------------------------
IDF_VERSION="v5.4"
export IDF_PATH="$HOME/esp/esp-idf"

if [ ! -d "$IDF_PATH" ]; then
    git clone -b "$IDF_VERSION" --depth=1 https://github.com/espressif/esp-idf.git "$IDF_PATH"
fi

# Install the toolchain and Python packages for ESP-IDF
"$IDF_PATH/install.sh"           # download cross‑toolchain & Python env
"$IDF_PATH/tools/idf_tools.py" install

# --- Install ESP-ADF (Audio Development Framework) ---------------------------
export ADF_PATH="$HOME/esp/esp-adf"

if [ ! -d "$ADF_PATH" ]; then
    git clone --depth=1 https://github.com/espressif/esp-adf.git "$ADF_PATH"
fi

"$ADF_PATH/install.sh"           # installs additional ADF requirements

# Persist environment variables for future sessions
{
    echo "export IDF_PATH=${IDF_PATH}"
    echo "export ADF_PATH=${ADF_PATH}"
    echo "source \"${IDF_PATH}/export.sh\""
} >> "$HOME/.bashrc"

echo
echo "ESP-IDF and ESP-ADF installed."
echo "Open a new shell or run 'source ~/.bashrc' to use the toolchain."
