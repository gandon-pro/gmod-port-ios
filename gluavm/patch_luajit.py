"""Apply the GLua lexical delta to the pinned LuaJIT. No source rewriting of Lua."""
import argparse
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('source',type=Path)
args=parser.parse_args()
source=args.source.resolve()
pin='a4c064539ea931274d407e1249b4bb575424a0f4'
assert subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()==pin
file=source/'src/lj_lex.c'
text=file.read_text(encoding='utf-8')
marker='/* GMODPORT: C-style comments, respecting lexer reader boundaries. */'
if marker not in text:
    before="    case '[': {\n      int sep = lex_skipeq(ls);"
    assert text.count(before)==1
    extra=r'''    case '/':
      /* GMODPORT: C-style comments, respecting lexer reader boundaries. */
      lex_next(ls);
      if (ls->c == '/') {
        do { lex_next(ls); } while (!lex_iseol(ls) && ls->c != LEX_EOF);
        continue;
      }
      if (ls->c == '*') {
        lex_next(ls);
        for (;;) {
          if (ls->c == LEX_EOF)
            lj_lex_error(ls, TK_eof, LJ_ERR_XLCOM);
          if (lex_iseol(ls)) {
            lex_newline(ls);
          } else if (ls->c == '*') {
            lex_next(ls);
            if (ls->c == '/') { lex_next(ls); break; }
          } else {
            lex_next(ls);
          }
        }
        continue;
      }
      return '/';
'''
    text=text.replace(before,extra+before)
    file.write_text(text,encoding='utf-8',newline='\n')

# The pinned upstream already implements != ! && || and scoped continue.
# Verify those prerequisites instead of silently assuming another revision.
lex=(source/'src/lj_lex.h').read_text()
parse=(source/'src/lj_parse.c').read_text()
for token in ['_(continue)','__(and_, &&)','__(or_, ||)','__(ne_, !=)']:
    assert token in lex, token
for code in ['parse_continue','fscope_continue','case TK_and_:','case TK_or_:','case TK_ne_:']:
    assert code in parse, code

file=source/'src/msvcbuild.bat'
text=file.read_text(encoding='utf-8')
if '/DLUAJIT_DISABLE_JIT /DLUAJIT_DISABLE_FFI' not in text:
    text=text.replace('@set LJCOMPILE=cl /nologo /c ', '@set LJCOMPILE=cl /nologo /c /MD /DLUAJIT_DISABLE_JIT /DLUAJIT_DISABLE_FFI ')
    text=text.replace('-D JIT ', '').replace('-D FFI ', '')
    # Upstream failure label prints an error but does not reliably propagate it.
    text=text.replace('@goto :END\n:FAIL', '@exit /b 1\n:FAIL')
    file.write_text(text,encoding='utf-8',newline='\r\n')
print('GLua lexical patch applied to',pin)
