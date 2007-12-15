#!/usr/bin/env python
import sys
from distutils.core import setup, Extension

if sys.version_info < (2, 3):
  raise Error, "Python 2.3 or later is required"

ext = Extension('pytc',
                libraries = ['tokyocabinet', 'z', 'pthread', 'm', 'c'],
                sources = ['pytc.c'])

setup(name = 'pytc',
      version = '0.2',
      description = 'Tokyo Cabinet Python bindings',
      long_description = '''
        Tokyo Cabinet Python bindings
      ''',
      license='BSD',
      author = 'Tasuku SUENAGA',
      author_email = 'gunyarakun@sourceforge.jp',
      ext_modules = [ext]
     )
