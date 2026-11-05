#!/usr/bin/env python3
"""Flags two Enforce Script number traps that have shipped real bugs.

1. An int on the left of a comparison compares in ints: `strength < need * 1.25` with an int
   strength truncates the float side (int 6 >= 6.75 is true). Write the float first.
   Truncating only hurts `<` and `>=` (for positive values `i <= 6.75` and `i > 6.75` give the
   same answer as `i <= 6` and `i > 6`), so those two fail and `<=`/`>` are listed with --all.
2. int / int divides in ints: `float r = dead / total` is 0 while anyone lives. Make one side
   float first (`dead / (float)total`).

The script types each operand from declarations it can see (locals, parameters, members, method
return types across the scanned tree, the m_i/m_f naming convention, literals and casts). When it
can't tell a type it stays quiet, so it misses cases rather than crying wolf.
The same file lives in each mod's tools/ (ACA, BBB, AiO, MCC); keep them identical.

  python3 tools/check_enforce_numbers.py [--all] [paths...]   (default: Scripts)
  Exit 1 when it finds something. A deliberate line can end with `// int-math ok`.
"""
import re
import sys
from pathlib import Path

ALLOW = 'int-math ok'
STRICT = {'<', '>='}
INT, FLOAT = 'int', 'float'
TOKEN = re.compile(r'''
    (?P<ws>\s+)
  | (?P<comment>//[^\n]*|/\*.*?\*/)
  | (?P<string>"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')
  | (?P<num>(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?[fF]?|0[xX][0-9a-fA-F]+)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op><<=|>>=|&&|\|\||==|!=|<=|>=|\+\+|--|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|>>|::|[-+*/%<>=!&|^~?:;,.(){}\[\]])
''', re.S | re.X)
COMPARE = {'<', '<=', '>', '>='}
# Tokens that end an operand when scanning outwards from a comparison.
STOP = {'&&', '||', ',', ';', '?', ':', '{', '}', '=', '+=', '-=', '*=', '/=', '%=', '&=', '|=',
        '^=', '<', '<=', '>', '>=', '==', '!=', 'return', 'if', 'while', 'for', 'else', 'case'}
KEYWORDS = {'if', 'while', 'for', 'foreach', 'return', 'else', 'switch', 'case', 'new', 'delete',
            'break', 'continue', 'null', 'true', 'false', 'this', 'super', 'class', 'modded',
            'static', 'const', 'ref', 'autoptr', 'private', 'protected', 'override', 'proto',
            'native', 'external', 'event', 'sealed', 'out', 'inout', 'notnull', 'typedef', 'enum',
            'reference', 'owned', 'local', 'volatile', 'thread', 'func', 'typename', 'do'}
NUMBER_TYPES = {'int': INT, 'float': FLOAT, 'bool': 'bool', 'string': 'string', 'vector': 'vector'}
# Engine calls whose return type matters often enough to know without the engine source.
KNOWN_CALLS = {
    'Count': INT, 'Find': INT, 'IndexOf': INT, 'GetRandomIndex': INT,
    'RandomInt': INT, 'RandomIntInclusive': INT, 'ClampInt': INT, 'AbsInt': INT, 'ToInt': INT,
    'GetPlayerId': INT, 'GetPlayerIdFromControlledEntity': INT, 'GetPlayerCount': INT,
    'GetAgentsCount': INT, 'GetGroupID': INT,
    'RandomFloat': FLOAT, 'RandomFloat01': FLOAT, 'RandomFloatInclusive': FLOAT, 'Sqrt': FLOAT,
    'Pow': FLOAT, 'Sin': FLOAT, 'Cos': FLOAT, 'Atan2': FLOAT, 'AbsFloat': FLOAT, 'Lerp': FLOAT,
    'InverseLerp': FLOAT, 'Distance': FLOAT, 'DistanceSq': FLOAT, 'DistanceXZ': FLOAT,
    'DistanceSqXZ': FLOAT, 'ToFloat': FLOAT, 'GetWorldTime': FLOAT, 'GetTimeSlice': FLOAT,
    'GetSurfaceY': FLOAT, 'GetHealthScaled': FLOAT, 'GetSuppressionMeasure': FLOAT,
}


class Tok:
    __slots__ = ('kind', 'text', 'line')

    def __init__(self, kind, text, line):
        self.kind, self.text, self.line = kind, text, line


def tokenize(src):
    toks, line, pos = [], 1, 0
    while pos < len(src):
        m = TOKEN.match(src, pos)
        if not m:
            pos += 1
            continue
        kind, text = m.lastgroup, m.group()
        if kind not in ('ws', 'comment'):
            toks.append(Tok(kind, text, line))
        line += text.count('\n')
        pos = m.end()
    return toks


