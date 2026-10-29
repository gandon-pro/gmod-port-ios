"""Exercise the actual patched FixedVector header with a standalone ASSERT shim."""
from pathlib import Path
import shutil
import subprocess
import sys

root, out = Path(sys.argv[1]), Path(sys.argv[2]).resolve()
(out / 'common').mkdir(parents=True, exist_ok=True)
shutil.copyfile(root / 'src/common/FixedVector.h', out / 'common/FixedVector.h')
# Only debug.h is isolated; the container implementation is the pinned header.
(out / 'common/debug.h').write_text('#pragma once\n#include <cassert>\n#define ASSERT(x) assert(x)\n')
source = out / 'fixedvector_test.cpp'
source.write_text('''#include "common/FixedVector.h"
#include <cassert>
#include <cstdio>
int main() {
    angle::FixedVector<int, 0> zero;
    assert(zero.begin() == zero.end());
    const auto &czero = zero;
    assert(czero.begin() == czero.end());
    angle::FixedVector<int, 1> one;
    one.push_back(17);
    assert(std::distance(one.begin(), one.end()) == 1);
    for (size_t size = 0; size <= 8; ++size) {
        angle::FixedVector<int, 8> v;
        for (size_t i = 0; i < size; ++i) v.push_back(int(i));
        assert(std::distance(v.begin(), v.end()) == int(size));
        const auto &cv = v;
        assert(std::distance(cv.begin(), cv.end()) == int(size));
        int count = 0;
        for (int value : cv) assert(value == count++);
        assert(count == int(size));
        auto copy = v;
        assert(std::distance(copy.begin(), copy.end()) == int(size));
        v.clear();
        assert(v.begin() == v.end());
    }
    puts("FixedVector: zero/one/eight capacity, all sizes, const/mutable iteration, copy/clear PASS");
}
''')
if sys.platform == 'win32':
    exe = out / 'fixedvector_test.exe'
    subprocess.run(['cl','/nologo','/std:c++17','/EHsc',str(source),'/I'+str(out),'/Fe'+str(exe),'/Fo'+str(out/'fixedvector_test.obj')],check=True)
else:
    exe = out / 'fixedvector_test'
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wunsafe-buffer-usage',str(source),'-I'+str(out),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
