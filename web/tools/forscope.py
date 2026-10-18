# Report MSVC6 for-scope dependencies: variables declared in a for-init that are referenced
# after the loop ends, inside the same enclosing block (including via macros).
import re, sys, glob
def strip(src):
    # blank out comments and string/char literals, preserving offsets
    out = list(src); i = 0; n = len(src)
    while i < n:
        c = src[i]
        if src.startswith('//', i):
            j = src.find('\n', i); j = n if j < 0 else j
            for k in range(i, j): out[k] = ' '
            i = j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2); j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n': out[k] = ' '
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == '\\' else 1
            for k in range(i + 1, min(j, n)): out[k] = ' '
            i = j + 1
        else:
            i += 1
    return ''.join(out)
FOR = re.compile(r'\bfor\s*\(\s*((?:const\s+)?[A-Za-z_][A-Za-z0-9_:<>]*(?:\s*\*)*)\s+(\*?\s*[A-Za-z_]\w*)\s*=([^;]*);')
def match_paren(s, i):
    d = 0
    while i < len(s):
        if s[i] == '(': d += 1
        elif s[i] == ')':
            d -= 1
            if d == 0: return i
        i += 1
    return -1
def stmt_end(s, i):
    # end of the statement starting at i (after for(...))
    while s[i].isspace(): i += 1
    if s[i] == '{':
        d = 0
        while True:
            if s[i] == '{': d += 1
            elif s[i] == '}':
                d -= 1
                if d == 0: return i + 1
            i += 1
    if re.match(r'(for|while|if|switch)\b', s[i:]):
        j = s.index('(', i); j = match_paren(s, j) + 1
        e = stmt_end(s, j)
        if s[i:i+2] == 'if':
            m = re.match(r'\s*else\b', s[e:])
            if m: e = stmt_end(s, e + m.end())
        return e
    if re.match(r'do\b', s[i:]):
        e = stmt_end(s, i + 2); m = re.match(r'\s*while\s*\(', s[e:]); j = match_paren(s, e + m.end() - 1)
        return s.index(';', j) + 1
    d = 0
    while True:
        if s[i] in '({[': d += 1
        elif s[i] in ')}]': d -= 1
        elif s[i] == ';' and d == 0: return i + 1
        i += 1
def block_end(s, i):
    d = 0
    while i < len(s):
        if s[i] == '{': d += 1
        elif s[i] == '}':
            if d == 0: return i
            d -= 1
        i += 1
    return len(s)
for f in sorted(glob.glob(sys.argv[1] + '/*.cpp') + glob.glob(sys.argv[1] + '/*.h')):
    src = open(f, encoding='latin-1').read(); s = strip(src)
    for m in FOR.finditer(s):
        name = m.group(2).replace('*', '').strip()
        p = s.index('(', m.start()); close = match_paren(s, p)
        try: end = stmt_end(s, close + 1)
        except Exception: continue
        bend = block_end(s, end)
        rest = s[end:bend]
        # stop at a redeclaration of the same name in the rest of the block
        r = re.search(r'\b' + re.escape(name) + r'\b', rest)
        if r:
            line = s.count('\n', 0, m.start()) + 1
            uline = s.count('\n', 0, end + r.start()) + 1
            ctx = rest[max(0, r.start() - 30): r.end() + 10].replace('\n', ' ')
            redecl = re.search(r'(int|long|char|float|double|\*)\s+' + re.escape(name) + r'\b', rest[max(0, r.start()-20): r.end()])
            print(f'{f}:{line}: for-var {name} used at line {uline}{" (REDECLARED)" if redecl else ""}: ...{ctx.strip()}...')
