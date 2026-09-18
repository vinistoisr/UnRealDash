"""Build the chunk 14 live-binding gate fixture.

It is the chunk 12 dial fixture with two changes, and both exist so the gate can measure something
a still capture could not.

1. The dial declares a VISIBLE stale presentation. Chunk 12's fixture used the inert default, all
   four states hidden, because nothing could change a state after construction and a hidden state
   was as good as any. Now a signal really does go stale when its deadline passes, and criterion 4
   measures the status band that appears when it does.

2. The face image stays unbound, which makes it the control. If everything in the frame moved
   between two captures, the gate would pass on a tree that rebuilt itself rather than on a binding
   that worked.

Everything else is deliberately shared with the dial fixture, including the colours, so a
measurement here means the same thing it means there.
"""
import importlib.util
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
PACKAGES = ROOT / 'tests/fixtures/packages'
DOCUMENTS = ROOT / 'tests/fixtures/documents/valid'
NAME = 'live-stage0'

SPEC = importlib.util.spec_from_file_location('gen_chunk12_fixture', ROOT / 'tools/gen-chunk12-fixture.py')
CHUNK12 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHUNK12)
CHUNK11 = CHUNK12.CHUNK11


def dashboard():
    document = CHUNK12.dashboard()
    # The dial shows its state. dash on a primitive with no value text hides the content and lets
    # the band carry the state alone, which is the rendering chunk 11 revision 4 C2 defines, so a
    # stale dial is a coloured band and no needle.
    document['components']['dial']['missing_data'] = CHUNK11.missing(
        ('dash', 'state_stale'), ('hidden', 'state_unavailable'),
        ('last_value_dimmed', 'state_invalid'), ('icon', 'state_age_unknown'))
    return document


def generate():
    document = dashboard()
    outputs = CHUNK12.write_package(NAME, document)

    # The validator corpus takes the sidecar form, so the semantic pass can resolve the binding's
    # signal the same way the runtime does.
    png = CHUNK12.face_png()
    target = DOCUMENTS / (NAME + '.json')
    CHUNK11.write_json(target, {'dashboard': document, 'signals': CHUNK12.signals(),
                                'manifest': {
                                    'schema_version': 1, 'package_id': 'unrealdash.chunk14.gate',
                                    'revision': 1, 'runtime_compatibility': 'stage0',
                                    'assets': {'assets/face.png': {'width': 60, 'height': 60,
                                                                   'format': 'rgba8', 'bytes': len(png)}}}})
    outputs.append(target)
    print('Generated ' + NAME)
    return outputs


if __name__ == '__main__':
    generate()
