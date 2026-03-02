#!/usr/bin/env bash
set -euo pipefail

# --- Usage ---
# ./build.sh <url> <sha256>
if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <url> <sha256>"
    exit 1
fi

SRC_URL="$1"
SRC_HASH="$2"

NIX_STORE_VOL="nix-store"
NIX_STATE_VOL="nix-state"

# --- Ensure Podman volumes exist ---
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

# --- Run inside Nix container ---
podman run --rm -it \
    -v "$NIX_STORE_VOL":/nix/store \
    -v "$NIX_STATE_VOL":/nix/var \
    -v /etc/ssl/certs:/etc/ssl/certs:ro \
    -v /etc/pki:/etc/pki:ro \
    ${HOST_CA_BUNDLE:+-v "$HOST_CA_BUNDLE:$HOST_CA_BUNDLE:ro"} \
    -e SRC_URL="$SRC_URL" \
    -e SRC_HASH="$SRC_HASH" \
    -e SSL_CERT_FILE="$HOST_CA_BUNDLE" \
    -e NIX_SSL_CERT_FILE="$HOST_CA_BUNDLE" \
    docker.io/nixos/nix:latest \
    nix-shell -p bash nix-prefetch-url --run "
        set -euo pipefail

        echo \"⬇ Fetching into nix store...\"

        STORE_PATH=\$(nix-prefetch-url --unpack --type sha256 \"\$SRC_URL\" \"\$SRC_HASH\")

        echo \"📦 Source stored at: \$STORE_PATH\"

        if [[ ! -d \"\$STORE_PATH\" ]]; then
            echo \"❌ Expected unpacked directory in nix store\"
            exit 1
        fi

        cd \"\$STORE_PATH\"

        echo \"📂 Entered: \$(pwd)\"

        if [[ ! -f default.nix ]]; then
            echo \"❌ No default.nix found in source\"
            exit 1
        fi

        echo \"🔨 Running nix-build...\"
        nix-build default.nix --out-link result

        BIN_DIR=./result/bin
        if [[ ! -d \"\$BIN_DIR\" ]]; then
            echo \"ℹ No binaries produced.\"
            exit 0
        fi

        mapfile -t BINARIES < <(ls -1 \"\$BIN_DIR\")
        if (( \${#BINARIES[@]} == 0 )); then
            echo \"ℹ No binaries found in result/bin\"
            exit 0
        fi

        echo \"Available binaries:\"
        select CHOSEN_BIN in \"\${BINARIES[@]}\" \"Quit\"; do
            if [[ \"\$CHOSEN_BIN\" == \"Quit\" ]]; then
                echo \"Exiting.\"
                break
            elif [[ -n \"\$CHOSEN_BIN\" ]]; then
                echo \"▶ Running \$CHOSEN_BIN...\"
                \"\$BIN_DIR/\$CHOSEN_BIN\"
                break
            else
                echo \"Invalid selection, try again.\"
            fi
        done
    "

echo "✅ Done!"
echo "   • Persistent Nix store: $NIX_STORE_VOL"
echo "   • Persistent Nix state: $NIX_STATE_VOL"
