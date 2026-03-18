/**
 * secrets/gen_header.js
 * Converts tls_cert.pem + tls_key.pem → tls_credentials.h (C++ PROGMEM header)
 * 
 * Usage:
 *   node secrets/gen_header.js
 */
const fs = require('fs');
const path = require('path');
const dir = path.dirname(__filename);

const cert = fs.readFileSync(path.join(dir, 'tls_cert.pem'), 'utf8').trim();
const key  = fs.readFileSync(path.join(dir, 'tls_key.pem'),  'utf8').trim();

const header = `#pragma once
// AUTO-GENERATED — DO NOT COMMIT
// Run: node secrets/gen_header.js   (after regenerating the PEMs)

static const char TLS_CERT[] PROGMEM = R"(${cert}
)";

static const char TLS_KEY[] PROGMEM = R"(${key}
)";
`;

fs.writeFileSync(path.join(dir, 'tls_credentials.h'), header);
console.log('✅  secrets/tls_credentials.h updated');
