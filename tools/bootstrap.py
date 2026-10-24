import argparse
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--engine-only',action='store_true')
args=parser.parse_args()
lock=json.loads((ROOT/'dependencies.lock.json').read_text())
for name,spec in lock.items():
    if args.engine_only and name!='engine':continue
    folder=ROOT/'deps'/name
    created=not folder.exists()
    if created:subprocess.run(['git','clone','--filter=blob:none','--no-checkout',spec['url'],str(folder)],check=True)
    subprocess.run(['git','-C',str(folder),'fetch','--depth','1','origin',spec['commit']],check=True)
    current=subprocess.run(['git','-C',str(folder),'rev-parse','--verify','HEAD'],capture_output=True,text=True)
    if not created and current.returncode==0 and current.stdout.strip()!=spec['commit']:
        raise SystemExit(f'{folder}: unexpected checkout; will not reset user changes')
    subprocess.run(['git','-C',str(folder),'checkout','--detach',spec['commit']],check=True)
    if name=='engine':
        subprocess.run(['git','-C',str(folder),'submodule','update','--init','--recursive','--depth','1'],check=True)
    assert subprocess.check_output(['git','-C',str(folder),'rev-parse','HEAD'],text=True).strip()==spec['commit']
    print(name,spec['commit'],flush=True)
