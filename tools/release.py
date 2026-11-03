#!/usr/bin/env python3
"""
Publish a GitLab release of moonsamp from a tag that is already pushed.

    python3 tools/release.py v0.4.9 "moonsamp 0.4.9" notes.md

Runs tools/package.sh <tag>, uploads dist/moonsamp.zip to the project's
generic package registry as release-assets/<tag>/moonsamp.zip, and creates
the release with an "<name> - drop-in install" link to it: the same layout
every akacross mod release uses. It then downloads the zip back through the
release link and checks it is byte-for-byte the one it built.

The token comes from glab's keyring (`glab auth login`) and is never printed.
An existing release is left alone, so a failed run can simply be repeated.
"""
import argparse, hashlib, json, os, subprocess, sys, urllib.error, urllib.parse, urllib.request

PROJECT = 'akacross/moonsamp'
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

p = argparse.ArgumentParser(description='Create a GitLab release from a pushed tag.')
p.add_argument('tag', help='pushed tag, e.g. v0.4.9')
p.add_argument('name', help='release title, e.g. "moonsamp 0.4.9"')
p.add_argument('notes', help='Markdown file with the release description')
a = p.parse_args()


def git(*args):
    return subprocess.run(['git', '-C', ROOT, *args], capture_output=True, text=True,
                          check=True).stdout.strip()


cred = subprocess.run(['glab', 'auth', 'git-credential', 'get'],
                      input='protocol=https\nhost=gitlab.com\n\n', text=True,
                      capture_output=True, check=True).stdout
token = next(l.split('=', 1)[1] for l in cred.splitlines() if l.startswith('password='))


def api(method, url, data=None, ctype=None):
    req = urllib.request.Request(url, data=data, method=method,
                                 headers={'Authorization': 'Bearer ' + token})
    if ctype:
        req.add_header('Content-Type', ctype)
    try:
        with urllib.request.urlopen(req) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


proj = urllib.parse.quote(PROJECT, safe='')
etag = urllib.parse.quote(a.tag, safe='')
base = f'https://gitlab.com/api/v4/projects/{proj}'

if api('GET', f'{base}/releases/{etag}')[0] == 200:
    print(f'release {a.tag} already exists, nothing to do')
    sys.exit(0)

# The release must describe what was pushed, not a local-only or moved tag.
remote = git('ls-remote', 'origin', f'refs/tags/{a.tag}^{{}}') or \
         git('ls-remote', 'origin', f'refs/tags/{a.tag}')
if not remote:
    sys.exit(f'tag {a.tag} is not on origin; push it first')
if remote.split()[0] != git('rev-parse', f'{a.tag}^{{commit}}'):
    sys.exit(f'tag {a.tag} on origin points somewhere else than the local tag')

subprocess.run(['bash', os.path.join(ROOT, 'tools', 'package.sh'), a.tag], check=True)
zpath = os.path.join(ROOT, 'dist', 'moonsamp.zip')
zbytes = open(zpath, 'rb').read()
zsha = hashlib.sha256(zbytes).hexdigest()

pkg = f'{base}/packages/generic/release-assets/{etag}/moonsamp.zip'
status, body = api('PUT', pkg, zbytes)
if status not in (200, 201):
    sys.exit(f'upload failed HTTP {status} {body[:200]!r}')

notes = open(a.notes).read().strip()
payload = json.dumps({'tag_name': a.tag, 'name': a.name, 'description': notes,
    'assets': {'links': [{'name': f'{a.name} - drop-in install', 'url': pkg,
                          'direct_asset_path': '/moonsamp.zip', 'link_type': 'package'}]}})
status, body = api('POST', f'{base}/releases', payload.encode(), 'application/json')
if status != 201:
    sys.exit(f'release failed HTTP {status} {body[:300]!r}')
page = json.loads(body)['_links']['self']

link = f'https://gitlab.com/{PROJECT}/-/releases/{etag}/downloads/moonsamp.zip'
with urllib.request.urlopen(link) as r:
    served = r.read()
if hashlib.sha256(served).hexdigest() != zsha:
    sys.exit(f'created {page}, but {link} does not serve the uploaded zip')

print(f'{a.name} released: {page}')
print(f'  download {link}')
print(f'  moonsamp.zip sha256 {zsha} ({len(zbytes)} bytes)')
