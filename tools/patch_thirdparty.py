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
