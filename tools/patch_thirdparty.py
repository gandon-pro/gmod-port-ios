"""Targeted iOS fixes to the engine-pinned dependency sources."""
from pathlib import Path
import sys

third=Path(sys.argv[1])
file=third/'curl/CMakeLists.txt'
text=file.read_text()
before='if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")'
after='if(APPLE) # GModPort: SecureTransport is also available on iOS'
if after not in text:
    assert text.count(before)==1
    file.write_text(text.replace(before,after))
print('curl: enable Apple native TLS configuration for iOS as well as Darwin')
file=third/'fontconfig/src/fcatomic.h'
text=file.read_text()
# The old Apple backend returns an integer from fc_atomic_ptr_get, rejected by
# current Clang. Clang supports the existing __sync pointer/int implementation;
# select it instead of disabling threading or weakening the reference count.
apple='#elif !defined(FC_NO_MT) && defined(__APPLE__)'
clang_apple=apple+' && !defined(__clang__)'
intel='#elif !defined(FC_NO_MT) && defined(HAVE_INTEL_ATOMIC_PRIMITIVES)'
clang_intel='#elif !defined(FC_NO_MT) && (defined(HAVE_INTEL_ATOMIC_PRIMITIVES) || defined(__clang__))'
if clang_apple not in text:
    assert text.count(apple)==1 and text.count(intel)==1
    text=text.replace(apple,clang_apple).replace(intel,clang_intel)
    file.write_text(text)
print('fontconfig: use its existing GCC/Clang atomic backend on Apple Clang')
file=third/'libpng/pngpriv.h'
text=file.read_text()
# Modern Apple SDKs expose TARGET_OS_MAC on iOS too. It does not identify
# a classic Mac compiler with fp.h. Keep the actual legacy compiler tests;
# Apple Clang must use the existing standard math.h branch.
before='defined(THINK_C) || defined(__SC__) || defined(TARGET_OS_MAC)'
after='defined(THINK_C) || defined(__SC__) /* GModPort: modern SDKs use math.h */'
if after not in text:
    assert text.count(before)==1, 'Unexpected libpng legacy math include guard'
    file.write_text(text.replace(before,after))
print('libpng: do not select classic Mac fp.h from TARGET_OS_MAC')
