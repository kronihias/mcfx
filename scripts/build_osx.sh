#!/bin/bash
# Build mcfx plugins for macOS
#
# Usage: ./build_osx.sh [--vst2] [--vst3] [--au] [--standalone]
#        With no flags, builds all formats.

set -e

ROOT=$(cd "$(dirname "$0")/.."; pwd)
BUILD_DIR=$ROOT/build
VERSION=$(<"$ROOT/VERSION")

# Load codesigning credentials
CODESIGN_ENV="$ROOT/scripts/codesign.env"
if [ ! -f "$CODESIGN_ENV" ]; then
    echo "Error: $CODESIGN_ENV not found. Create it with your codesigning credentials."
    exit 1
fi
source "$CODESIGN_ENV"

# =========================================================
# Parse arguments
# =========================================================

BUILD_VST2=false
BUILD_VST3=false
BUILD_AU=false
BUILD_STANDALONE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --vst2)       BUILD_VST2=true ;;
        --vst3)       BUILD_VST3=true ;;
        --au)         BUILD_AU=true ;;
        --standalone) BUILD_STANDALONE=true ;;
        *)            echo "Unknown option: $1"; echo "Usage: $0 [--vst2] [--vst3] [--au] [--standalone]"; exit 1 ;;
    esac
    shift
done

# If no flags given, build all
if ! $BUILD_VST2 && ! $BUILD_VST3 && ! $BUILD_AU && ! $BUILD_STANDALONE; then
    BUILD_VST2=true
    BUILD_VST3=true
    BUILD_AU=true
    BUILD_STANDALONE=true
fi

# =========================================================
# Helper functions
# =========================================================

codesign_bundles() {
    local dir="$1"
    local ext="$2"
    find "$dir" -type d -name "*.$ext" -print0 2>/dev/null | while IFS= read -r -d '' bundle; do
        codesign -s "$CODESIGN_APP" \
                 --deep --strict --force --verbose --timestamp --options=runtime \
                 "$bundle"
    done
}

build_installer() {
    local pkg_root="$1"
    local identifier="$2"
    local install_location="$3"
    local installer_name="$4"
    local unsigned="${BUILD_DIR}/$(basename "$installer_name" .pkg)_unsigned.pkg"

    pkgbuild --root "${pkg_root}" --identifier "${identifier}" --version ${VERSION} --install-location "${install_location}" "${unsigned}"

    productsign --sign "$CODESIGN_INSTALLER" "${unsigned}" "${installer_name}"
    rm -rf "${unsigned}"

    pkgutil --check-signature "${installer_name}"

    echo ""; echo "notarizing $(basename "$installer_name")"
    xcrun notarytool submit "${installer_name}" --apple-id "$NOTARIZE_APPLE_ID" --password "$NOTARIZE_PASSWORD" --team-id "$NOTARIZE_TEAM_ID" --wait
    xcrun stapler staple "${installer_name}"

    echo ""; echo "verifying $(basename "$installer_name")"
    stapler validate "${installer_name}"
    spctl -a -vvv --assess --type install "${installer_name}"
}

# =========================================================
# Clean and prepare
# =========================================================

rm -rf "$BUILD_DIR" 2> /dev/null || sudo rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$ROOT/_OSX_RELEASE"

# VST2SDKPATH is always passed so mcfx_anything can host VST2 plugins
VST2SDK="${VST2SDKPATH:-$HOME/SDKs/vstsdk2.4}"

# =========================================================
# VST2 — one build per channel count
# =========================================================

if $BUILD_VST2; then
    VST_DIR=$BUILD_DIR/vst

    for numch in 2 4 8 16 24 32 36 50 64 84 128; do
        echo ""; echo "=== Building VST2 ${numch}ch v$VERSION ==="

        pushd "$BUILD_DIR"
        cmake .. -G Ninja \
            -DNUM_CHANNELS:STRING=$numch \
            -DBUILD_VST=TRUE \
            -DBUILD_VST3=FALSE \
            -DBUILD_AU=FALSE \
            -DBUILD_STANDALONE=FALSE \
            -DMCFX_BUILD_VST2_PER_CHANNEL=ON \
            -DMCFX_BUILD_MC=OFF \
            -DVST2SDKPATH="$VST2SDK"
        ninja
        popd
    done

    echo ""; echo "codesigning VST2 plugins"
    codesign_bundles "$VST_DIR" "vst"

    INSTALLER=${ROOT}/_OSX_RELEASE/mcfx_v${VERSION}_macos_vst2.pkg
    build_installer "$VST_DIR" "com.kronlachner.mcfx.vst2" "/Library/Audio/Plug-Ins/VST/mcfx" "$INSTALLER"
