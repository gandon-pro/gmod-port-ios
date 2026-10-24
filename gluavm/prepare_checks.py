import argparse
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('--garrysmod',required=True,type=Path)
parser.add_argument('--out',required=True,type=Path)
args=parser.parse_args()
args.out.mkdir(parents=True,exist_ok=True)
root=args.garrysmod/'garrysmod'
includes=list((root/'lua/includes').rglob('*.lua'))
sandbox=list((root/'gamemodes/sandbox').rglob('*.lua'))
assert includes and sandbox
corpus=sorted(set(includes+sandbox))
tests=sorted((Path(__file__).parent/'tests').glob('*.lua'))
for name,paths in [('syntax',corpus),('tests',tests)]:
    (args.out/(name+'.txt')).write_text('\n'.join(str(p.resolve()) for p in paths)+'\n',encoding='utf-8')
print('Corpus:',len(includes),'includes +',len(sandbox),'sandbox;',len(tests),'runtime tests')
