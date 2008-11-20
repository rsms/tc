#!/usr/bin/env python
import sys
from distutils.core import setup, Extension

if sys.version_info < (2, 3):
  raise Error, "Python 2.3 or later is required"

include_dirs = []
library_dirs = ['/usr/local/lib']

if sys.platform == 'darwin':
  # darwin ports
  include_dirs.append('/opt/local/include')
  library_dirs.append('/opt/local/lib')
  # fink
  include_dirs.append('/sw/include')
  library_dirs.append('/sw/lib')

ext = Extension('pytc',
                libraries = ['tokyocabinet', 'z', 'pthread', 'm', 'c'],
                sources = ['pytc.c'],
                include_dirs = include_dirs,
                library_dirs = library_dirs,
               )

setup(name = 'pytc',
      version = '0.4',
      description = 'Tokyo Cabinet Python bindings',
      long_description = '''
        Tokyo Cabinet Python bindings
      ''',
      license='BSD',
      author = 'Tasuku SUENAGA',
      author_email = 'gunyarakun@sourceforge.jp',
      ext_modules = [ext]
     )
