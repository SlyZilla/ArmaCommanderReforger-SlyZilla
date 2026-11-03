#!/usr/bin/env python3
"""Flags script that grows the engine's shared static-initializer budget.

The engine builds every class-level static that needs code ("static ref array<T> s_X = {};",
"= new map<...>()", "= RplId.Invalid()") and every const declared inside a function body in ONE
hidden function per script module, and that function has an instruction cap shared by every
loaded mod. Over the cap the "Game" module fails to compile with "Too many instructions per
function" on random local consts in vanilla or other mods (2 Nov 2026: ACA + ACE + the Cold War
pack + AiO). Plain literals (static const float X = 2.5; static string S = "a";) cost nothing.

Declare such statics bare and fill them in the class's ACR_InitStatics(), listed in
ACR_StaticInit (tests: ACR_TestStaticInit). Move local consts to class level (static const).
The same file lives in each mod's tools/ (ACA, BBB, AiO); only the init hint differs per mod.

  python3 tools/check_static_init.py [paths...]   (default: Scripts)
  Exit 1 when it finds something. A deliberate line can end with `// static-init ok`.
"""
import re
import sys
from pathlib import Path

ALLOW = 'static-init ok'
TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\{|\}|;|\b(?:class|enum)\b|\bconst\b|\bstatic\b', re.S)
STATIC_DECL = re.compile(r'static\s+(?:const\s+)?(?:ref\s+)?[A-Za-z_][\w<>, ]*?\s+\w+\s*=\s*(\{|new\b|[A-Za-z_][\w.]*\s*\()')


def scan(path):
    text = path.read_text(encoding='utf-8-sig', errors='replace')
    lines = text.split('\n')
    found = []
    stack = []
    saw_class = False
    for m in TOKEN.finditer(text):
        t = m.group(0)
        if t.startswith('//') or t.startswith('/*') or t.startswith('"'):
            continue
        line = text.count('\n', 0, m.start()) + 1
        if ALLOW in lines[line - 1]:
            continue
        if t == '{':
            if 'func' in stack:
                stack.append('block')
            elif saw_class:
                stack.append('class')
            else:
                stack.append('func')
            saw_class = False
        elif t == '}':
            if stack:
                stack.pop()
            saw_class = False
        elif t == ';':
            saw_class = False
        elif t in ('class', 'enum'):
            saw_class = True
        elif t == 'const':
            if 'func' in stack:
                found.append((line, 'const inside a function: make it a class-level static const'))
        elif t == 'static':
            if not stack or stack[-1] == 'class':
                end = text.find(';', m.start())
                if STATIC_DECL.match(text, m.start(), end if end > 0 else len(text)):
                    found.append((line, 'static built at script load: declare it bare, fill it in ACR_InitStatics()'))
    return found


def main(argv):
    roots = [Path(a) for a in argv] or [Path('Scripts')]
    bad = 0
    for root in roots:
        files = [root] if root.is_file() else sorted(root.rglob('*.c'))
        for f in files:
            for line, why in scan(f):
                print('%s:%d: %s' % (f.as_posix(), line, why))
                bad += 1
    if bad:
        print('%d static-initializer budget issue(s); see the docstring of tools/check_static_init.py' % bad)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
