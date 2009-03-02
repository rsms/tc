#!/usr/bin/env python
import sys, os, shutil, time, platform
from subprocess import Popen, PIPE
from distutils.core import setup, Extension, Command
from distutils import log
from distutils.dir_util import remove_tree

# -----------------------------------------------------------------------------
# Bootstrap

if sys.version_info < (2, 3):
  exec('raise Error, "Python 2.3 or later is required"')

# remove --no-user-cfg
for i,arg in enumerate(sys.argv[:]):
  if arg == '--no-user-cfg':
    del sys.argv[i]
    break

os.chdir(os.path.dirname(os.path.abspath(__file__)))

__version__ = '?'
exec(open(os.path.join("lib", "tc", "release.py")).read())
system_config_h = os.path.join("src", "system_config.h")
libraries = [ ['tokyocabinet', ['tchdb.h']] ]
X86_MACHINES = ['i386', 'i686', 'i86pc', 'amd64', 'x86_64']
sources = [
  'src/__init__.c',
  'src/util.c',
  'src/HDB.c',
  'src/BDB.c',
  'src/BDBCursor.c'
]

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

def read(*rnames):
  return open(os.path.join(*rnames)).read()

def rm_file(path):
  if os.access(path, os.F_OK):
    os.remove(path)
    log.info('removed file %s', path)

def rm_dir(path):
  if os.access(path, os.F_OK):
    remove_tree(path)
    log.info('removed directory %s', path)

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
  description = 'configure build (automatically runned by build commands)'
  
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
  

class sphinx_build(Command):
  description = 'build documentation using Sphinx'
  user_options = [
    ('builder=', 'b', 'builder to use; default is html'),
    ('all', 'a', 'write all files; default is to only write new and changed files'),
    ('reload-env', 'E', "don't use a saved environment, always read all files"),
    ('source-dir=', 's', 'path where sources are read from (default: docs/source)'),
    ('out-dir=', 'o', 'path where output is stored (default: docs/<builder>)'),
    ('cache-dir=', 'd', 'path for the cached environment and doctree files (default: outdir/.doctrees)'),
    ('conf-dir=', 'c', 'path where configuration file (conf.py) is located (default: same as source-dir)'),
    ('set=', 'D', '<setting=value> override a setting in configuration'),
    ('no-color', 'N', 'do not do colored output'),
    ('pdb', 'P', 'run Pdb on exception'),
  ]
  boolean_options = ['all', 'reload-env', 'no-color', 'pdb']
  
  def initialize_options(self):
    self.sphinx_args = []
    self.builder = None
    self.all = False
    self.reload_env = False
    self.source_dir = None
    self.out_dir = None
    self.cache_dir = None
    self.conf_dir = None
    self.set = None
    self.no_color = False
    self.pdb = False
  
  def finalize_options(self):
    self.sphinx_args.append('sphinx-build')
    
    if self.builder is None:
      self.builder = 'html'
    self.sphinx_args.extend(['-b', self.builder])
    
    if self.all:
      self.sphinx_args.append('-a')
    if self.reload_env:
      self.sphinx_args.append('-E')
    if self.no_color or ('PS1' not in os.environ and 'PROMPT_COMMAND' not in os.environ):
      self.sphinx_args.append('-N')
    if not self.distribution.verbose:
      self.sphinx_args.append('-q')
    if self.pdb:
      self.sphinx_args.append('-P')
    
    if self.cache_dir is not None:
      self.sphinx_args.extend(['-d', self.cache_dir])
    if self.conf_dir is not None:
      self.sphinx_args.extend(['-c', self.conf_dir])
    if self.set is not None:
      self.sphinx_args.extend(['-D', self.set])
    
    if self.source_dir is None:
      self.source_dir = os.path.join('docs', 'source')
    if self.out_dir is None:
      self.out_dir = os.path.join('docs', self.builder)
    
    self.sphinx_args.extend([self.source_dir, self.out_dir])
  
  def run(self):
    try:
      import sphinx
      if not os.path.exists(self.out_dir):
        if self.dry_run:
          self.announce('skipping creation of directory %s (dry run)' % self.out_dir)
        else:
          self.announce('creating directory %s' % self.out_dir)
          os.makedirs(self.out_dir)
      if self.dry_run:
        self.announce('skipping %s (dry run)' % ' '.join(self.sphinx_args))
      else:
        self.announce('running %s' % ' '.join(self.sphinx_args))
        sphinx.main(self.sphinx_args)
    except ImportError:
      log.info('Sphinx not installed -- skipping documentation. (%s)', sys.exc_info()[1])
  

from distutils.command.clean import clean as _clean
class clean(_clean):
  def run(self):
    _clean.run(self)
    log.info('removing files generated by setup.py')
    for path in ['MANIFEST', 'src/system_config.h']:
      rm_file(path)
    log.info('removing generated documentation')
    rm_dir('docs/html')
    rm_dir('docs/latex')
    rm_dir('docs/pdf')
    rm_dir('docs/text')
  

from distutils.command.sdist import sdist as _sdist
class sdist(_sdist):
  def run(self):
    try:
      _sdist.run(self)
    finally:
      for path in ['MANIFEST']:
        rm_file(path)
  

# -----------------------------------------------------------------------------
# Distribution

from distutils.dist import Distribution
class TCDistribution(Distribution):
  def __init__(self, attrs=None):
    Distribution.__init__(self, attrs)
    self.cmdclass = {
      'config': config,
      'build_ext': build_ext,
      'sdist': sdist,
      'docs': sphinx_build,
      'clean': clean,
    }
    try:
      shell_cmd('which dpkg-buildpackage')
      self.cmdclass['debian'] = debian
    except IOError:
      pass
  

# -----------------------------------------------------------------------------
# Main

setup(name = 'tc',
      version = __version__,
      description = 'Python bindings to the Tokyo Cabinet database library',
      long_description = (
        "\n"+read('README.rst')
        + "\n"
        + read('CHANGELOG.rst')
        + '\n'
        'Download\n'
        '========\n'
      ),
      license = read('LICENSE'),
      author = 'Rasmus Andersson',
      author_email = 'rasmus@notion.se',
      url = 'http://github.com/rsms/tc',
      distclass = TCDistribution,
      ext_modules = [Extension('_tc',
        libraries = [v[0] for v in libraries],
        sources = sources
       )],
      package_dir = {'': 'lib'},
      packages = ['tc'],
      platforms = "ALL",
      classifiers = [
        'Development Status :: 4 - Beta',
        'License :: OSI Approved :: BSD License',
        'Intended Audience :: Developers',
        'Intended Audience :: Information Technology',
        'Natural Language :: English',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: POSIX',
        'Operating System :: Unix',
        'Programming Language :: C',
        'Programming Language :: Python',
        'Topic :: Database',
        'Topic :: Software Development',
        'Topic :: System :: Clustering',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.3',
        'Programming Language :: Python :: 2.4',
        'Programming Language :: Python :: 2.5',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.0',
      ],
     )
