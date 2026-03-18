# secrets/

This directory holds TLS credentials for the HTTPS server on the M5StickC Plus2.

**All `*.pem` and `tls_credentials.h` files are gitignored — never commit them.**

## Setup (first time or key rotation)

```bash
# 1. Generate a new ECDSA P-256 self-signed cert (10 years)
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 \
  -keyout secrets/tls_key.pem -out secrets/tls_cert.pem \
  -days 3650 -nodes -subj "//CN=m5cam.local" -sha256

# 2. Convert to C++ header
node secrets/gen_header.js

# 3. Build normally
pio run
```

## iPhone trust

On first visit, iOS will show "Not Secure". To trust the cert:  
Settings → General → About → Certificate Trust Settings → enable **m5cam.local**.

## Files

| File | Committed? | Purpose |
|---|---|---|
| `tls_credentials.h` | ❌ gitignored | Included by firmware at compile time |
| `tls_cert.pem` | ❌ gitignored | Raw certificate (source for header) |
| `tls_key.pem` | ❌ gitignored | Raw private key (source for header) |
| `tls_credentials.h.example` | ✅ | Template / documentation |
| `gen_header.js` | ✅ | Script to regenerate the header |
| `README.md` | ✅ | This file |
