"""Build the chunk 18 lifecycle gate fixture.

It is the chunk 14 live-binding fixture with one signal added and nothing bound to it.

That signal is the whole point. PLAN 4.8's partition separates a sample that reached the screen
from one that was acquired and never displayed, and names three reasons for the second. The only
one reachable without forcing the acquisition and presentation sides out of step is the first: the
signal is in the registry but no visible component binds it, so every frame acquires its samples
and none of them is ever rendered.

Without it the gate would only ever see `presented`, and a classifier that has only seen its happy
path is a classifier nobody has tested.
"""
import importlib.util
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
DOCUMENTS = ROOT / 'tests/fixtures/documents/valid'
NAME = 'lifecycle-stage0'

SPEC = importlib.util.spec_from_file_location('gen_chunk14_fixture', ROOT / 'tools/gen-chunk14-fixture.py')
CHUNK14 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHUNK14)
CHUNK12 = CHUNK14.CHUNK12
CHUNK11 = CHUNK12.CHUNK11

UNBOUND = 'coolant_temperature'


def signals():
    declared = CHUNK12.signals()
    # Declared, given a freshness deadline like any other, and bound by nothing. The scenario
    # source generates values for every declared signal, so this one really does produce samples
    # that really do reach the registry and really are never drawn.
    declared['signals'].append({'id': UNBOUND, 'type': 'number', 'unit': 'degC',
                                'freshness_deadline_ms': 200, 'discrete': False})
    return declared


def generate():
    document = CHUNK14.dashboard()
    outputs = CHUNK12.write_package(NAME, document, signals())

    png = CHUNK12.face_png()
    target = DOCUMENTS / (NAME + '.json')
    CHUNK11.write_json(target, {'dashboard': document, 'signals': signals(),
                                'manifest': {
                                    'schema_version': 1, 'package_id': 'unrealdash.chunk18.gate',
                                    'revision': 1, 'runtime_compatibility': 'stage0',
                                    'assets': {'assets/face.png': {'width': 60, 'height': 60,
                                                                   'format': 'rgba8', 'bytes': len(png)}}}})
    outputs.append(target)
    print('Generated ' + NAME)
    return outputs


if __name__ == '__main__':
    generate()
