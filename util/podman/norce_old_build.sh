#!/usr/bin/env bash
set -euo pipefail

# --- Configuration: package name and version ---
PKG_NAME="kel-lbm"
PKG_VERSION="0.0.3"

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

# --- Detect host CA bundle (best effort) ---
HOST_CA_BUNDLE=""
if [[ -f /etc/ssl/certs/ca-certificates.crt ]]; then
    HOST_CA_BUNDLE="/etc/ssl/certs/ca-certificates.crt"
elif [[ -f /etc/pki/tls/certs/ca-bundle.crt ]]; then
    HOST_CA_BUNDLE="/etc/pki/tls/certs/ca-bundle.crt"
fi

# --- Run build + packaging inside container ---
podman run --rm -it \
    -v "$GIT_ROOT":/workspace \
    -v /etc/ssl/certs:/etc/ssl/certs:ro \
    -v /etc/pki:/etc/pki:ro \
    ${HOST_CA_BUNDLE:+-v "$HOST_CA_BUNDLE:$HOST_CA_BUNDLE:ro"} \
    -w /workspace \
    -e USER=nix \
    -e PKG_NAME="$PKG_NAME" \
    -e PKG_VERSION="$PKG_VERSION" \
    -e SSL_CERT_FILE="$HOST_CA_BUNDLE" \
    -e NIX_SSL_CERT_FILE="$HOST_CA_BUNDLE" \
    docker.io/nixos/nix:latest \
    nix-shell -p bash --run "
        set -euo pipefail

        echo \"Using CA bundle: \${SSL_CERT_FILE:-system default}\"

        # Build the derivation
        nix-build default.nix -A release.examples --out-link result

        # Install fpm if missing
        if ! command -v fpm >/dev/null 2>&1; then
            nix-shell -p fpm --run 'true'
        fi

        # Create .deb package
        nix-shell -p fpm --run \"fpm -s dir -t deb -n \$PKG_NAME -v \$PKG_VERSION -C result --prefix /usr/local .\"

        # RPM disabled for now
        # nix-shell -p fpm rpm --run \"fpm -s dir -t rpm -n \$PKG_NAME -v \$PKG_VERSION -C result --prefix /usr/local .\"
    "

echo "✅ Build complete!"
echo " - result -> $GIT_ROOT/result"
echo " - ${PKG_NAME}_${PKG_VERSION}.deb is in $GIT_ROOT"
