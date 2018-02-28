from building import *

cwd = GetCurrentDir()
src = Split('''
minilzo.c
''')
CPPPATH = [cwd]

if GetDepend('MINILZO_USING_SAMPLE'):
    src += Glob('minilzo_sample.c')

group = DefineGroup('minilzo', src, depend = ['PKG_USING_MINILZO'], CPPPATH = CPPPATH)

Return('group')
