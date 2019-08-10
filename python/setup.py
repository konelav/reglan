import os
import re

from distutils.core import setup, Extension

sources = ['reglanmodule.c']
extra_compile_args = ["-DNOERRMSG", "-O3"]

RE_VERSION = re.compile(r'#define\s+VERSION\s+"(?P<version>[^"]+)"')
version = None
with open('../src/reglan.h', 'r') as hdr:
    for line in hdr.readlines():
        m = RE_VERSION.search(line)
        if m is not None:
            version = m.group('version')
            break

reglanmodule = Extension('reglan', 
    sources=sources,
    extra_compile_args=extra_compile_args)

setup(name='reglan',
      version=version,
      description='Regular language generator',
      ext_modules=[reglanmodule])
