"""Generate exhaustive reference cases and compile the actual replacement body."""
from pathlib import Path
import itertools
import json
import subprocess
import sys

def reference(pattern, name):
    row = [True] + [False] * len(name)
    for c in pattern:
        result = [c == '*' and row[0]]
        for i, n in enumerate(name):
            result.append((row[i + 1] or result[i]) if c == '*' else row[i] and (c == '?' or c == n))
        row = result
    return row[-1]

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
strings = lambda alphabet: [''.join(v) for size in range(5) for v in itertools.product(alphabet, repeat=size)]
cases = [(p, n, reference(p, n)) for p in strings('a?*') for n in strings('ab*')]
code = '#include <string_view>\n#include <cstddef>\n#include <cstdio>\n'
code += Path(__file__).with_name('angle_wildcard_function.txt').read_text()
code += '\nint main(){ int failures=0;\n'
for p, n, result in cases:
    code += f'if(NamesMatchWithWildcard({json.dumps(p)},{json.dumps(n)})!={str(result).lower()}) ++failures;\n'
code += f'printf("Wildcard: {len(cases)} cases, %d failures\\n",failures); return failures?1:0;}}\n'
source = out / 'wildcard_test.cpp'
source.write_text(code)
if sys.platform == 'win32':
    subprocess.run(['cl','/nologo','/std:c++17','/EHsc',str(source),'/Fe'+str(out/'wildcard_test.exe'),'/Fo'+str(out/'wildcard_test.obj')],check=True)
    subprocess.run([str(out/'wildcard_test.exe')],check=True)
else:
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wunsafe-buffer-usage',str(source),'-o',str(out/'wildcard_test')],check=True)
    subprocess.run([str(out/'wildcard_test')],check=True)
