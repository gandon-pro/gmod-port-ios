"""Replace the pinned ANGLE raw-pointer wildcard matcher with bounded indices."""
from pathlib import Path
import sys

def apply(root):
    file = Path(root) / 'src/common/string_utils.cpp'
    text = file.read_text()
    start = text.index('bool NamesMatchWithWildcard(')
    end = text.index('\n}\n', start) + 3
    old = text[start:end]
    replacement = Path(__file__).with_name('angle_wildcard_function.txt').read_text()
    if old == replacement:
        return
    if 'using StringIter = std::string_view::iterator;' not in old or 'nameNext = nameIter + 1;' not in old:
        raise RuntimeError('Unexpected ANGLE wildcard implementation')
    file.write_text(text[:start] + replacement + text[end:])

if __name__ == '__main__':
    apply(sys.argv[1])
