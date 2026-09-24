#!/bin/bash
# Build mcfx plugins for macOS
#
# Usage: ./build_osx.sh [--vst2] [--vst3] [--au] [--standalone] [--no-sign]
#        With no format flags, builds all formats.
#        --no-sign skips codesigning, notarization and stapling (for CI dry runs).

set -e

ROOT=$(cd "$(dirname "$0")/.."; pwd)
BUILD_DIR=$ROOT/build
VERSION=$(<"$ROOT/VERSION")

# =========================================================
# Parse arguments
# =========================================================

BUILD_VST2=false
BUILD_VST3=false
BUILD_AU=false
BUILD_STANDALONE=false
NO_SIGN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --vst2)       BUILD_VST2=true ;;
        --vst3)       BUILD_VST3=true ;;
        --au)         BUILD_AU=true ;;
        --standalone) BUILD_STANDALONE=true ;;
        --no-sign)    NO_SIGN=true ;;
        *)            echo "Unknown option: $1"; echo "Usage: $0 [--vst2] [--vst3] [--au] [--standalone] [--no-sign]"; exit 1 ;;
    esac
    shift
done

# If no format flags given, build all
if ! $BUILD_VST2 && ! $BUILD_VST3 && ! $BUILD_AU && ! $BUILD_STANDALONE; then
    BUILD_VST2=true
    BUILD_VST3=true
    BUILD_AU=true
    BUILD_STANDALONE=true
fi

# Load codesigning credentials (only required when actually signing)
if ! $NO_SIGN; then
    CODESIGN_ENV="$ROOT/scripts/codesign.env"
    if [ ! -f "$CODESIGN_ENV" ]; then
        echo "Error: $CODESIGN_ENV not found. Create it with your codesigning credentials, or pass --no-sign."
        exit 1
    fi
    source "$CODESIGN_ENV"
fi

# =========================================================
# Helper functions
# =========================================================

codesign_bundles() {
    $NO_SIGN && return 0
    local dir="$1"
    local ext="$2"
    local entitlements="$ROOT/scripts/scanner.entitlements"
    local sa_entitlements="$ROOT/scripts/standalone.entitlements"
    local sa_host_entitlements="$ROOT/scripts/standalone_host.entitlements"

    find "$dir" -type d -name "*.$ext" -print0 2>/dev/null | while IFS= read -r -d '' bundle; do
        # Sign any embedded *_plugin_scanner helper first, with the
        # library-validation-disable entitlement so it can dlopen 3rd-party
        # VST3/AU/VST2 plugins for scanning. Must happen before signing the
        # outer bundle so the bundle seal hashes the final helper signature.
        while IFS= read -r -d '' helper; do
            codesign -s "$CODESIGN_APP" \
                     --force --timestamp --options=runtime \
                     --entitlements "$entitlements" \
                     "$helper"
        done < <(find "$bundle/Contents/Helpers" -type f -name "*_plugin_scanner" -print0 2>/dev/null)

        # Outer bundle: a .app (Standalone) needs audio input, which the
        # hardened runtime blocks unless claimed. One that carries a scanner
        # also loads 3rd-party plugins in-process and needs the scanner's
        # entitlements too. For .vst3 / .component / .vst the DAW is the
        # loader, so its entitlements apply and the bundle stays without one.
        # --deep is intentionally omitted — the helper above is already signed
        # and would otherwise be re-signed without entitlements.
        if [ "$ext" = "app" ]; then
            local app_entitlements="$sa_entitlements"
            if compgen -G "$bundle/Contents/Helpers/*_plugin_scanner" > /dev/null; then
                app_entitlements="$sa_host_entitlements"
            fi
            codesign -s "$CODESIGN_APP" \
                     --force --strict --verbose --timestamp --options=runtime \
                     --entitlements "$app_entitlements" \
                     "$bundle"
        else
            codesign -s "$CODESIGN_APP" \
                     --force --strict --verbose --timestamp --options=runtime \
                     "$bundle"
        fi
    done
}

build_installer() {
    local pkg_root="$1"
    local identifier="$2"
    local install_location="$3"
    local installer_name="$4"

    # By default pkgbuild marks every bundle relocatable: if the Installer
    # finds a bundle with the same identifier elsewhere on disk (a dev build,
    # a copy on another volume) it updates that one instead of installing to
    # install_location. Always install where we say.
    local component_plist="${BUILD_DIR}/$(basename "$installer_name" .pkg)_components.plist"
    pkgbuild --analyze --root "${pkg_root}" "${component_plist}"
    local i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "${component_plist}" > /dev/null 2>&1; do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "${component_plist}" 2> /dev/null \
            || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "${component_plist}"
        i=$((i + 1))
    done

    if $NO_SIGN; then
        pkgbuild --root "${pkg_root}" --component-plist "${component_plist}" --identifier "${identifier}" --version ${VERSION} --install-location "${install_location}" "${installer_name}"
        return 0
    fi

    local unsigned="${BUILD_DIR}/$(basename "$installer_name" .pkg)_unsigned.pkg"

    pkgbuild --root "${pkg_root}" --component-plist "${component_plist}" --identifier "${identifier}" --version ${VERSION} --install-location "${install_location}" "${unsigned}"

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
            -DMCFX_STANDALONE_PLUGINS= \
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
    STANDALONE_DIR=$BUILD_DIR/standalone

    # Standalone apps shipped alongside the VST3s: the network tools and the
    # graph host are useful outside a DAW; the effects stay plug-in only.
    VST3_STANDALONE_APPS="mcfx_send;mcfx_receive;mcfx_graph"

    echo ""; echo "=== Building VST3 multichannel (+ ${VST3_STANDALONE_APPS} Standalone) v$VERSION ==="

    pushd "$BUILD_DIR"
    cmake .. -G Ninja \
        -DBUILD_VST=TRUE \
        -DBUILD_VST3=TRUE \
        -DBUILD_AU=FALSE \
        -DBUILD_STANDALONE=TRUE \
        -DMCFX_BUILD_VST2_PER_CHANNEL=OFF \
        -DMCFX_BUILD_MC=ON \
        "-DMCFX_STANDALONE_PLUGINS=${VST3_STANDALONE_APPS}" \
        -DVST2SDKPATH="$VST2SDK"
    ninja
    popd

    echo ""; echo "codesigning VST3 plugins"
    codesign_bundles "$VST3_DIR" "vst3"
    echo ""; echo "codesigning Standalone apps"
    codesign_bundles "$STANDALONE_DIR" "app"

    # One package, two destinations: stage a tree rooted at / .
    PKG_ROOT=$BUILD_DIR/pkg_vst3
    rm -rf "$PKG_ROOT"
    mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/VST3/mcfx" "$PKG_ROOT/Applications/mcfx"
    ditto "$VST3_DIR" "$PKG_ROOT/Library/Audio/Plug-Ins/VST3/mcfx"
    IFS=';' read -ra _apps <<< "$VST3_STANDALONE_APPS"
    for app in "${_apps[@]}"; do
        ditto "$STANDALONE_DIR/$app.app" "$PKG_ROOT/Applications/mcfx/$app.app"
    done

    INSTALLER=${ROOT}/_OSX_RELEASE/mcfx_v${VERSION}_macos_vst3.pkg
    build_installer "$PKG_ROOT" "com.kronlachner.mcfx.vst3" "/" "$INSTALLER"
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
        -DMCFX_STANDALONE_PLUGINS= \
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
        -DMCFX_STANDALONE_PLUGINS= \
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