def literal_type(text):
    if text.lower().startswith('0x'):
        return INT
    return FLOAT if re.search(r'[.eEfF]', text) else INT


def is_type_name(tok):
    return tok.kind == 'id' and tok.text not in KEYWORDS


def skip_template(toks, i):
    """i at '<' after a type name: index after the matching '>', or None."""
    depth = 0
    for j in range(i, min(i + 40, len(toks))):
        t = toks[j].text
        if t == '<':
            depth += 1
        elif t == '>':
            depth -= 1
            if depth == 0:
                return j + 1
        elif t == '>>':
            depth -= 2
            if depth <= 0:
                return j + 1
        elif t in (';', '{', '}', '(', ')', '=', '&&', '||'):
            return None
    return None


def declarations(toks):
    """Yields (index, name, type, is_method) for `type name` patterns."""
    i = 0
    while i < len(toks) - 1:
        t = toks[i]
        prev = toks[i - 1].text if i else ';'
        if is_type_name(t) and prev not in ('.', '::') and not (prev == ')' and t.text not in NUMBER_TYPES):
            j = i + 1
            if toks[j].text == '<':
                j = skip_template(toks, j)
                if j is None:
                    i += 1
                    continue
            while j < len(toks) and toks[j].text == '[':  # fixed arrays: int x[4]
                j += 1
            if j < len(toks) and toks[j].kind == 'id' and toks[j].text not in KEYWORDS:
                typ = NUMBER_TYPES.get(t.text, 'other')
                nxt = toks[j + 1].text if j + 1 < len(toks) else ''
                yield j, toks[j].text, typ, nxt == '('
                # int a, b = 2, c;
                k = j + 1
                depth = 0
                while k < len(toks) and nxt != '(':
                    tx = toks[k].text
                    if tx in '([{':
                        depth += 1
                    elif tx in ')]}':
                        if depth == 0:
                            break
                        depth -= 1
                    elif tx == ';' and depth == 0:
                        break
                    elif tx == ',' and depth == 0 and k + 1 < len(toks) and toks[k + 1].kind == 'id' \
                            and k + 2 < len(toks) and toks[k + 2].text in ('=', ',', ';'):
                        yield k + 1, toks[k + 1].text, typ, False
                    k += 1
        i += 1


class Tree:
    """Member and method types across every scanned file (name -> set of types)."""

    def __init__(self):
        self.members, self.methods = {}, {}

    def add(self, toks):
        depth = 0
        decl_at = {idx: (name, typ, meth) for idx, name, typ, meth in declarations(toks)}
        for i, t in enumerate(toks):
            if i in decl_at:
                name, typ, meth = decl_at[i]
                if meth:
                    self.methods.setdefault(name, set()).add(typ)
                elif depth <= 1:  # class level
                    self.members.setdefault(name, set()).add(typ)
            if t.text == '{':
                depth += 1
            elif t.text == '}':
                depth = max(0, depth - 1)

    @staticmethod
    def one(types):
        return next(iter(types)) if types and len(types) == 1 else None


def hungarian(name):
    m = re.match(r'(?:m|s)_([if])[A-Z0-9]', name)
    if m:
        return INT if m.group(1) == 'i' else FLOAT
    return None


