"""Read-only browser for Enfusion .pak archives (Reforger game data).

  python3 pakread.py index <data dir> <index.json>   build an index of every file
  python3 pakread.py find <index.json> <regex>       list matching paths
  python3 pakread.py cat <index.json> <path>         print a file
"""
import json, os, re, struct, sys, zlib

def read_tree(buf, pak):
    out = {}
    pos = 0
    def entry(prefix):
        nonlocal pos
        kind, nlen = buf[pos], buf[pos + 1]
        name = buf[pos + 2:pos + 2 + nlen].decode('utf-8', 'replace')
        pos += 2 + nlen
        path = prefix + name if not prefix else prefix + '/' + name
        if kind == 0:
            count = struct.unpack_from('<I', buf, pos)[0]; pos += 4
            for _ in range(count):
                entry(path if name else '')
        else:
            off, csize, osize, _unk, _unk2, comp, level, _ts = struct.unpack_from('<IIIIHBBI', buf, pos)
            pos += 24
            out[path] = [pak, off, csize, osize, comp]
    entry('')
    return out

def index(data_dir, index_path):
    idx = {}
    for name in sorted(os.listdir(data_dir)):
        if not name.endswith('.pak'):
            continue
        full = os.path.join(data_dir, name)
        with open(full, 'rb') as f:
            f.seek(12)
            while True:
                head = f.read(8)
                if len(head) < 8:
                    break
                cid, size = head[:4], struct.unpack('>I', head[4:])[0]
                start = f.tell()
                if cid == b'FILE':
                    idx.update(read_tree(f.read(size), full))
                    break
                f.seek(start + size)
    with open(index_path, 'w') as f:
        json.dump(idx, f)
    print(f'{len(idx)} files indexed')

def cat(index_path, path):
    idx = json.load(open(index_path))
    pak, off, csize, osize, comp = idx[path]
    with open(pak, 'rb') as f:
        f.seek(off)
        raw = f.read(csize)
    data = zlib.decompress(raw) if comp else raw
    sys.stdout.buffer.write(data)

if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'index':
        index(sys.argv[2], sys.argv[3])
    elif cmd == 'find':
        idx = json.load(open(sys.argv[2]))
        rx = re.compile(sys.argv[3], re.I)
        for p in sorted(idx):
            if rx.search(p):
                print(p)
    elif cmd == 'cat':
        cat(sys.argv[2], sys.argv[3])
