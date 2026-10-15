# Builds dist/ACActual-WithBBB.zip: a player-only copy of the addon plus guide and launcher.
import os, sys, zipfile, pathlib, subprocess, datetime
from bbb_dependency import source as bbb_source, release_files as bbb_files
from release_console import clean_commit
from package_guard import seal
bbb_root, bbb_commit = bbb_source()
root = pathlib.Path(__file__).resolve().parent.parent
clean_commit(root)
out = root / 'dist' / 'ACActual-WithBBB.zip'
out.parent.mkdir(exist_ok=True)
top = 'ACActual-Arland'
keep_dirs = ['Configs', 'Missions', 'Prefabs', 'Scripts', 'Worlds']
# Dev-only test worlds stay out of player builds (Jacob, 11 Oct).
skip_files = {'Worlds/BBB_TestArland.ent', 'Worlds/BBB_TestArland.ent.meta', 'Missions/BBB_TestArland.conf', 'Missions/BBB_TestArland.conf.meta'}
# Test scripts stay out too (R17, Jacob 14 Oct); game code reaches them only through
# ACR_TestHooks, which does nothing without them. The test range world and entity stay.
def is_test_script(rel):
    if rel.startswith('Scripts/Game/ACR_Tests/'): return True
    return rel.startswith('Scripts/Game/BBB/') and rel.endswith('Test.c')
keep_files = ['LICENSE.md', 'addon.gproj', 'resourceDatabase.rdb', 'Start-Test-Range.ps1', 'ACR-Multiplayer.ps1', 'ACR-Update.ps1', 'ACR-Find.ps1', 'ACR-ZeroTier.ps1', 'ACR-HostSetup.ps1', 'Play-Test-Range.cmd', 'README.md', 'MISSION-MAKING.md']
# The mission-maker guide must list every World Editor setting and host option.
if subprocess.run([sys.executable, str(root / 'tools/check_mission_doc.py')]).returncode != 0:
    sys.exit('Update MISSION-MAKING.md first.')
def crlf(p): return p.read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
# Version: first argument, else date + commit.
try:
    sha = subprocess.run(['git', '-C', str(root), 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True).stdout.strip()
except Exception:
    sha = 'local'
# --local: a test build for Jacob that must not self-update (the updater would replace it
# with the latest published release). It goes to dist/ACActual-WithBBB-local.zip.
local = '--local' in sys.argv
args = [a for a in sys.argv[1:] if a != '--local']
version = args[0] if args else 'v' + datetime.date.today().strftime('%Y.%m.%d') + '-' + sha
source_file = root / 'tools/package/update-source.txt'
update_source = source_file.read_text().strip() if source_file.exists() else ''
if local:
    version += '-local'
    update_source = ''
    out = root / 'dist' / 'ACActual-WithBBB-local.zip'
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for p in bbb_files(bbb_root):
        z.write(p, f'{top}/BetterBattleBuddies/{p.relative_to(bbb_root).as_posix()}')
    z.writestr(f'{top}/BetterBattleBuddies/SOURCE-COMMIT.txt', bbb_commit + '\n')
    z.writestr(f'{top}/ArmaCommanderReforger/VERSION.txt', version + '\n')
    if update_source:
        z.writestr(f'{top}/ArmaCommanderReforger/update-source.txt', update_source + '\n')
    z.writestr(f'{top}/READ ME FIRST.txt', crlf(root / 'tools/package/GUIDE.txt'))
    for name in ['Play-ACA.cmd', 'Host-ACA.cmd', 'Join-ACA.cmd', 'Leave-ZeroTier.cmd']:
        z.writestr(f'{top}/{name}', crlf(root / 'tools/package' / name))
    for name in ['Host-ACA-online.cmd', 'Join-ACA-online.cmd']:
        z.writestr(f'{top}/If joining fails/{name}', crlf(root / 'tools/package/alt' / name))
    for f in keep_files:
        z.write(root / f, f'{top}/ArmaCommanderReforger/{f}')
    for d in keep_dirs:
        for p in sorted((root / d).rglob('*')):
            rel = p.relative_to(root).as_posix()
            if p.is_file() and rel not in skip_files and not is_test_script(rel):
                z.write(p, f'{top}/ArmaCommanderReforger/{p.relative_to(root).as_posix()}')
print(out, out.stat().st_size, version)
if not local:
    (root / 'dist' / 'VERSION.txt').write_text(version + '\n')
    seal(root)
