"""Independent PI2/PI4 circuit reduction at 80 decimal digits.

Build an (n+1)-vertex chain from the physical branch admittance and mutual
capacitances, then eliminate its internal vertices with a Schur complement.
The recovered driver instead solves two mixed current/voltage boundary problems
and converts their transfer coefficients to Y. No C++ factors, node snapshots,
or exported final admittances are used as inputs to this reduction.
"""
import json
import sys

import mpmath as mp


def matrix(values):
    return mp.matrix([[mp.mpc(*z) for z in row] for row in values])


def encode(values):
    return [[[float(mp.re(values[i, j])), float(mp.im(values[i, j]))]
             for j in range(values.cols)] for i in range(values.rows)]


def scalar(value):
    return float(value) if not mp.im(value) and mp.isfinite(value) else None


def metrics(y, frequency, shunts=None):
    # The native port labels reverse the two chain endpoints. Single-ended
    # ZIN uses the other port shorted, while differential ZIN drives [1,-1].
    angular = mp.mpf(float.fromhex('0x1.921fb54442d18p+2')) * frequency
    series = -y[1, 0]
    branches = [mp.mpc(*shunts['second']), mp.mpc(*shunts['first'])] if shunts else [y[1, 1] + y[1, 0], y[0, 0] + y[0, 1]]
    zs = 1 / series
    inductance, resistance = mp.im(zs) / angular, mp.re(zs)
    capacitances, resistances = [], []
    for branch in branches:
        if abs(branch) > mp.mpf(float.fromhex('0x1.b7cdfd9d7bdbbp-34')):
            z = 1 / branch
            capacitances.append(-1 / (angular * mp.im(z)))
            resistances.append(mp.re(z))
        else:
            capacitances.append(mp.mpf(0))
            resistances.append(mp.mpf(1e15))
    inverse = y**-1
    differential = (mp.matrix([[1, -1]]) * inverse * mp.matrix([1, -1]))[0]
    result = dict(frequency_hz=frequency, inductance_h=inductance,
                  resistance=resistance, capacitance_second_f=capacitances[0],
                  capacitance_first_f=capacitances[1],
                  resistance_second=resistances[0], resistance_first=resistances[1],
                  quality_first=-mp.im(y[1, 1]) / mp.re(y[1, 1]),
                  quality_second=-mp.im(y[0, 0]) / mp.re(y[0, 0]),
                  quality_differential=mp.im(differential) / mp.re(differential))
    for name, cap, res in [('single', capacitances[0], resistances[0]),
                           ('differential',
                            mp.fprod(capacitances) / sum(capacitances),
                            sum(resistances))]:
        # Parallel resistance is the reciprocal of the real branch admittance,
        # independently of the original expanded ratios and squared terms.
        if not cap:
            result['shunt_' + name] = mp.nan  # Original inf/inf classification.
        else:
            shunt = res - mp.j / (angular * cap)
            result['shunt_' + name] = 1 / mp.re(1 / zs + 1 / shunt)
    if not capacitances[0]:
        resonance = mp.inf
    else:
        ratio = capacitances[0] / inductance
        correction = (1 - resistance**2 * ratio) / (1 - resistances[0]**2 * ratio)
        resonance = mp.sqrt((correction if correction > 0 else 1) /
                            (inductance * capacitances[0])) / (angular / frequency)
    return dict(pi={key: scalar(value) for key, value in result.items()},
                resonance_hz=scalar(resonance),
                input_impedance=[[float(mp.re(z)), float(mp.im(z))]
                                 for z in [1 / y[1, 1], 1 / y[0, 0], differential]])


def reference(result):
    with mp.workdps(80):
        cap = matrix(result['capacitance_matrix'])
        branch = matrix(result['inductance_admittance_matrix'])
        n = cap.rows
        assert n == cap.cols == branch.rows == branch.cols and n > 0
        incidence = mp.matrix(n, n + 1)
        nodal_cap = mp.matrix(n + 1)
        for i in range(n):
            incidence[i, i], incidence[i, i + 1] = 1, -1
            for j in range(n):
                nodal_cap[i, j] = mp.fsum(cap[i, k] for k in range(n)) if i == j else -cap[i, j]
        nodal = nodal_cap + incidence.T * branch * incidence
        ports, internal = [n, 0], list(range(1, n))
        submatrix = lambda rows, cols: mp.matrix([[nodal[i, j] for j in cols] for i in rows])
        y = submatrix(ports, ports)
        if internal:
            y -= submatrix(ports, internal) * submatrix(internal, internal)**-1 * submatrix(internal, ports)
        stages = dict(node_capacitance_matrix=encode(nodal_cap[:n, :n]))
        if 'pad_capacitance_matrix' in result:
            pads = matrix(result['pad_capacitance_matrix'])
            assert pads.rows == pads.cols and pads.rows >= 2
            stages['with_pad_admittance_matrix'] = encode(y)
            # PI4 explicitly uses the first two folded pad groups, even for a
            # multisegment pad. Keep that original model choice at this boundary.
            a, b, d = pads[0, 0], pads[1, 0], pads[1, 1]
            y -= mp.matrix([[d + b, -b], [-b, a + b]])
        return dict(mpmath_version=mp.__version__, decimal_digits=80, groups=n,
                    matrix=encode(y), **stages,
                    circuit_shunts=dict(first=[float(mp.re(y[0, 0]+y[0, 1])), float(mp.im(y[0, 0]+y[0, 1]))],
                                        second=[float(mp.re(y[1, 0]+y[1, 1])), float(mp.im(y[1, 0]+y[1, 1]))]),
                    circuit_metrics=metrics(y, mp.mpf(result['frequency_hz'])),
                    reported_metrics=metrics(matrix(result['matrix']), mp.mpf(result['frequency_hz']), result.get('pi_shunts')))


if __name__ == '__main__':
    print(json.dumps(reference(json.load(sys.stdin)), allow_nan=False))
