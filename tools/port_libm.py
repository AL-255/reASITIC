"""Compile the original process's pinned glibc scalar sin/cos in portable C.

Only runtime headers/aliases are adapted. Numerical source files stay unmodified.
WASM IEEE binary64 operations supply i386 libm's explicit 53-bit precision mode.
The adapter does not expose errno or floating exception flags.
"""
from pathlib import Path
import hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]

def build(emxx,dist):
    vendor=ROOT/'vendor/glibc-math';manifest=json.loads((vendor/'provenance.json').read_text())
    for name,info in manifest['files'].items():
        if hashlib.sha256((vendor/name).read_bytes()).hexdigest()!=info['sha256']:raise RuntimeError('Pinned glibc math source changed: '+name)
    out=ROOT/'build/glibc-math';out.mkdir(exist_ok=True)
    for name in manifest['files']:
        if name.endswith(('.c','.h')):shutil.copy2(vendor/name,out/name)
    compatibility={
        'endian.h':'#pragma once\n#define LITTLE_ENDI 1\n#define LOW_HALF 0\n#define HIGH_HALF 1\n',
        'math_private.h':'#pragma once\n#define attribute_hidden\n#ifndef __always_inline\n#define __always_inline inline __attribute__((always_inline))\n#endif\n#define libm_alias_double(a,b)\n#define __set_errno(value) ((void)0)\n',
        'fenv_private.h':'#pragma once\n#define SET_RESTORE_ROUND_53BIT(mode) ((void)0)\n',
        'math-underflow.h':'#pragma once\n#define math_check_force_underflow(value) ((void)0)\n',
        'libm-alias-double.h':'#pragma once\n#define libm_alias_double(a,b)\n'
    }
    for name,text in compatibility.items():(out/name).write_text(text)
    flags=['-x','c','-std=gnu11','-O2','-fno-fast-math','-ffp-contract=off','-fno-builtin','-I'+str(out),
        '-D__sin=reasitic_glibc_sin','-D__cos=reasitic_glibc_cos','-D__branred=reasitic_glibc_branred','-D__sincostab=reasitic_glibc_sincostab']
    key=hashlib.sha256((subprocess.check_output([emxx,'--version'],text=True)+json.dumps(manifest,sort_keys=True)+repr(flags)+repr(compatibility)).encode()).hexdigest()[:16]
    objects=[]
    for name in ['s_sin','sincostab','branred']:
        obj=out/(name+'-'+key+'.o');objects.append(str(obj))
        if not obj.exists():subprocess.run([emxx,*flags,'-c',str(out/(name+'.c')),'-o',str(obj)],check=True)
    license_dir=dist/'licenses/glibc-math';license_dir.mkdir(parents=True,exist_ok=True)
    for name in ['COPYING','COPYING.LIB','NOTICE']:shutil.copy2(vendor/name,license_dir/name)
    return objects
