#!/usr/bin/env python3
"""A stand-in for the Microsoft Entra token endpoint.

The Azure SDK for C++ exchanges a federated token for an access token with a POST to

    POST https://<authority>/<tenant>/oauth2/v2.0/token
    grant_type=client_credentials
    client_assertion_type=urn:ietf:params:oauth:client-assertion-type:jwt-bearer
    client_id=<client id>
    scope=<scope>
    client_assertion=<the content of the federated token file>

This server implements just that, over TLS with a self signed certificate, so that the whole
workload identity flow can be exercised without an Azure subscription.

The authority host has to be an https URL, the SDK rejects anything else, and the certificate
is not trusted by default, so the demo either passes --insecure-tls or a CA file.

Usage:
    tools/mock_entra.py --cert cert.pem --key key.pem [--host 127.0.0.1] [--port 8443]
"""

import argparse
import base64
import json
import ssl
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs


def base64url(raw: bytes) -> str:
    return base64.urlsafe_b64encode(raw).decode("ascii").rstrip("=")


def make_jwt(claims: dict) -> str:
    header = base64url(json.dumps({"alg": "RS256", "typ": "JWT"}, separators=(",", ":")).encode())
    payload = base64url(json.dumps(claims, separators=(",", ":")).encode())
    return f"{header}.{payload}.mock-signature"


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):  # noqa: A003 - the base class defines the name
        sys.stderr.write("mock-entra: " + (fmt % args) + "\n")

    def do_POST(self):  # noqa: N802 - the name comes from BaseHTTPRequestHandler
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8")
        fields = {key: values[0] for key, values in parse_qs(body).items()}

        self.log_message("request %s", self.path)
        for key in sorted(fields):
            value = fields[key]
            if key == "client_assertion":
                # The assertion is a credential, only its shape is interesting here.
                value = f"<{len(value)} characters>"
            self.log_message("  %s = %s", key, value)

        expected_path = "/{tenant}/oauth2/v2.0/token"
        if not self.path.endswith("/oauth2/v2.0/token"):
            self.respond(404, {"error": "not_found", "error_description": self.path})
            return

        if fields.get("grant_type") != "client_credentials":
            self.respond(400, {"error": "unsupported_grant_type"})
            return

        if not fields.get("client_id"):
            self.respond(400, {"error": "invalid_client", "error_description": "client_id missing"})
            return

        if not fields.get("client_assertion"):
            self.respond(
                401,
                {"error": "invalid_client", "error_description": "client_assertion missing"},
            )
            return

        now = int(time.time())
        scope = fields.get("scope", "")
        tenant = self.path.strip("/").split("/")[0]
        token = make_jwt(
            {
                "aud": scope or "https://management.azure.com/",
                "iss": f"https://login.microsoftonline.com/{tenant}/v2.0",
                "tid": tenant,
                "appid": fields["client_id"],
                "azp": fields["client_id"],
                "exp": now + 3600,
                "iat": now,
            }
        )

        self.respond(
            200,
            {
                "token_type": "Bearer",
                "expires_in": 3600,
                "ext_expires_in": 3600,
                "access_token": token,
            },
        )

    def respond(self, status: int, payload: dict):
        raw = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cert", required=True, help="PEM certificate")
    parser.add_argument("--key", required=True, help="PEM private key")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8443)
    args = parser.parse_args()

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile=args.cert, keyfile=args.key)

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    server.socket = context.wrap_socket(server.socket, server_side=True)

    print(f"mock-entra listening on https://{args.host}:{args.port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
