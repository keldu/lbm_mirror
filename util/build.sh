#!/usr/bin/env bash
set -euo pipefail

# --- Configuration: package name and version ---
PKG_NAME="kel-lbm"
PKG_VERSION="0.0.2"

# --- Ensure we're in the Git root ---
if ! GIT_ROOT=$(git rev-parse --show-toplevel 2>/dev/null); then
    echo "Error: Not inside a git repository."
    exit 1
fi
cd "$GIT_ROOT"

# --- Ensure default.nix exists ---
if [[ ! -f default.nix ]]; then
    echo "Error: no default.nix in Git root ($GIT_ROOT)"
    exit 1
fi

# --- Run build + packaging inside container ---
podman run --rm -it \
    -v "$GIT_ROOT":/workspace \
    -w /workspace \
    -e USER=nix \
    -e PKG_NAME="$PKG_NAME" \
    -e PKG_VERSION="$PKG_VERSION" \
    docker.io/nixos/nix:latest \
    nix-shell -p bash --run "
        set -euo pipefail

        # Build the derivation
        nix-build default.nix --out-link result

        # Install fpm if missing
        if ! command -v fpm >/dev/null 2>&1; then
            nix-shell -p rubyPackages.fpm --run 'true'
        fi

        # Create .deb package
        nix-shell -p rubyPackages.fpm --run \"fpm -s dir -t deb -n \$PKG_NAME -v \$PKG_VERSION -C result --prefix /usr/local .\"

        # Create .rpm package
        nix-shell -p rubyPackages.fpm --run \"fpm -s dir -t rpm -n \$PKG_NAME -v \$PKG_VERSION -C result --prefix /usr/local .\"
    "

echo "✅ Build complete!"
echo " - result -> $GIT_ROOT/result"
echo " - ${PKG_NAME}_${PKG_VERSION}.deb and ${PKG_NAME}-${PKG_VERSION}.rpm are in $GIT_ROOT"
