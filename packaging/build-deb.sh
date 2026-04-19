#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEBIAN_DIR="$SCRIPT_DIR/debian"

# Extract version from CMakeLists.txt (match build-rpm.sh)
VERSION=$(grep -oP 'project\(tilepeek VERSION \K[0-9.]+' "$PROJECT_DIR/CMakeLists.txt")
NAME="tilepeek"
SRC_DIR_NAME="${NAME}-${VERSION}"

echo "Building ${NAME} ${VERSION} .deb..."

# Pick a container runtime (podman preferred, docker as fallback)
if command -v podman >/dev/null 2>&1; then
    RUNTIME=podman
elif command -v docker >/dev/null 2>&1; then
    RUNTIME=docker
else
    echo "Error: neither podman nor docker is installed." >&2
    echo "On Fedora: sudo dnf install podman" >&2
    exit 1
fi

IMAGE="tilepeek-deb-builder:ubuntu26.04"

# Build the container image. The runtime's own layer cache means this is cheap
# on subsequent runs when the Containerfile hasn't changed.
echo "Building container image ${IMAGE}..."
"$RUNTIME" build -t "$IMAGE" -f "$DEBIAN_DIR/Containerfile" "$DEBIAN_DIR"

# Stage the source tree
DEBBUILD_DIR="$PROJECT_DIR/build/debbuild"
STAGE_DIR="$DEBBUILD_DIR/$SRC_DIR_NAME"
ORIG_TARBALL="$DEBBUILD_DIR/${NAME}_${VERSION}.orig.tar.gz"

rm -rf "$DEBBUILD_DIR"
mkdir -p "$STAGE_DIR"

# Use git archive if the working tree is clean, otherwise tar the working tree
if git -C "$PROJECT_DIR" diff --quiet HEAD 2>/dev/null && \
   git -C "$PROJECT_DIR" diff --cached --quiet HEAD 2>/dev/null && \
   [ -z "$(git -C "$PROJECT_DIR" ls-files --others --exclude-standard)" ]; then
    git -C "$PROJECT_DIR" archive --prefix="${SRC_DIR_NAME}/" -o "$ORIG_TARBALL" HEAD
else
    echo "Note: working tree has uncommitted changes, creating tarball from working tree"
    tar -czf "$ORIG_TARBALL" \
        --transform "s,^,${SRC_DIR_NAME}/," \
        --exclude=build --exclude=.git \
        -C "$PROJECT_DIR" .
fi

tar -xzf "$ORIG_TARBALL" -C "$DEBBUILD_DIR"

# Copy debian/ into the staged source tree
cp -r "$DEBIAN_DIR" "$STAGE_DIR/debian"

# Build the .deb inside the container
echo "Running dpkg-buildpackage in ${IMAGE}..."
"$RUNTIME" run --rm \
    -v "$DEBBUILD_DIR":/build:Z \
    -w "/build/$SRC_DIR_NAME" \
    "$IMAGE" \
    dpkg-buildpackage -us -uc -b

# Collect artifacts
OUT_DIR="$PROJECT_DIR/build/deb"
mkdir -p "$OUT_DIR"
find "$DEBBUILD_DIR" -maxdepth 1 -type f \
    \( -name "*.deb" -o -name "*.ddeb" -o -name "*.buildinfo" -o -name "*.changes" \) \
    -exec mv {} "$OUT_DIR/" \;

# Run lintian as a soft check (don't fail the build on warnings)
echo ""
echo "Running lintian..."
set +e
"$RUNTIME" run --rm \
    -v "$OUT_DIR":/deb:Z \
    -w /deb \
    "$IMAGE" \
    sh -c 'lintian *.changes'
LINTIAN_RC=$?
set -e
if [ "$LINTIAN_RC" -ne 0 ]; then
    echo "lintian reported issues (exit ${LINTIAN_RC}) — review above."
fi

echo ""
echo "DEBs:"
find "$OUT_DIR" -name '*.deb'
