"""Compile the unchanged recovered geometry with original x87 arithmetic."""
from pathlib import Path
import subprocess
from port_geometry import ROOT,SOURCES,source
(ROOT/'build').mkdir(exist_ok=True)
(ROOT/'build/estimate.cpp').write_text(source('estimate'))
subprocess.run(['g++','-std=c++17','-O2','-m32','-mfpmath=387','-fno-fast-math','-ffp-contract=off','-fno-pie','-fno-pic','-fno-exceptions','-fno-rtti','-fno-stack-protector','-fno-builtin','-nostdlib','-no-pie',
               '-I'+str(ROOT/'vendor/asitic/include'),'-I'+str(ROOT/'vendor/asitic/src'),
               *[str(ROOT/('build/estimate.cpp' if n=='estimate' else 'vendor/asitic/src/'+n+'.cpp')) for n in SOURCES],str(ROOT/'kernel/bridge.cpp'),str(ROOT/'tests/native_geometry.cpp'),
               '/lib/i386-linux-gnu/libc.so.6','-Wl,--dynamic-linker=/lib/ld-linux.so.2','-o',str(ROOT/'build/native-geometry')],check=True)
