"""Exercise the packaged webpage in Chrome, including its untouched Q sweep."""
import argparse
import csv
import functools
import gzip
import http.server
import json
import math
import shutil
import struct
import threading
import time
from pathlib import Path
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', help='Published site; otherwise serve the local dist directory')
    args = parser.parse_args()
    evidence = ROOT / 'build/evidence/browser-pi2-q'
    evidence.mkdir(parents=True, exist_ok=True)
    server = None
    if not args.url:
        class Handler(http.server.SimpleHTTPRequestHandler):
            def log_message(self, *args):
                pass
        server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Handler, directory=str(ROOT / 'dist')))
        threading.Thread(target=server.serve_forever, daemon=True).start()
    url = args.url or f'http://127.0.0.1:{server.server_port}/'
    report = {'url': url, 'renderedBrowserQA': True, 'scenarios': []}
    original = json.loads(gzip.decompress((ROOT / 'tests/fixtures/original-analysis-200.json.gz').read_bytes()))
    refs = {c['frequency_hz']: c for c in original['cases'] if c['id'].startswith('cmos-starting-square-')}
    errors = []
    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(executable_path=shutil.which('google-chrome'), headless=True, args=['--no-sandbox'])
            report['browserVersion'] = browser.version
            page = browser.new_page(viewport={'width': 1440, 'height': 1100}, accept_downloads=True)
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.goto(url, wait_until='networkidle')
            page.wait_for_function("!document.getElementById('network-quality-sweep').disabled")
            field = lambda id: page.locator('#' + id)
            report['release'] = page.request.get(url.rstrip('/') + '/build.json').json()['release']
            assert field('process').input_value() == 'CMOS'
            defaults = {id: field(id).input_value() for id in ['cell-length', 'cell-width', 'cell-thickness', 'cap-length', 'cap-width', 'cap-fftx', 'cap-ffty', 'network-start', 'network-end', 'network-step']}
            assert defaults == dict(zip(defaults, ['50', '1.5', '0.5', '50', '20', '256', '256', '1', '40', '2'])), defaults
            # No input changes before this click: this is the broken user flow.
            field('network-quality-sweep').click()
            page.wait_for_function("document.getElementById('network-sweep-state').textContent.includes('Solving PI2 Q sample 1/20')")
            assert field('network-stop').is_enabled()
            assert field('generate').is_disabled()
            started = time.monotonic()
            previous = None
            while field('network-quality-sweep').is_disabled():
                state = field('network-sweep-state').inner_text()
                if state != previous:
                    print(state, flush=True)
                    previous = state
                assert not errors, errors
                assert time.monotonic() - started < 1800, 'Default sweep timed out'
                page.wait_for_timeout(1000)
            assert 'solve complete' in field('status').inner_text(), field('status').inner_text()
            assert page.locator('#network-q-plot circle').count() == 20
            assert field('network-stop').is_disabled()
            assert field('generate').is_enabled()
            with page.expect_download() as download:
                field('network-sweep-csv').click()
            path = evidence / 'default-sweep.csv'
            download.value.save_as(path)
            rows = list(csv.DictReader(path.open()))
            assert len(rows) == 20 and all(r['model'] == 'PI2' for r in rows)
            # Original PI2 command input stores Hz in a binary32 argument word.
            assert [float(r['frequency_hz']) for r in rows] == [struct.unpack('<f', struct.pack('<f', i * 10**9))[0] for i in range(1, 40, 2)]
            values = [float(r['quality_first']) for r in rows]
            assert all(map(math.isfinite, values))
            peak = max(range(len(values)), key=values.__getitem__)
            assert 0 < peak < len(values) - 1 and values[-1] < values[peak]
            comparisons = []
            columns = {'pi.inductance_nh': ('inductance_h', 1e9), 'pi.resistance_ohm': ('resistance_ohm', 1), 'pi.c1_ff': ('capacitance_first_f', 1e15), 'pi.c2_ff': ('capacitance_second_f', 1e15), 'pi.r1_ohm': ('resistance_first_ohm', 1), 'pi.r2_ohm': ('resistance_second_ohm', 1), 'pi.q1': ('quality_first', 1), 'pi.q2': ('quality_second', 1), 'pi.qd': ('quality_differential', 1), 'pi.resonance_ghz': ('resonance_hz', 1e-9)}
            for row in rows:
                ref = refs.get(float(row['frequency_hz']))
                if not ref:
                    continue
                for key, (column, scale) in columns.items():
                    want = ref['values'][key]
                    if want is None:
                        assert row[column] == ''
                        continue
                    actual = float(row[column]) * scale
                    error = abs(actual - want) / abs(want) if want else 0 if actual == 0 else math.inf
                    assert error < .001, (ref['id'], key, actual, want, error)
                    comparisons.append({'case': ref['id'], 'metric': key, 'relativeError': error})
            assert len(comparisons) == 40, 'Expected ten PI2 metrics at four original frequencies'
            for definition, label in [('quality_first', 'Port 1 Q'), ('quality_second', 'Port 2 Q'), ('quality_differential', 'Differential Q')]:
                field('network-q-definition').select_option(definition)
                assert field('network-q-label').inner_text() == label
                assert abs(float(field('network-q').inner_text()) - float(rows[-1][definition])) < abs(float(rows[-1][definition])) * 1e-6
                assert page.locator('#network-q-plot circle').count() == 20
            field('network-q-definition').select_option('quality_first')
            page.locator('#network-quality-sweep').locator('xpath=ancestor::section[1]').screenshot(path=str(evidence / 'default-sweep.png'))
            report['scenarios'].append({'name': 'untouched-defaults', 'defaults': defaults, 'samples': len(rows), 'seconds': time.monotonic() - started, 'peakGHz': float(rows[peak]['frequency_hz']) / 1e9, 'originalComparisons': comparisons, 'allQualityDefinitions': True})
            (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

            # A deliberately coarse user selection must fail visibly and recover.
            field('cap-fftx').fill('128'); field('cap-ffty').fill('128')
            field('network-quality-sweep').click()
            page.wait_for_function("!document.getElementById('network-quality-sweep').disabled")
            assert 'contact collapses' in field('status').inner_text(), field('status').inner_text()
            assert 'contact collapses' in field('network-sweep-state').inner_text(), 'Sweep error was only shown away from the Q controls'
            assert page.locator('#network-q-plot circle').count() == 0

            # Retain user FFT settings through real generation and edit handlers.
            field('cap-fftx').fill('64'); field('cap-ffty').fill('64')
            page.get_by_text('Original creator commands', exact=True).click()
            medium = json.loads((ROOT / 'tests/fixtures/quality-sweep-cases.json').read_text())['cases'][0]
            field('native-command').fill(medium['command'])
            field('generate').click()
            page.wait_for_function("!document.getElementById('generate').disabled")
            assert field('cap-fftx').input_value() == field('cap-ffty').input_value() == '64'
            page.get_by_text('Edit and assemble layout', exact=True).click()
            field('edit-commands').fill('UNSELECT'); field('apply-edit').click()
            page.wait_for_function("!document.getElementById('apply-edit').disabled")
            assert field('cap-fftx').input_value() == field('cap-ffty').input_value() == '64'
            for id in ['cell-length', 'cell-width', 'cell-thickness', 'cap-length', 'cap-width']:
                field(id).fill('1000')
            for id, value in [('network-start', '5'), ('network-end', '10'), ('network-step', '2.5')]:
                field(id).fill(value)
            field('network-quality-sweep').click()
            page.wait_for_function("!document.getElementById('network-quality-sweep').disabled", timeout=120000)
            assert page.locator('#network-q-plot circle').count() == 3, field('status').inner_text()
            assert '7.5000 GHz' in field('network-q-note').inner_text(), field('network-q-note').inner_text()
            assert abs(float(field('network-q').inner_text()) - 5.273) < .00051
            page.locator('#network-quality-sweep').locator('xpath=ancestor::section[1]').screenshot(path=str(evidence / 'medium-sweep.png'))
            report['scenarios'].append({'name': 'coarse-error-recovery-and-medium-coil', 'samples': 3, 'peakGHz': 7.5, 'preservedFFTThroughGenerationAndEdit': True})
            assert not errors, errors
            report['passed'] = True
            browser.close()
    finally:
        report['pageErrors'] = errors
        (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        if server:
            server.shutdown()
    print('Chrome: untouched 20-point default PI2 Q sweep, 40 original comparisons, three Q definitions, CSV export, finer-grid defaults, retained mesh and error recovery pass', flush=True)


if __name__ == '__main__':
    main()
