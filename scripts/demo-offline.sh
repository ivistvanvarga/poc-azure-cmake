#!/usr/bin/env bash
# Runs the workload identity flow end to end without an Azure subscription.
#
# A local TLS server stands in for the Microsoft Entra token endpoint, the federated token is a
# file we generate ourselves, and the program does the real exchange against it.
#
# Usage: scripts/demo-offline.sh
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-${repo_root}/build}"
binary="${build_dir}/poc-azure-cmake"

if [[ ! -x "${binary}" ]]; then
    echo "the program is not built yet, run: cmake -B build && cmake --build build" >&2
    exit 1
fi

work_dir="$(mktemp -d)"
mock_pid=""

cleanup() {
    if [[ -n "${mock_pid}" ]]; then
        kill "${mock_pid}" 2>/dev/null || true
        wait "${mock_pid}" 2>/dev/null || true
    fi
    rm -rf "${work_dir}"
}
trap cleanup EXIT

echo "== generating a self signed certificate for localhost =="
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "${work_dir}/key.pem" \
    -out "${work_dir}/cert.pem" \
    -days 1 \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1" >/dev/null 2>&1

echo "== generating the federated token that AKS would project =="
python3 - "${work_dir}/federated-token.jwt" <<'PY'
import base64
import json
import sys
import time

now = int(time.time())
claims = {
    "aud": "api://AzureADTokenExchange",
    "iss": "https://kubernetes.default.svc.cluster.local",
    "sub": "system:serviceaccount:default:azure-workload-identity",
    "exp": now + 3600,
    "iat": now,
}


def b64(raw: bytes) -> str:
    return base64.urlsafe_b64encode(raw).decode("ascii").rstrip("=")


header = b64(json.dumps({"alg": "RS256", "typ": "JWT"}, separators=(",", ":")).encode())
payload = b64(json.dumps(claims, separators=(",", ":")).encode())
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    handle.write(f"{header}.{payload}.kubernetes-signature")
PY

port="$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')"

echo "== starting the mock token endpoint on port ${port} =="
python3 "${repo_root}/tools/mock_entra.py" \
    --cert "${work_dir}/cert.pem" \
    --key "${work_dir}/key.pem" \
    --port "${port}" &
mock_pid=$!

for _ in $(seq 1 50); do
    if python3 -c "import socket; socket.create_connection(('127.0.0.1', ${port}), 0.2).close()" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

echo "== checking the environment =="
AZURE_TENANT_ID="11111111-2222-3333-4444-555555555555" \
AZURE_CLIENT_ID="66666666-7777-8888-9999-000000000000" \
AZURE_FEDERATED_TOKEN_FILE="${work_dir}/federated-token.jwt" \
AZURE_AUTHORITY_HOST="https://localhost:${port}/" \
    "${binary}" check || true

echo
echo "== exchanging the federated token for an access token =="
AZURE_TENANT_ID="11111111-2222-3333-4444-555555555555" \
AZURE_CLIENT_ID="66666666-7777-8888-9999-000000000000" \
AZURE_FEDERATED_TOKEN_FILE="${work_dir}/federated-token.jwt" \
AZURE_AUTHORITY_HOST="https://localhost:${port}/" \
    "${binary}" token --insecure-tls --scope "https://management.azure.com/.default"
