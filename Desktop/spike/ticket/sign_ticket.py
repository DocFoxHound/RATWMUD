#!/usr/bin/env python3
"""Phase 0 spike (doc 77, section 5): central's side of a ticket.

Makes an Ed25519 key, writes the public set as central's `/keys` would serve it (a JWK set), and writes a list of
test tickets, each with the answer a world must give: a good one, and ones a world must refuse (wrong aud, expired,
bad signature, other algorithms, unknown key, replay, issued before the world started, duplicate fields...).

A ticket is a JWT (RFC 7519) with the fully specified `Ed25519` algorithm of RFC 9864 (not the older, polymorphic
`EdDSA`). `kid` is in the header, where JWT puts it.

    python3 sign_ticket.py OUT_DIR WORLD_ID WORLD_START_UNIX
writes OUT_DIR/keys.json and OUT_DIR/cases.txt (lines: `label expect ticket`, expect = accept | refuse).
Spike code only; uses the `cryptography` package (46.x here, system Python).
"""
import base64
import hashlib
import hmac
import json
import os
import secrets
import sys
import time

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey


def b64u(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def compact(obj) -> bytes:
    return json.dumps(obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def sign(key: Ed25519PrivateKey, header: dict, payload: dict, raw_payload: bytes | None = None) -> str:
    signing_input = b64u(compact(header)) + "." + b64u(raw_payload if raw_payload is not None else compact(payload))
    return signing_input + "." + b64u(key.sign(signing_input.encode("ascii")))


def main() -> int:
    out_dir, world, start = sys.argv[1], sys.argv[2], int(sys.argv[3])
    os.makedirs(out_dir, exist_ok=True)
    key = Ed25519PrivateKey.generate()
    other = Ed25519PrivateKey.generate()
    pub = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    kid = "2026-11-a"
    with open(os.path.join(out_dir, "keys.json"), "w") as f:
        json.dump({"keys": [{"kty": "OKP", "crv": "Ed25519", "alg": "Ed25519", "use": "sig", "kid": kid,
                             "x": b64u(pub)}]}, f)

    now = int(time.time())
    header = {"alg": "Ed25519", "typ": "JWT", "kid": kid}

    def payload(**over):
        p = {
            "sub": "acct-" + secrets.token_hex(16),
            "wolf": "wolf-" + secrets.token_hex(16),
            "name": "Ash Tallow",
            "sex": "female",
            "look": {"coat": "grey", "eyes": "amber", "markings": ["blaze"]},
            "gift": "quickened",
            "build": {"strength": "B", "speed": "A", "nose": "C"},
            "aud": world,
            "kind": "community",
            "jti": secrets.token_hex(16),
            "iat": now,
            "exp": now + 60,
        }
        p.update(over)
        return p

    cases = []
    good = sign(key, header, payload())
    cases.append(("good", "accept", good))
    cases.append(("replayed_jti", "refuse", good))  # The same ticket a second time.
    cases.append(("wrong_aud", "refuse", sign(key, header, payload(aud="world-someone-else"))))
    cases.append(("expired", "refuse", sign(key, header, payload(iat=now - 400, exp=now - 300))))
    cases.append(("expired_in_leeway", "accept", sign(key, header, payload(iat=now - 150, exp=now - 90))))
    cases.append(("clock_90s_ahead", "accept", sign(key, header, payload(iat=now + 90, exp=now + 150))))
    cases.append(("issued_before_world_start", "refuse", sign(key, header, payload(iat=start - 600, exp=now + 60))))
    # A bad signature: flip one bit of the signature.
    head, body, sig = sign(key, header, payload()).split(".")
    raw = bytearray(base64.urlsafe_b64decode(sig + "=="))
    raw[10] ^= 0x01
    cases.append(("bad_signature", "refuse", f"{head}.{body}.{b64u(bytes(raw))}"))
    # A changed payload under the old signature (aud swapped back and forth would be too easy: change the name).
    h2, b2, s2 = sign(key, header, payload()).split(".")
    tampered = json.loads(base64.urlsafe_b64decode(b2 + "=="))
    tampered["name"] = "Someone Else"
    cases.append(("tampered_payload", "refuse", f"{h2}.{b64u(compact(tampered))}.{s2}"))
    cases.append(("signed_by_other_key", "refuse", sign(other, header, payload())))
    cases.append(("unknown_kid", "refuse", sign(key, {"alg": "Ed25519", "typ": "JWT", "kid": "nope"}, payload())))
    cases.append(("missing_kid", "refuse", sign(key, {"alg": "Ed25519", "typ": "JWT"}, payload())))
    # Other algorithms, refused before anything else is read.
    unsigned = b64u(compact({"alg": "none", "typ": "JWT", "kid": kid})) + "." + b64u(compact(payload())) + "."
    cases.append(("alg_none", "refuse", unsigned))
    cases.append(("alg_EdDSA", "refuse", sign(key, {"alg": "EdDSA", "typ": "JWT", "kid": kid}, payload())))
    # The classic confusion: HS256 with the public key as the HMAC secret.
    hs_input = b64u(compact({"alg": "HS256", "typ": "JWT", "kid": kid})) + "." + b64u(compact(payload()))
    hs_sig = hmac.new(pub, hs_input.encode("ascii"), hashlib.sha256).digest()
    cases.append(("alg_HS256_pubkey_secret", "refuse", hs_input + "." + b64u(hs_sig)))
    cases.append(("alg_lowercase", "refuse", sign(key, {"alg": "ed25519", "typ": "JWT", "kid": kid}, payload())))
    cases.append(("dup_alg_in_header", "refuse",
                  b64u(b'{"alg":"Ed25519","alg":"none","typ":"JWT","kid":"' + kid.encode() + b'"}') + "." +
                  b64u(compact(payload())) + ".AAAA"))
    # A duplicate `aud` in a correctly signed payload: a lenient parser might read either one.
    dup = compact(payload())[:-1] + b',"aud":"world-someone-else"}'
    cases.append(("dup_aud_in_payload", "refuse", sign(key, header, {}, raw_payload=dup)))
    cases.append(("padding_chars", "refuse", good.replace(".", "=.", 1)))
    cases.append(("two_parts", "refuse", ".".join(good.split(".")[:2])))
    cases.append(("four_parts", "refuse", good + ".AAAA"))
    cases.append(("missing_wolf", "refuse", sign(key, header, {k: v for k, v in payload().items() if k != "wolf"})))
    cases.append(("exp_as_string", "refuse", sign(key, header, payload(exp=str(now + 60)))))
    cases.append(("bad_wolf_id", "refuse", sign(key, header, payload(wolf="wolf-XYZ"))))
    cases.append(("good_official", "accept", sign(key, header, payload(kind="official"))))

    with open(os.path.join(out_dir, "cases.txt"), "w") as f:
        for label, expect, ticket in cases:
            f.write(f"{label} {expect} {ticket}\n")
    # A batch of good tickets for the timing run (each with its own jti).
    with open(os.path.join(out_dir, "bulk.txt"), "w") as f:
        for _ in range(5000):
            f.write(sign(key, header, payload()) + "\n")
    print(f"wrote {len(cases)} cases, 5000 bulk tickets; good ticket is {len(good)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
