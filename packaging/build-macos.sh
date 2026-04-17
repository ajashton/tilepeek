#!/bin/bash
set -euo pipefail

# Build a signed, notarized, Apple Silicon (arm64) macOS .dmg for TilePeek.
# Must be run on an Apple Silicon Mac; the output matches the host arch.
#
# Required environment:
#   MACOS_SIGNING_IDENTITY Full "Developer ID Application: Name (TEAMID)" identity name,
#                          matching what `security find-identity -v -p codesigning` reports.
#   MACOS_NOTARY_PROFILE   Name of a keychain profile created with `xcrun notarytool
#                          store-credentials`. Used for --keychain-profile during notarize.
#
# Optional environment:
#   QT_DIR                 Qt 6 install prefix. Defaults to `brew --prefix qt@6`.
#
# See packaging/README.md for one-time setup.
#
# Flags:
#   --skip-tests      Don't run ctest after the build.
#   --skip-notarize   Build & sign the .app and .dmg, but skip the Apple notarization
#                     round-trip. The resulting .dmg will show a Gatekeeper warning; use
#                     only for local smoke tests.
#   --keep-build      Don't wipe build/macos before configuring.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SKIP_TESTS=0
SKIP_NOTARIZE=0
KEEP_BUILD=0
for arg in "$@"; do
    case "$arg" in
        --skip-tests) SKIP_TESTS=1 ;;
        --skip-notarize) SKIP_NOTARIZE=1 ;;
        --keep-build) KEEP_BUILD=1 ;;
        -h|--help)
            sed -n '4,23p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown flag: $arg" >&2
            exit 2
            ;;
    esac
done

die() { echo "error: $*" >&2; exit 1; }
note() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }

# --- Validate prerequisites --------------------------------------------------

note "Validating prerequisites"

[ "$(uname -s)" = "Darwin" ] || die "build-macos.sh must run on macOS"
[ "$(uname -m)" = "arm64" ] || die "build-macos.sh must run on Apple Silicon (arm64). Host is $(uname -m)."

for bin in cmake codesign xcrun iconutil rsvg-convert create-dmg; do
    command -v "$bin" >/dev/null 2>&1 || die "missing required tool: $bin (see packaging/README.md)"
done

: "${MACOS_SIGNING_IDENTITY:?MACOS_SIGNING_IDENTITY is not set; see packaging/README.md}"
[ "$SKIP_NOTARIZE" -eq 1 ] || : "${MACOS_NOTARY_PROFILE:?MACOS_NOTARY_PROFILE is not set; see packaging/README.md}"

if [ -z "${QT_DIR:-}" ]; then
    if command -v brew >/dev/null 2>&1 && QT_DIR=$(brew --prefix qt@6 2>/dev/null) && [ -d "$QT_DIR" ]; then
        note "QT_DIR not set; using $QT_DIR (brew --prefix qt@6)"
    else
        die "QT_DIR is not set and Homebrew qt@6 is not installed; run 'brew install qt@6' or set QT_DIR"
    fi
fi

MACDEPLOYQT="$QT_DIR/bin/macdeployqt"
[ -x "$MACDEPLOYQT" ] || die "macdeployqt not found at $MACDEPLOYQT"

if ! security find-identity -v -p codesigning | grep -qF "$MACOS_SIGNING_IDENTITY"; then
    die "signing identity not found in keychain: $MACOS_SIGNING_IDENTITY"
fi

if [ "$SKIP_NOTARIZE" -eq 0 ]; then
    if ! xcrun notarytool history --keychain-profile "$MACOS_NOTARY_PROFILE" >/dev/null 2>&1; then
        die "notary profile '$MACOS_NOTARY_PROFILE' is not usable; run 'xcrun notarytool store-credentials' (see packaging/README.md)"
    fi
fi

# --- Extract version ---------------------------------------------------------

VERSION=$(grep -oE 'project\(tilepeek VERSION [0-9.]+' "$PROJECT_DIR/CMakeLists.txt" | awk '{print $NF}')
[ -n "$VERSION" ] || die "could not extract version from CMakeLists.txt"
note "Building TilePeek ${VERSION}"

# --- Paths -------------------------------------------------------------------

BUILD_DIR="$PROJECT_DIR/build/macos"
STAGE_DIR="$BUILD_DIR/stage"
APP="$STAGE_DIR/TilePeek.app"
DMG_STAGE="$BUILD_DIR/dmg-contents"
DMG_OUT="$BUILD_DIR/TilePeek-${VERSION}.dmg"
ENTITLEMENTS="$PROJECT_DIR/resources/macos/tilepeek.entitlements"

if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

# --- Configure + build -------------------------------------------------------

note "Configuring (arm64; deployment target 14.0)"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="14.0" \
    -DCMAKE_PREFIX_PATH="$QT_DIR"

note "Building"
cmake --build "$BUILD_DIR" --parallel

