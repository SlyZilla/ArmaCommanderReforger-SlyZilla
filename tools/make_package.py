# Builds dist/ArmaCommander-Arland.zip: a player-only copy of the addon plus guide and launcher.
import os, sys, zipfile, pathlib, subprocess, datetime
root = pathlib.Path(__file__).resolve().parent.parent
out = root / 'dist' / 'ArmaCommander-Arland.zip'
out.parent.mkdir(exist_ok=True)
top = 'ArmaCommander-Arland'
keep_dirs = ['Configs', 'Missions', 'Scripts', 'Worlds']
keep_files = ['LICENSE.md', 'addon.gproj', 'resourceDatabase.rdb', 'Start-Test-Range.ps1', 'ACR-Multiplayer.ps1', 'ACR-Update.ps1', 'ACR-Find.ps1', 'Play-Test-Range.cmd', 'README.md']
def crlf(p): return p.read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
# Version: first argument, else date + commit.
try:
    sha = subprocess.run(['git', '-C', str(root), 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True).stdout.strip()
except Exception:
    sha = 'local'
version = sys.argv[1] if len(sys.argv) > 1 else 'v' + datetime.date.today().strftime('%Y.%m.%d') + '-' + sha
source_file = root / 'tools/package/update-source.txt'
update_source = source_file.read_text().strip() if source_file.exists() else ''
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr(f'{top}/ArmaCommanderReforger/VERSION.txt', version + '\n')
    if update_source:
        z.writestr(f'{top}/ArmaCommanderReforger/update-source.txt', update_source + '\n')
    z.writestr(f'{top}/READ ME FIRST.txt', crlf(root / 'tools/package/GUIDE.txt'))
    for name in ['Play-Arland.cmd', 'Host-Arland.cmd', 'Join-Arland.cmd']:
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
(root / 'dist' / 'VERSION.txt').write_text(version + '\n')
