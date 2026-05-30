#!/bin/bash

# Test variables
TEST_FAIL_PROTON=0
TEST_FAIL_WINE=0
TEST_FAIL_PROTON_GE=0

# Path to the executable in the current directory
EXE_PATH="$(pwd)/FLibrary.exe"

# Verify the executable exists in the current directory
if [ ! -f "$EXE_PATH" ]; then
    echo "Error: FLibrary.exe not found in the current directory."
    exit 1
else
    echo "FLibrary.exe found."
fi

# Make sure the executable is executable (in case it's necessary)
chmod +x "$EXE_PATH"

# Check for the highest version of Proton GE in the specified path
if [ "$TEST_FAIL_PROTON_GE" -eq 1 ]; then
    PROTON_GE_PATH=""
else
    PROTON_GE_PATH=$(find "$HOME/.local/share/Steam/compatibilitytools.d" -maxdepth 3 -type d -name "GE-Proton*" | sort -V | tail -n 1)
fi

# If Proton GE is not found, check for Proton Experimental in the default Steam path and the user's home folder
if [ -z "$PROTON_GE_PATH" ]; then
    echo "Proton GE not found. Falling back on Proton Experimental."
    if [ "$TEST_FAIL_PROTON" -eq 1 ]; then
        PROTON_PATH=""
    else
        PROTON_PATH=$(find "$HOME/.local/share/Steam/steamapps/common" "$HOME/.steam/steam/steamapps/common" -maxdepth 2 -type d -name "Proton - Experimental" -print -quit)
    fi
else
    PROTON_PATH=""
fi

if [ -z "$PROTON_GE_PATH" ] && [ -z "$PROTON_PATH" ]; then
    echo "Proton GE and Proton Experimental not found. Falling back on Wine."
    if [ "$TEST_FAIL_WINE" -eq 1 ]; then
        command_wine="false"
    else
        command_wine=$(command -v wine)
    fi

    if [ -z "$command_wine" ]; then
        echo "Error: Neither Proton GE, Proton Experimental, nor Wine is installed."
        exit 1
    fi

    "$command_wine" "$EXE_PATH"


    # Check for errors in running
    if [ $? -ne 0 ]; then
        echo "Error: Failed to run FLibrary.exe with Wine."
        exit 1
    else
        echo "Successfully ran FLibrary.exe with Wine."
    fi
elif [ -n "$PROTON_GE_PATH" ]; then
    echo "Proton GE found: $PROTON_GE_PATH. Launching FLibrary.exe..."

    # Set up compatibility data path for Proton GE
    COMPAT_DATA_PATH="$HOME/.steam/steam/steamapps/compatdata/FLibrary"
    mkdir -p "$COMPAT_DATA_PATH/pfx"
    export STEAM_COMPAT_DATA_PATH="$COMPAT_DATA_PATH"
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.steam/steam"

    "$PROTON_GE_PATH/proton" run "$EXE_PATH"

    # Check for errors in running
    if [ $? -ne 0 ]; then
        echo "Error: Failed to run FLibrary.exe with Proton GE."
        exit 1
    else
        echo "Successfully ran FLibrary.exe with Proton GE."
    fi
else
    echo "Proton Experimental found: $PROTON_PATH. Launching FLibrary.exe..."

    # Set up compatibility data path for Proton Experimental
    COMPAT_DATA_PATH="$HOME/.steam/steam/steamapps/compatdata/FLibrary"
    mkdir -p "$COMPAT_DATA_PATH/pfx"
    export STEAM_COMPAT_DATA_PATH="$COMPAT_DATA_PATH"
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.steam/steam"

    "$PROTON_PATH/proton" run "$EXE_PATH"

    # Check for errors in running
    if [ $? -ne 0 ]; then
        echo "Error: Failed to run FLibrary.exe with Proton Experimental."
        exit 1
    else
        echo "Successfully ran FLibrary.exe with Proton Experimental."
    fi
fi

echo "Script execution completed."