if [ "$SKIP_TESTS" -eq 0 ]; then
    note "Running tests"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

# --- Stage + deploy Qt -------------------------------------------------------

BUILT_APP="$BUILD_DIR/src/TilePeek.app"
[ -d "$BUILT_APP" ] || die "expected bundle not found at $BUILT_APP"

note "Staging bundle"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
cp -R "$BUILT_APP" "$APP"

note "Bundling Qt frameworks (macdeployqt)"
"$MACDEPLOYQT" "$APP" -always-overwrite -verbose=1

# --- Codesign (hardened runtime), inside-out --------------------------------

note "Codesigning (hardened runtime)"
SIGN_ARGS=(--force --timestamp --options runtime --sign "$MACOS_SIGNING_IDENTITY")

# Sign every dylib and Mach-O binary inside Frameworks/ and PlugIns/ first.
while IFS= read -r -d '' f; do
    codesign "${SIGN_ARGS[@]}" "$f"
done < <(find "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" \
    -type f \( -name '*.dylib' -o -name '*.so' \) -print0 2>/dev/null)

# Sign each .framework bundle.
while IFS= read -r -d '' fw; do
    codesign "${SIGN_ARGS[@]}" "$fw"
done < <(find "$APP/Contents/Frameworks" -type d -name '*.framework' -print0 2>/dev/null)

# Sign the main executable and any helpers.
while IFS= read -r -d '' bin; do
    codesign "${SIGN_ARGS[@]}" "$bin"
done < <(find "$APP/Contents/MacOS" -type f -perm +111 -print0)

# Final outer signing of the .app with entitlements for hardened runtime.
codesign --force --timestamp --options runtime \
    --entitlements "$ENTITLEMENTS" \
    --sign "$MACOS_SIGNING_IDENTITY" \
    "$APP"

note "Verifying signature"
codesign --verify --deep --strict --verbose=2 "$APP"

# --- Build DMG ---------------------------------------------------------------

note "Building DMG"
rm -rf "$DMG_STAGE"
mkdir -p "$DMG_STAGE"
cp -R "$APP" "$DMG_STAGE/TilePeek.app"

rm -f "$DMG_OUT"
create-dmg \
    --volname "TilePeek ${VERSION}" \
    --window-pos 200 120 \
    --window-size 540 380 \
    --icon-size 96 \
    --icon "TilePeek.app" 140 180 \
    --hide-extension "TilePeek.app" \
    --app-drop-link 400 180 \
    --no-internet-enable \
    "$DMG_OUT" \
    "$DMG_STAGE/"

note "Signing DMG"
codesign --force --timestamp --sign "$MACOS_SIGNING_IDENTITY" "$DMG_OUT"

# --- Notarize + staple -------------------------------------------------------

if [ "$SKIP_NOTARIZE" -eq 0 ]; then
    note "Submitting to Apple notary service (this can take several minutes)"
    NOTARIZE_LOG="$BUILD_DIR/notarize.log"
    if ! xcrun notarytool submit "$DMG_OUT" \
            --keychain-profile "$MACOS_NOTARY_PROFILE" \
            --wait 2>&1 | tee "$NOTARIZE_LOG"; then
        SUBMISSION_ID=$(grep -oE 'id: [0-9a-f-]+' "$NOTARIZE_LOG" | head -1 | awk '{print $2}' || true)
        if [ -n "${SUBMISSION_ID:-}" ]; then
            echo "--- notarization log ---" >&2
            xcrun notarytool log "$SUBMISSION_ID" --keychain-profile "$MACOS_NOTARY_PROFILE" >&2 || true
        fi
        die "notarization failed (see $NOTARIZE_LOG)"
    fi

    if grep -q 'status: Invalid' "$NOTARIZE_LOG"; then
        SUBMISSION_ID=$(grep -oE 'id: [0-9a-f-]+' "$NOTARIZE_LOG" | head -1 | awk '{print $2}')
        echo "--- notarization log ---" >&2
        xcrun notarytool log "$SUBMISSION_ID" --keychain-profile "$MACOS_NOTARY_PROFILE" >&2 || true
        die "notarization rejected"
    fi

    note "Stapling ticket to DMG"
    xcrun stapler staple "$DMG_OUT"
    xcrun stapler validate "$DMG_OUT"

    note "Gatekeeper assessment"
    spctl -a -vv -t install "$DMG_OUT" || die "spctl assessment failed"
else
    note "Skipping notarization (--skip-notarize). DMG will not pass Gatekeeper on other machines."
fi

# --- Done --------------------------------------------------------------------

SHA=$(shasum -a 256 "$DMG_OUT" | awk '{print $1}')
SIZE=$(ls -lh "$DMG_OUT" | awk '{print $5}')

echo
note "Done"
echo "  DMG:    $DMG_OUT"
echo "  Size:   $SIZE"
echo "  SHA256: $SHA"
