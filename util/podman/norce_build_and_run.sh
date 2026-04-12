#!/usr/bin/env bash
set -euo pipefail

# --- Configuration ---
PKG_NAME="kel-lbm"
PKG_VERSION="0.0.5"

NIX_STORE_VOL="nix-store"
NIX_STATE_VOL="nix-state"

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

# --- Ensure Podman volumes exist (one-time setup) ---
ensure_volume() {
    local vol="$1"
    if ! podman volume inspect "$vol" >/dev/null 2>&1; then
        echo "📦 Creating Podman volume: $vol"
        podman volume create "$vol" >/dev/null
    fi
}

ensure_volume "$NIX_STORE_VOL"
ensure_volume "$NIX_STATE_VOL"

# --- Detect host CA bundle ---
HOST_CA_BUNDLE=""
if [[ -f /etc/ssl/certs/ca-certificates.crt ]]; then
    HOST_CA_BUNDLE="/etc/ssl/certs/ca-certificates.crt"
elif [[ -f /etc/pki/tls/certs/ca-bundle.crt ]]; then
    HOST_CA_BUNDLE="/etc/pki/tls/certs/ca-bundle.crt"
fi

TIMESTAMP=$(date +%s)       # e.g. 1763452799
HOST_LBM_BASE="$HOME/.lbm"
HOST_LBM_RUN_DIR="$HOST_LBM_BASE/$TIMESTAMP"

mkdir -p "$HOST_LBM_RUN_DIR"

echo "📁 Using host LBM directory: $HOST_LBM_RUN_DIR"

# --- Run build + packaging inside container ---
podman run --rm -it \
    -v "$GIT_ROOT":/workspace \
		-v "$HOST_LBM_RUN_DIR":/root/.lbm \
    -v "$NIX_STORE_VOL":/nix/store \
    -v "$NIX_STATE_VOL":/nix/var \
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

        echo \"🔐 Using CA bundle: \${SSL_CERT_FILE:-system default}\"

        # Optional sanity check (never fatal)
        nix-store --verify --check-contents || true

        # Build
        nix-build default.nix -A release.examples --out-link /tmp/result

			# --- Interactive binary selection ---
        BIN_DIR=/tmp/result/bin
        mapfile -t BINARIES < <(ls -1 \$BIN_DIR)
        if (( \${#BINARIES[@]} == 0 )); then
            echo 'No binaries found in result/bin'
            exit 0
        fi

        echo 'Available binaries:'
        select CHOSEN_BIN in \"\${BINARIES[@]}\" 'Quit'; do
            if [[ \"\$CHOSEN_BIN\" == 'Quit' ]]; then
                echo 'Exiting.'
                break
            elif [[ -n \"\$CHOSEN_BIN\" ]]; then
                echo \"Running \$CHOSEN_BIN...\"
                \$BIN_DIR/\$CHOSEN_BIN
                break
            else
                echo 'Invalid selection, try again.'
            fi
        done
    "

echo "✅ Build complete!"
echo "   • Persistent Nix store: $NIX_STORE_VOL"
echo "   • Persistent Nix state: $NIX_STATE_VOL"
