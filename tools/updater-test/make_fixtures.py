# Builds the updater fixtures (R02) in a scratch folder, from a real package zip:
#   python3 make_fixtures.py <ACActual-Arland.zip> <ACR-Update.ps1> <out dir>
# then: pwsh -File <out dir>/test-updater.ps1  (copy test-updater.ps1 there first).
# Never point it at a real install.
import zipfile, shutil, os, sys
base, updater, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
src = zipfile.ZipFile(base)
names = src.namelist()
newups = open(updater, 'rb').read()

def build(name, top='ACActual-Arland', version='v-new', drop=None, extra_root=None, modid=True, garbage=False):
    path = os.path.join(out, name)
    if garbage:
        open(path, 'wb').write(b'PK\x03\x04garbage' * 50)
        return
    with zipfile.ZipFile(path, 'w') as z:
        for n in names:
            if n.endswith('/'):
                continue
            data = src.read(n)
            rel = n.split('/', 1)[1]
            if drop and rel.startswith(drop):
                continue
            if rel.endswith('ArmaCommanderReforger/VERSION.txt'):
                data = (version + '\n').encode()
            if rel.endswith('ArmaCommanderReforger/ACR-Update.ps1'):
                data = newups
            if rel.endswith('addon.gproj') and not modid:
                data = data.replace(b'AC230926A0010001', b'FFFFFFFFFFFFFFFF')
            z.writestr(top + '/' + rel, data)
        if extra_root:
            z.writestr(extra_root + '/x.txt', 'x')

build('good.zip')
build('wrongroot.zip', top='SomethingElse')
build('tworoots.zip', extra_root='Another')
build('noprefabs.zip', drop='ArmaCommanderReforger/Prefabs/')
build('corrupt.zip', garbage=True)
build('badversion.zip', version='v-other')
build('othermod.zip', modid=False)

# The "installed" old version, with local state that must survive and an obsolete prefab.
inst_root = os.path.join(out, 'installed')
if os.path.exists(inst_root):
    shutil.rmtree(inst_root)
src.extractall(inst_root)
inst = os.path.join(inst_root, 'ACActual-Arland', 'ArmaCommanderReforger')
open(os.path.join(inst, 'VERSION.txt'), 'w').write('v-old\n')
open(os.path.join(inst, 'ACR-Update.ps1'), 'wb').write(newups)
os.makedirs(os.path.join(inst, 'Prefabs/ACR/Obsolete'), exist_ok=True)
open(os.path.join(inst, 'Prefabs/ACR/Obsolete/Gone.et'), 'w').write('old')
for f, c in [('host-settings.txt', 'H'), ('play-settings.txt', 'P'), ('zerotier.txt', 'Use=yes'), ('last-host.txt', '1.2.3.4'), ('launcher-log.txt', 'log')]:
    open(os.path.join(inst, f), 'w').write(c)
os.makedirs(os.path.join(inst, '.local-profile'), exist_ok=True)
open(os.path.join(inst, '.local-profile/p.txt'), 'w').write('prof')
print('fixtures in', out)
