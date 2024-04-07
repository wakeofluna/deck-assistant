import sys
import os
import subprocess

if os.name != 'nt':
  print('NOT SUPPORTED FOR NON-WINDOWS PLATFORMS')
  sys.exit(1)

libname = 'lua51.lib'

rootdir = os.path.abspath(os.environ['MESON_SOURCE_ROOT'])
srcdir = os.path.join(rootdir, os.environ['MESON_SUBDIR'])
workdir = os.path.join(srcdir, 'src')
libfile = os.path.join(workdir, libname)

if os.path.exists(libfile):
  print('Target already exists')
  sys.exit(0)

if not os.path.exists(libfile):
  subprocess.run(['.\\msvcbuild.bat', 'static'], cwd=workdir, shell=True)

if not os.path.exists(libfile):
  print('COMPILE FAILED')
  sys.exit(1)
