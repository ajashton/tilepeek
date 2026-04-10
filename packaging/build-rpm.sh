#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Extract version from CMakeLists.txt
VERSION=$(grep -oP 'project\(tilepeek VERSION \K[0-9.]+' "$PROJECT_DIR/CMakeLists.txt")
NAME="tilepeek"
TARBALL="${NAME}-${VERSION}"

echo "Building ${NAME}-${VERSION} RPM..."

# Set up rpmbuild directory structure
RPMBUILD_DIR="$PROJECT_DIR/build/rpmbuild"
mkdir -p "$RPMBUILD_DIR"/{SOURCES,SPECS,BUILD,RPMS,SRPMS}

# Create source tarball
# Use git archive if the working tree is clean, otherwise tar the working tree
if git -C "$PROJECT_DIR" diff --quiet HEAD 2>/dev/null && \
   git -C "$PROJECT_DIR" diff --cached --quiet HEAD 2>/dev/null && \
   [ -z "$(git -C "$PROJECT_DIR" ls-files --others --exclude-standard)" ]; then
    git -C "$PROJECT_DIR" archive --prefix="${TARBALL}/" -o "$RPMBUILD_DIR/SOURCES/${TARBALL}.tar.gz" HEAD
else
    echo "Note: working tree has uncommitted changes, creating tarball from working tree"
    tar -czf "$RPMBUILD_DIR/SOURCES/${TARBALL}.tar.gz" \
        --transform "s,^,${TARBALL}/," \
        --exclude=build --exclude=.git \
        -C "$PROJECT_DIR" .
fi

# Copy spec file
cp "$SCRIPT_DIR/tilepeek.spec" "$RPMBUILD_DIR/SPECS/"

# Build binary and source RPMs
rpmbuild --define "_topdir $RPMBUILD_DIR" -ba "$RPMBUILD_DIR/SPECS/tilepeek.spec"

echo ""
echo "RPMs:"
find "$RPMBUILD_DIR/RPMS" -name '*.rpm'
echo "SRPMs:"
find "$RPMBUILD_DIR/SRPMS" -name '*.src.rpm'
