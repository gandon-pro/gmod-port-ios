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
    if old != replacement:
        if 'using StringIter = std::string_view::iterator;' not in old or 'nameNext = nameIter + 1;' not in old:
            raise RuntimeError('Unexpected ANGLE wildcard implementation')
        file.write_text(text[:start] + replacement + text[end:])
    file = Path(root) / 'src/common/FixedVector.h'
    text = file.read_text()
    before = '    return mStorage.begin() + mSize;'
    after = '    return std::next(mStorage.begin(), mSize);'
    if text.count(after) != 2:
        if text.count(before) != 2:
            raise RuntimeError('Unexpected ANGLE FixedVector end iterators')
        text = text.replace('#include <initializer_list>', '#include <initializer_list>\n#include <iterator>')
        file.write_text(text.replace(before, after))

if __name__ == '__main__':
    apply(sys.argv[1])
