"""Bind a fallback ZIP to clean ACA/BBB source and refuse stale/edited uploads."""
from pathlib import Path
import json
import sys
import zipfile
from release_console import ROOT, Blocked, clean_commit, digest, read_json, write_json
from bbb_dependency import source as bbb_source


def seal(root=ROOT):
    bbb, bbb_commit = bbb_source()
    commit = clean_commit(root)
    archive = root / 'dist/ACActual-WithBBB.zip'
    version = (root / 'dist/VERSION.txt').read_text().strip()
    with zipfile.ZipFile(archive) as package:
        embedded = package.read('ACActual-Arland/ArmaCommanderReforger/VERSION.txt').decode().strip()
        dependency = package.read('ACActual-Arland/BetterBattleBuddies/SOURCE-COMMIT.txt').decode().strip()
    if embedded != version or dependency != bbb_commit:
        raise Blocked('ZIP version/dependency does not match the release metadata.')
    write_json(root / 'dist/PACKAGE-BUILD.json', dict(commit=commit, bbbCommit=bbb_commit,
               version=version, sha256=digest(archive)))


def verify(root=ROOT):
    manifest = read_json(root / 'dist/PACKAGE-BUILD.json')
    if clean_commit(root) != manifest['commit']:
        raise Blocked('ZIP predates current source. Rebuild the fallback ZIP before publishing.')
    bbb, commit = bbb_source()
    if commit != manifest['bbbCommit']:
        raise Blocked('ZIP BBB dependency is stale. Rebuild the fallback ZIP.')
    if (root / 'dist/VERSION.txt').read_text().strip() != manifest['version']:
        raise Blocked('ZIP version metadata changed. Rebuild the fallback ZIP.')
    if digest(root / 'dist/ACActual-WithBBB.zip') != manifest['sha256']:
        raise Blocked('ZIP was replaced or edited after building. Rebuild the fallback ZIP.')
    print('Fallback ZIP provenance verified; this action publishes to GitHub, not Workshop.')


if __name__ == '__main__':
    try:
        if sys.argv[1:] != ['verify']:
            raise Blocked('Usage: python tools/package_guard.py verify')
        verify()
    except (Blocked, OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(f'BLOCKED: {error}', file=sys.stderr)
        sys.exit(1)
