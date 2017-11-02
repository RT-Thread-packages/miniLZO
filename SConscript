from building import *

cwd = GetCurrentDir()
src = Split('''
minilzo.c
lzo.c
''')
CPPPATH = [cwd]

group = DefineGroup('lzo', src, depend = ['PKG_USING_MINILZO'], CPPPATH = CPPPATH)

Return('group')
