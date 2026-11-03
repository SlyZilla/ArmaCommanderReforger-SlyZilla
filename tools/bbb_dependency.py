"""Resolve the pinned, clean sibling BBB checkout for reproducible player packages."""
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
BBB_ID = 'BBB0261015000001'


def source():
    root = ROOT.parent / 'BetterBattleBuddies'
    for directory in ['Scripts/Game/BBB', 'Configs/BBB']:
        if any(p.is_file() for p in (ROOT / directory).rglob('*')):
            raise SystemExit(f'BBB is a dependency; remove duplicate ACA-owned files in {directory}.')
    pin = json.loads((ROOT / 'tools/bbb-source.json').read_text(encoding='utf-8'))
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != pin['commit']:
        raise SystemExit(f'BBB checkout {commit} differs from pinned {pin["commit"]}. Review and update tools/bbb-source.json.')
    if subprocess.check_output(['git', '-C', str(root), 'status', '--porcelain'], text=True).strip():
        raise SystemExit('Commit or stash BBB changes before packaging ACA.')
    if f'GUID "{BBB_ID}"' not in (root / 'addon.gproj').read_text():
        raise SystemExit('Unexpected BBB addon identity.')
    if not (root / 'resourceDatabase.rdb').is_file():
        raise SystemExit('Open BBB addon.gproj in Workbench to build resourceDatabase.rdb first.')
    missing = missing_references(root)
    if missing:
        raise SystemExit('The fallback ZIP would leave out files BBB loads: ' +
                         ', '.join(f'{path} (from {origin})' for path, origin in missing.items()))
    return root, commit


def release_files(root):
    for name in ['addon.gproj', 'resourceDatabase.rdb', 'README.md', 'LICENSE.md']:
        yield root / name
    # Same directories as the Workshop console's BBB spec (release_console.SPECS).
    for directory in ['Scripts/Game/BBB', 'Configs/BBB', 'Prefabs/BBB', 'UI/layouts/BBB']:
        yield from (p for p in sorted((root / directory).rglob('*')) if p.is_file())


def missing_references(root, files=None):
    """Files that BBB's packaged scripts, configs and layouts load by {GUID}path but the package leaves out."""
    packaged = {p.relative_to(root).as_posix() for p in (release_files(root) if files is None else files)}
    missing = {}
    for relative in sorted(packaged):
        path = root / relative
        if path.suffix == '.rdb' or not path.is_file():
            continue
        for reference in re.findall(r'\{[0-9A-Fa-f]{16}\}([\w./-]+)', path.read_text(encoding='utf-8-sig', errors='replace')):
            # A path that is not in BBB's own checkout belongs to the base game or another addon.
            for needed in (reference, reference + '.meta'):
                if (root / needed).is_file() and needed not in packaged:
                    missing.setdefault(needed, relative)
    return missing


if __name__ == '__main__':
    root, commit = source()
    print(commit)
