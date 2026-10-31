#!/usr/bin/env python3
"""Generate public firmware defaults; private signing keys are never build inputs."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[2]

def generate(output, base_url='', public_key=None):
    base_url = base_url.rstrip('/')
    if base_url:
        url = urlsplit(base_url)
        if (url.scheme != 'https' or not url.hostname or url.username or url.password
                or url.port or url.query or url.fragment or any(c in base_url for c in '\\\r\n\t ')
                or len(base_url) > 210 or '..' in url.path.split('/')):
            raise ValueError('public base URL must be a plain HTTPS site/project URL')
    key = Path(public_key or ROOT / json.loads((ROOT/'distribution/config.json').read_text())['release_public_key']).read_text()
    if not key.startswith('-----BEGIN PUBLIC KEY-----') or len(key) > 511:
        raise ValueError('expected RSA-2048 public PEM; never supply a private key')
    checked = subprocess.run(['openssl', 'rsa', '-pubin', '-text', '-noout'],
                             input=key.encode(), capture_output=True, check=True)
    if b'2048 bit' not in checked.stdout:
        raise ValueError('release public key must be RSA-2048')
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    (output / 'distribution_defaults.h').write_text(
        '#pragma once\n#define PS_DEFAULT_STORE_URL ' + json.dumps(base_url + '/apps/index.json' if base_url else '') +
        '\n#define PS_DEFAULT_UPDATE_URL ' + json.dumps(base_url + '/updates/latest.json' if base_url else '') + '\n')
    (output / 'passport_trusted_key_generated.h').write_text(
        '#pragma once\nstatic const char ps_trusted_key[] = ' + json.dumps(key) + ';\n')

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', required=True)
    p.add_argument('--base-url', default=os.environ.get('PASSPORT_DISTRIBUTION_URL', json.loads((ROOT/'distribution/config.json').read_text())['base_url']))
    p.add_argument('--public-key', default=os.environ.get('PASSPORT_RELEASE_PUBLIC_KEY') or None)
    a = p.parse_args()
    generate(a.output, a.base_url, a.public_key)