fi

# =========================================================
# VST3 — single multichannel build
# =========================================================

if $BUILD_VST3; then
    VST3_DIR=$BUILD_DIR/vst3

    echo ""; echo "=== Building VST3 multichannel v$VERSION ==="

    pushd "$BUILD_DIR"
    cmake .. -G Ninja \
        -DBUILD_VST=TRUE \
        -DBUILD_VST3=TRUE \
        -DBUILD_AU=FALSE \
        -DBUILD_STANDALONE=FALSE \
        -DMCFX_BUILD_VST2_PER_CHANNEL=OFF \
        -DMCFX_BUILD_MC=ON \
        -DVST2SDKPATH="$VST2SDK"
    ninja
    popd

    echo ""; echo "codesigning VST3 plugins"
    codesign_bundles "$VST3_DIR" "vst3"

    INSTALLER=${ROOT}/_OSX_RELEASE/mcfx_v${VERSION}_macos_vst3.pkg
    build_installer "$VST3_DIR" "com.kronlachner.mcfx.vst3" "/Library/Audio/Plug-Ins/VST3/mcfx" "$INSTALLER"
fi

# =========================================================
# AudioUnit — single multichannel build
# =========================================================

if $BUILD_AU; then
    AU_DIR=$BUILD_DIR/au

    echo ""; echo "=== Building AudioUnit multichannel v$VERSION ==="

    pushd "$BUILD_DIR"
    cmake .. -G Ninja \
        -DBUILD_VST=TRUE \
        -DBUILD_VST3=FALSE \
        -DBUILD_AU=TRUE \
        -DBUILD_STANDALONE=FALSE \
        -DMCFX_BUILD_VST2_PER_CHANNEL=OFF \
        -DMCFX_BUILD_MC=ON \
        -DVST2SDKPATH="$VST2SDK"
    ninja
    popd

    echo ""; echo "codesigning AudioUnit plugins"
    codesign_bundles "$AU_DIR" "component"

    INSTALLER=${ROOT}/_OSX_RELEASE/mcfx_v${VERSION}_macos_au.pkg
    build_installer "$AU_DIR" "com.kronlachner.mcfx.au" "/Library/Audio/Plug-Ins/Components/mcfx" "$INSTALLER"
fi

# =========================================================
# Standalone — single multichannel build
# =========================================================

if $BUILD_STANDALONE; then
    STANDALONE_DIR=$BUILD_DIR/standalone

    echo ""; echo "=== Building Standalone multichannel v$VERSION ==="

    pushd "$BUILD_DIR"
    cmake .. -G Ninja \
        -DBUILD_VST=TRUE \
        -DBUILD_VST3=FALSE \
        -DBUILD_AU=FALSE \
        -DBUILD_STANDALONE=TRUE \
        -DMCFX_BUILD_VST2_PER_CHANNEL=OFF \
        -DMCFX_BUILD_MC=ON \
        -DVST2SDKPATH="$VST2SDK"
    ninja
    popd

    echo ""; echo "codesigning Standalone apps"
    codesign_bundles "$STANDALONE_DIR" "app"

    INSTALLER=${ROOT}/_OSX_RELEASE/mcfx_v${VERSION}_macos_standalone.pkg
    build_installer "$STANDALONE_DIR" "com.kronlachner.mcfx.standalone" "/Applications/mcfx" "$INSTALLER"
fi

# =========================================================
echo ""
echo "Done! Installers:"
ls -la "${ROOT}/_OSX_RELEASE/"*.pkg 2>/dev/null || echo "(none)"
