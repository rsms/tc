#!/usr/bin/env python
import sys, os
from distutils.core import setup, Extension
from subprocess import Popen, PIPE
from distutils import log
import shutil, time, platform

# -----------------------------------------------------------------------------
# Bootstrap

if sys.version_info < (2, 3):
  raise Error, "Python 2.3 or later is required"

os.chdir(os.path.dirname(os.path.abspath(__file__)))

__version__ = '?'
execfile(os.path.join("lib", "pytc", "release.py"))
system_config_h = os.path.join("src", "system_config.h")
libraries = [ ['tokyocabinet', ['tchdb.h']] ]
sources = ['src/pytc.c']
X86_MACHINES = ['i386', 'i686', 'i86pc', 'amd64', 'x86_64']

# -----------------------------------------------------------------------------
# Helpers

copy_file = shutil.copy

def shell_cmd(args, cwd=None):
  '''Returns stdout as string or None on failure
  '''
  if cwd is None:
    cwd = os.path.abspath('.')
  if not isinstance(args, (list, tuple)):
    args = [args]
  ps = Popen(args, shell=True, cwd=cwd, stdout=PIPE, stderr=PIPE,
             close_fds=True)
  stdout, stderr = ps.communicate()
  if ps.returncode != 0:
    if stderr:
      stderr = stderr.strip()
    raise IOError('Shell command %s failed (exit status %r): %s' %\
      (args, ps.returncode, stderr))
  return stdout.strip()

# -----------------------------------------------------------------------------
# Commands

from distutils.command.build_ext import build_ext as _build_ext
class build_ext(_build_ext):
  description = 'build the C extension (compile/link to build directory)'
  
  def finalize_options(self):
    _build_ext.finalize_options(self)
    # Ports
    if os.path.isdir('/opt/local/include'):
      self.include_dirs.append('/opt/local/include')
      self.library_dirs.append('/opt/local/lib')
    # Fink
    if os.path.isdir('/opt/local/include'):
      self.include_dirs.append('/sw/include')
      self.library_dirs.append('/sw/lib')
    # Other
    self.library_dirs.append('/usr/local/lib')
    # Define macros
    if not isinstance(self.define, list):
      self.define = []
    self.define.append(('PYTC_VERSION', '"%s"' % __version__))
    # Remove NDEBUG if bulding with debug
    if not isinstance(self.undef, list):
      self.undef = []
    # Update distribution
    self.distribution.include_dirs = self.include_dirs
    self.distribution.library_dirs = self.library_dirs
  
  def run(self):
    self._run_config_if_needed()
    self._configure_compiler()
    log.debug('include dirs: %r', self.include_dirs)
    log.debug('library dirs: %r', self.library_dirs)
    _build_ext.run(self)
  
  def _configure_compiler(self):
    machine = platform.machine()
    cflags = ''
    
    log.debug("configuring compiler...")
    
    #cflags += ' -include "%s"' % os.path.realpath(os.path.join(
    #  os.path.dirname(__file__), "src", "prefix.h"))
    
    # Warning flags
    warnings = ['all', 'no-unknown-pragmas']
    cflags += ''.join([' -W%s' % w for w in warnings])
    
    if '--debug' in sys.argv:
      log.debug("build mode: debug -- setting appropriate cflags and macros")
      self.debug = True
      cflags += ' -O0'
      self.define.append(('DEBUG', '1'))
      self.undef.append('NDEBUG')
    else:
      log.debug("build mode: release -- setting appropriate cflags and macros")
      self.debug = False
      cflags += ' -Os'
      if machine in X86_MACHINES:
        log.debug("Enabling SSE3 support")
        cflags += ' -msse3'
        if platform.system() == 'Darwin':
          cflags += ' -mssse3'
    # set c flags
    if 'CFLAGS' in os.environ:
      os.environ['CFLAGS'] += cflags
    else:
      os.environ['CFLAGS'] = cflags
  
  def _run_config_if_needed(self):
    try:
      # If system_config.h is newer than this file and exists: don't create it again.
      if not self.force and os.path.getmtime(system_config_h) > os.path.getmtime(__file__):
        return
    except os.error:
      pass
    self.run_command('config')
  

from distutils.command.config import config as _config
class config(_config):
  description = 'configure build (almost like "./configure")'
  
  def initialize_options (self):
    _config.initialize_options(self)
    self.noisy = 0
    self.dump_source = 0
    self.macros = {}
  
  def finalize_options(self):
    _config.finalize_options(self)
    for path in self.distribution.library_dirs:
      if path not in self.library_dirs:
        self.library_dirs.append(path)
    for path in self.distribution.include_dirs:
      if path not in self.include_dirs:
        self.include_dirs.append(path)
  
  def run(self):
    self._libraries()
    self._write_system_config_h()
  
  def _silence(self):
    self.orig_log_threshold = log._global_log.threshold
    log._global_log.threshold = log.WARN
  
  def _unsilence(self):
    log._global_log.threshold = self.orig_log_threshold
  
  def _libraries(self):
    global libraries
    for n in libraries:
      log.info('checking for library %s', n[0])
      sys.stdout.flush()
      self._silence()
      ok = self.check_lib(library=n[0], headers=n[1])
      self._unsilence()
      if not ok:
        log.error("missing required library %s" % n[0])
        sys.exit(1)
  
  def _write_system_config_h(self):
    import re
    f = open(system_config_h, "w")
    try:
      try:
        f.write("/* Generated by setup.py at %s */\n" % time.strftime("%c %z"))
        f.write("#ifndef PYTC_SYSTEM_CONFIG_H\n")
        f.write("#define PYTC_SYSTEM_CONFIG_H\n\n")
        for k,v in self.macros.items():
          f.write("#ifndef %s\n" % k)
          f.write(" #define %s %s\n" % (k, str(v)) )
          f.write("#endif\n")
        f.write("\n#endif\n")
        log.info('wrote compile-time configuration to %s', system_config_h)
      finally:
        f.close()
    except:
      os.remove(system_config_h)
      raise
  
  def check_lib (self, library, library_dirs=None, headers=None, include_dirs=None, 
                 other_libraries=[]):
    self._check_compiler()
    return self.try_link("int main (void) { return 0; }", headers, include_dirs, 
    [library]+other_libraries, library_dirs)
  

from distutils.dist import Distribution
class PyTCDistribution(Distribution):
  def __init__(self, attrs=None):
    Distribution.__init__(self, attrs)
    self.cmdclass = {
      #'build': build,
      'build_ext': build_ext,
      'config': config,
      #'docs': sphinx_build,
      #'clean': clean,
    }
    try:
      shell_cmd('which dpkg-buildpackage')
      self.cmdclass['debian'] = debian
    except IOError:
      pass
  

# -----------------------------------------------------------------------------

# Package
setup(name = 'pytc',
      version = __version__,
      description = 'Tokyo Cabinet Python bindings',
      long_description = '''
      Tokyo Cabinet Python bindings
      ''',
      license='BSD',
      author = 'Tasuku SUENAGA',
      author_email = 'gunyarakun@sourceforge.jp',
      distclass = PyTCDistribution,
      ext_modules = [Extension('_pytc',
        libraries = [v[0] for v in libraries],
        sources = sources
       )],
      package_dir = {'': 'lib'},
      packages = ['pytc']
     )
