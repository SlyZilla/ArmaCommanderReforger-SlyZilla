# Builds dist/ACActual-Arland.zip: a player-only copy of the addon plus guide and launcher.
import os, sys, zipfile, pathlib, subprocess, datetime
root = pathlib.Path(__file__).resolve().parent.parent
out = root / 'dist' / 'ACActual-Arland.zip'
out.parent.mkdir(exist_ok=True)
top = 'ACActual-Arland'
keep_dirs = ['Configs', 'Missions', 'Prefabs', 'Scripts', 'Worlds']
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
# with the latest published release). It goes to dist/ACActual-Arland-local.zip.
local = '--local' in sys.argv
args = [a for a in sys.argv[1:] if a != '--local']
version = args[0] if args else 'v' + datetime.date.today().strftime('%Y.%m.%d') + '-' + sha
source_file = root / 'tools/package/update-source.txt'
update_source = source_file.read_text().strip() if source_file.exists() else ''
if local:
    version += '-local'
    update_source = ''
    out = root / 'dist' / 'ACActual-Arland-local.zip'
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr(f'{top}/ArmaCommanderReforger/VERSION.txt', version + '\n')
    if update_source:
        z.writestr(f'{top}/ArmaCommanderReforger/update-source.txt', update_source + '\n')
    z.writestr(f'{top}/READ ME FIRST.txt', crlf(root / 'tools/package/GUIDE.txt'))
    for name in ['Play-Arland.cmd', 'Host-Arland.cmd', 'Join-Arland.cmd', 'Leave-ZeroTier.cmd']:
        z.writestr(f'{top}/{name}', crlf(root / 'tools/package' / name))
    for name in ['Host-Arland-online.cmd', 'Join-Arland-online.cmd']:
        z.writestr(f'{top}/If joining fails/{name}', crlf(root / 'tools/package/alt' / name))
    for f in keep_files:
        z.write(root / f, f'{top}/ArmaCommanderReforger/{f}')
    for d in keep_dirs:
        for p in sorted((root / d).rglob('*')):
            if p.is_file():
                z.write(p, f'{top}/ArmaCommanderReforger/{p.relative_to(root).as_posix()}')
print(out, out.stat().st_size, version)
if not local:
    (root / 'dist' / 'VERSION.txt').write_text(version + '\n')