class Typer:
    def __init__(self, toks, tree):
        self.toks, self.tree = toks, tree
        self.scope_at = []  # per token: dict name -> type visible there
        self._build_scopes()

    def _build_scopes(self):
        decl_at = {}
        for idx, name, typ, meth in declarations(self.toks):
            if not meth:
                decl_at[idx] = (name, typ)
        stack = [{}]
        pending = {}  # parameters waiting for the function's '{'
        paren = 0
        for i, t in enumerate(self.toks):
            if t.text == '(':
                paren += 1
            elif t.text == ')':
                paren = max(0, paren - 1)
            if i in decl_at:
                name, typ = decl_at[i]
                if paren > 0 and len(stack) <= 2:
                    pending[name] = typ  # function parameter
                else:
                    stack[-1][name] = typ
            if t.text == '{':
                stack.append(dict(pending))
                pending = {}
            elif t.text == '}':
                if len(stack) > 1:
                    stack.pop()
            elif t.text == ';' and paren == 0:
                pending = {}
            self.scope_at.append(stack[:])

    def name_type(self, i, name):
        for scope in reversed(self.scope_at[i]):
            if name in scope:
                return scope[name]
        return hungarian(name) or self.tree.one(self.tree.members.get(name))

    # --- expression typing over toks[a:b] -------------------------------------------------
    def expr(self, a, b):
        """(type, has_int_division) of toks[a:b]; type None when unknown."""
        self.intdiv = False
        try:
            self.p, self.end = a, b
            typ = self._additive()
            if self.p != self.end:
                return None, self.intdiv
            return typ, self.intdiv
        except (IndexError, ValueError):
            return None, False

    def _peek(self):
        return self.toks[self.p].text if self.p < self.end else None

    def _additive(self):
        left = self._multiplicative()
        while self._peek() in ('+', '-', '&', '|', '^', '<<', '>>'):
            op = self._peek()
            self.p += 1
            right = self._multiplicative()
            left = self._combine(left, right) if op in ('+', '-') else (INT if left == right == INT else None)
        return left

    def _multiplicative(self):
        left = self._unary()
        while self._peek() in ('*', '/', '%'):
            op = self._peek()
            self.p += 1
            right = self._unary()
            if op == '/' and left == INT and right == INT:
                self.intdiv = True
            left = self._combine(left, right) if op != '%' else (INT if left == right == INT else None)
        return left

    @staticmethod
    def _combine(a, b):
        if FLOAT in (a, b) and a in (INT, FLOAT) and b in (INT, FLOAT):
            return FLOAT
        if a == b == INT:
            return INT
        return None

    def _unary(self):
        t = self._peek()
        if t in ('-', '+', '~'):
            self.p += 1
            return self._unary()
        if t == '!':
            self.p += 1
            self._unary()
            return 'bool'
        if t == '(':
            nxt = self.toks[self.p + 1].text if self.p + 1 < self.end else None
            close = self.toks[self.p + 2].text if self.p + 2 < self.end else None
            if nxt in ('int', 'float') and close == ')':  # cast
                self.p += 3
                self._unary()
                return NUMBER_TYPES[nxt]
            self.p += 1
            typ = self._additive()
            if self._peek() != ')':
                raise ValueError
            self.p += 1
            return self._postfix(typ, None)
        return self._primary()

    def _primary(self):
        tok = self.toks[self.p]
        if tok.kind == 'num':
            self.p += 1
            return literal_type(tok.text)
        if tok.kind == 'string':
            self.p += 1
            return 'string'
        if tok.kind != 'id':
            raise ValueError
        self.p += 1
        if tok.text in ('true', 'false'):
            return 'bool'
        if tok.text in ('null', 'new'):
            raise ValueError
        if self._peek() == '(':
            return self._postfix(None, tok.text, call=True)
        if self._peek() in ('.', '['):
            return self._postfix(None, tok.text)
        return self.name_type(self.p - 1, tok.text)

    def _skip_group(self, open_, close):
        depth = 0
        while self.p < self.end:
            t = self._peek()
            if t == open_:
                depth += 1
            elif t == close:
                depth -= 1
                if depth == 0:
                    self.p += 1
                    return
            self.p += 1
        raise ValueError

    def _postfix(self, typ, name, call=False):
        """Handles a chain like a.b[2].Foo(x).m_iBar; the type comes from its last link."""
        last_kind = 'name' if name else 'value'
        last = name
        receiver = None  # type of the link before the last one, for Length()
        if call:
            self._skip_group('(', ')')
            last_kind = 'call'
        while self._peek() in ('.', '[', '('):
            t = self._peek()
            if t == '.':
                receiver = None
                if last_kind == 'name' and last == name:
                    receiver = self.name_type(self.p - 1, last)
                elif last_kind == 'name':
                    receiver = hungarian(last) or self.tree.one(self.tree.members.get(last))
                self.p += 1
                last = self.toks[self.p].text
                self.p += 1
                last_kind = 'name'
                if self._peek() == '(':
                    self._skip_group('(', ')')
                    last_kind = 'call'
            elif t == '[':
                self._skip_group('[', ']')
                last_kind = 'index'
            else:
                self._skip_group('(', ')')
                last_kind = 'call'
        if last_kind == 'call':
            if last == 'Length':  # string.Length() is int, vector.Length() float
                return {'string': INT, 'vector': FLOAT}.get(receiver)
            if last in KNOWN_CALLS:
                return KNOWN_CALLS[last]
            return self.tree.one(self.tree.methods.get(last))
        if last_kind == 'name':
            if last == name:
                return self.name_type(self.p - 1, last)
            return hungarian(last) or self.tree.one(self.tree.members.get(last))
        if last_kind == 'index':
            return None  # element types of arrays aren't tracked
        return typ


def operand_bounds(toks, op_index):
    """[a, b) of the left operand and [c, d) of the right operand around a comparison."""
    depth, a = 0, op_index
    while a > 0:
        t = toks[a - 1].text
        if t in (')', ']'):
            depth += 1
        elif t in ('(', '['):
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and (t in STOP or t == '!'):
            break
        a -= 1
    depth, d = 0, op_index + 1
    while d < len(toks):
        t = toks[d].text
        if t in ('(', '['):
            depth += 1
        elif t in (')', ']'):
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and t in STOP:
            break
        d += 1
    return a, op_index, op_index + 1, d


