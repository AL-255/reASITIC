"""Build the recovered layered Green equations with ordinary binary64 arithmetic.

The reference implementation is preserved in port_geometry. Production uses
the same layer transfers, spectral weights, FFT and original constants here,
without emulating x87 operations inside every spectral sample/butterfly.
"""
import re
from port_geometry import source


def generate(target):
    text = source('potential_tables')
    text = text[text.index('namespace asitic {') + len('namespace asitic {'):text.rfind('}')]
    dispatch='\n#ifdef __EMSCRIPTEN__\n    if(!reasitic::reference_numerics()){reasitic::generate_portable_potential_tables(source,target,g,out,memory,status);return;}\n#endif\n'
    assert text.count(dispatch)==1
    text=text.replace(dispatch,'\n')
    start = text.index('double potential_tanh(long double x) {')
    end = text.index('\nstruct Transfer', start)
    text = text[:start] + 'double potential_tanh(double x) { return ::tanh(x); }\n' + text[end:]
    text = text.replace('long double', 'double')
    text = re.sub(r'(?<![\w.])((?:0x[\da-f]+(?:\.[\da-f]*)?p[+-]?\d+|\d+(?:\.\d*)?(?:e[+-]?\d+)?))L\b', r'\1', text, flags=re.I)
    text = text.replace('using x87::binary64;', '')
    text = text.replace('x87::binary64', 'binary64')
    for name in ['sin', 'exp', 'sqrt']:
        text = text.replace('x87::' + name, '::' + name)
    text = text.replace('legacy::divide', 'divide')
    # Keep these helpers separate from the historical functions during ADL.
    for name in ['potential_clipped_cosh', 'potential_fft', 'transform_potential_grid_allocated', 'transform_potential_grid', 'potential_tanh', 'potential_transfer_up', 'potential_transfer_down', 'potential_source_up', 'potential_source_down', 'potential_spectral_a', 'potential_spectral_b', 'generate_potential_tables']:
        text = re.sub(r'\b' + name + r'\b', 'portable_' + name, text)
    assert 'x87::' not in text and 'asm(' not in text and 'long double' not in text
    prefix = '''// Generated from the pinned recovered Green equations; do not edit.
#include "portable_potential.hpp"
#include "numerical_policy.hpp"
#include <cmath>
namespace reasitic::portable_green {
using namespace asitic;
inline double binary64(double x){return x;}
inline ComplexValue divide(ComplexValue a,ComplexValue b){
    if(b.imag==0)return {a.real/b.real,a.imag/b.real};
    if(std::abs(b.imag)>=std::abs(b.real)){
        const double r=b.real/b.imag,d=r*r*b.imag+b.imag;
        return {(a.real*r+a.imag)/d,(a.imag*r-a.real)/d};
    }
    const double r=b.imag/b.real,d=r*r*b.real+b.real;
    return {(a.imag*r+a.real)/d,(a.imag-a.real*r)/d};
}
'''
    suffix = '''
}
namespace reasitic {
void generate_portable_potential_tables(int source,int target,
    const asitic::PotentialTableGeometry& g,asitic::PotentialTableOutput out,
    asitic::SolverMemory memory,asitic::WarningSink status){
    portable_green::portable_generate_potential_tables(source,target,g,out,memory,status);
}
}
'''
    output = target / 'portable_potential.cpp'
    output.write_text(prefix + text + suffix)
    return output
