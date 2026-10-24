"""Pull files from GModPort's Documents over USB (needs Apple Devices / usbmuxd).

usage: py -3.13 tools/devlog.py [out_dir] [--ls]
Copies gmodport_log.txt (and console.log if present) into out_dir (default: devlogs).
"""
import asyncio
import inspect
from pathlib import Path
import sys

from pymobiledevice3.lockdown import create_using_usbmux
from pymobiledevice3.services.house_arrest import HouseArrestService

BUNDLE = 'com.kindmops.gmodport'
FILES = ('gmodport_log.txt', 'garrysmod/console.log')


async def call(value):
    return await value if inspect.isawaitable(value) else value


async def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    out = Path(args[0] if args else 'devlogs')
    out.mkdir(parents=True, exist_ok=True)
    lockdown = await call(create_using_usbmux())
    afc = await HouseArrestService.create(lockdown, BUNDLE, documents_only=True)
    if '--ls' in sys.argv:
        print(await call(afc.listdir('/Documents')))
    for name in FILES:
        path = '/Documents/' + name
        if not await call(afc.exists(path)):
            print('missing', path)
            continue
        data = await call(afc.get_file_contents(path))
        target = out / Path(name).name
        target.write_bytes(data)
        print(f'{path} -> {target} ({len(data)} bytes)')


if __name__ == '__main__':
    asyncio.run(main())