def is_template_bracket(toks, i):
    """True for the < of array<int>, ref map<string, int> and similar."""
    if toks[i].text != '<' or i == 0 or toks[i - 1].kind != 'id':
        return False
    end = skip_template(toks, i)
    if end is None or end >= len(toks):
        return False
    return toks[end].kind == 'id' or toks[end].text in ('>', ',', ')', '(')


def assignment_target_type(typer, toks, i):
    """Type of `x` in `x = <expr>` / `float x = <expr>` when the expression starts at i."""
    if i < 2 or toks[i - 1].text not in ('=', '+=', '-=', '*=', '/='):
        return None
    tgt = toks[i - 2]
    if tgt.kind != 'id':
        return None
    if i >= 3 and toks[i - 3].text in ('int', 'float'):
        return NUMBER_TYPES[toks[i - 3].text]
    prev = toks[i - 3].text if i >= 3 else ''
    if prev == '.':
        return hungarian(tgt.text) or typer.tree.one(typer.tree.members.get(tgt.text))
    return typer.name_type(i - 2, tgt.text)


def expression_end(toks, i):
    depth = 0
    while i < len(toks):
        t = toks[i].text
        if t in ('(', '['):
            depth += 1
        elif t in (')', ']'):
            if depth == 0:
                return i
            depth -= 1
        elif depth == 0 and t in (';', ','):
            return i
        i += 1
    return i


def function_return_types(toks, tree):
    """Per token: declared return type of the enclosing method (for `return a / b;`)."""
    out, stack, pending = [], [], None
    decl = {idx: typ for idx, name, typ, meth in declarations(toks) if meth}
    for i, t in enumerate(toks):
        if i in decl:
            pending = decl[i]
        if t.text == '{':
            stack.append(pending)
            pending = None
        elif t.text == '}':
            if stack:
                stack.pop()
        elif t.text == ';' and pending is not None and not stack:
            pending = None
        out.append(next((s for s in reversed(stack) if s), None))
    return out


def check_file(path, src, tree, show_all=False):
    toks = tokenize(src)
    lines = src.splitlines()
    typer = Typer(toks, tree)
    returns = function_return_types(toks, tree)
    found = []

    def allowed(line):
        return 0 < line <= len(lines) and ALLOW in lines[line - 1]

    for i, t in enumerate(toks):
        if t.kind != 'op' or t.text not in COMPARE:
            continue
        if t.text == '<' and is_template_bracket(toks, i):
            continue
        a, b, c, d = operand_bounds(toks, i)
        if a == b or c == d:
            continue
        left, ldiv = typer.expr(a, b)
        right, rdiv = typer.expr(c, d)
        if left == INT and right == FLOAT and not allowed(t.line):
            if t.text in STRICT or show_all:
                expr = ' '.join(x.text for x in toks[a:d])
                why = 'int-left compare (compares in ints)' if t.text in STRICT else \
                    'int-left compare (harmless for positive values)'
                found.append((t.line, why, expr))
        elif (ldiv and right == FLOAT or rdiv and left == FLOAT) and not allowed(t.line):
            expr = ' '.join(x.text for x in toks[a:d])
            found.append((t.line, 'int / int compared with a float', expr))

    # int / int assigned to a float or returned from a float method
    seen = set()
    for i in range(1, len(toks)):
        prev = toks[i - 1].text
        if prev not in ('=', '+=', '-=', '*=', '/=', 'return'):
            continue
        end = expression_end(toks, i)
        if end == i:
            continue
        typ, div = typer.expr(i, end)
        if not div or typ != INT:
            continue
        target = returns[i] if prev == 'return' else assignment_target_type(typer, toks, i)
        line = toks[i].line
        if target == FLOAT and line not in seen and not allowed(line):
            seen.add(line)
            expr = ' '.join(x.text for x in toks[i:end])
            found.append((line, 'int / int into a float (divides in ints)', expr))
    return found


def main(argv):
    show_all = '--all' in argv
    roots = [Path(p) for p in argv if p != '--all'] or [Path('Scripts')]
    files = []
    for root in roots:
        files += [root] if root.is_file() else sorted(root.rglob('*.c'))
    sources = {}
    tree = Tree()
    for f in files:
        src = f.read_text(encoding='utf-8-sig', errors='replace')
        sources[f] = src
        tree.add(tokenize(src))
    total = 0
    for f, src in sources.items():
        for line, why, expr in check_file(f, src, tree, show_all):
            print(f'{f.as_posix()}:{line}: {why}: {expr}')
            total += 'harmless' not in why
    if total:
        sys.stdout.flush()
        print(f'\n{total} finding(s). Put the float side first or cast one side to float; '
              f'a deliberate line can end with // {ALLOW}', file=sys.stderr)
        return 1
    print(f'Enforce number checks: {len(files)} files clean')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
