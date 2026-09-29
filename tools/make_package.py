# Builds dist/ArmaCommander-Arland.zip: a player-only copy of the addon plus guide and launcher.
import os, zipfile, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
out = root / 'dist' / 'ArmaCommander-Arland.zip'
out.parent.mkdir(exist_ok=True)
top = 'ArmaCommander-Arland'
keep_dirs = ['Configs', 'Missions', 'Scripts', 'Worlds']
keep_files = ['addon.gproj', 'resourceDatabase.rdb', 'Start-Test-Range.ps1', 'Play-Test-Range.cmd', 'README.md']
def crlf(p): return p.read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr(f'{top}/READ ME FIRST.txt', crlf(root / 'tools/package/GUIDE.txt'))
    z.writestr(f'{top}/Play-Arland.cmd', crlf(root / 'tools/package/Play-Arland.cmd'))
    for f in keep_files:
        z.write(root / f, f'{top}/ArmaCommanderReforger/{f}')
    for d in keep_dirs:
        for p in sorted((root / d).rglob('*')):
            if p.is_file():
                z.write(p, f'{top}/ArmaCommanderReforger/{p.relative_to(root).as_posix()}')
print(out, out.stat().st_size)
