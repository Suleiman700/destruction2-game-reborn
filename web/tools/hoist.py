# Hoist a for-init declaration to the enclosing scope (MSVC6 for-scope semantics).
# Usage: hoist.py file line   -- the for statement must start at a statement boundary.
import re, sys
f, line = sys.argv[1], int(sys.argv[2])
src = open(f, encoding='latin-1').read()
lines = src.split('\n')
L = lines[line - 1]
m = re.search(r'\bfor\s*\(\s*((?:const\s+)?[A-Za-z_][\w:]*(?:\s*\*)*)\s*(\*?)\s*([A-Za-z_]\w*)\s*=', L)
if not m: sys.exit(f'{f}:{line}: no declaring for found')
before = '\n'.join(lines[:line - 1]) + '\n' + L[:m.start()]
prev = re.sub(r'\s+', '', before)[-1:]
if prev not in ';{}' and not re.search(r'\*/\s*$', before):
    sys.exit(f'{f}:{line}: for is not at a statement boundary (prev char {prev!r}); fix manually')
typ = (m.group(1) + m.group(2)).strip()
# a for-init may declare several variables of the same type: int i=0, j=1
init_end = L.index(';', m.end())
decl_names = [m.group(3)] + re.findall(r',\s*\*?\s*([A-Za-z_]\w*)\s*=', L[m.end():init_end])
hoisted = " ".join(f"{typ} {n};" for n in decl_names)
# rebuild: for( <name> = ...
lines[line - 1] = L[:m.start()] + hoisted + ' for(' + m.group(3) + '=' + L[m.end():]
open(f, 'w', encoding='latin-1').write('\n'.join(lines))
print(f'{f}:{line}: hoisted {hoisted}')
