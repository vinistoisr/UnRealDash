"""Build the connector gate fixture: the live dashboard, fed by a real socket.

It is the chunk 14 live fixture plus a definition pack, and the pack is what makes the difference.
The scenario source numbers signals by their position in the document, because it writes the
recording from the same array. A telemetry stream cannot: the numbers come off the wire, from the
pack's own fields. So the pack declares a field named exactly as the document's signal, and
FDashBindingTable is told the mapping rather than assuming one.

The frame matches scripts/mock-relay.py, which matches BinaryTelemetryV1.cpp: frame 3200, an eight
byte payload, four little endian uint16 fields. Field 1 is the one the gauges read, and the relay
sweeps it over the full uint16 range, which is why the document's gauges declare 0 to 65535 here
rather than the 0 to 100 the scenario fixture uses.
"""
import importlib.util
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
DOCUMENTS = ROOT / 'tests/fixtures/documents/valid'
NAME = 'connector-stage0'
FRAME_ID = 3200
SIGNAL = 'engine_speed'
FULL_SCALE = 65535

SPEC = importlib.util.spec_from_file_location('gen_chunk12_fixture', ROOT / 'tools/gen-chunk12-fixture.py')
CHUNK12 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHUNK12)
CHUNK11 = CHUNK12.CHUNK11


def definition_pack():
    """One frame, four fields, named so the document's binding resolves against it.

    Only field 1 carries a name the document uses. The other three are declared because the frame
    really does carry them and a pack that lied about the payload would decode the wrong bytes.
    """
    def field(name, offset, scale=1.0):
        return {'name': name, 'byte_offset': offset, 'type': 'uint16', 'byte_order': 'little',
                'signed': False, 'scale': scale, 'offset': 0.0, 'unit': 'dimensionless',
                'acquisition': 'live', 'sentinels': []}

    return {
        'id': 'unrealdash.connector.gate',
        'version': '0.1.0',
        'api': 1,
        'input': 'binary-telemetry-v1',
        'frames': [{
            'id': FRAME_ID,
            'id_format': 'extended',
            'length': 8,
            'role': 'telemetry',
            'signals': [field('held_marker', 0), field(SIGNAL, 2),
                        field('scaled', 4, 0.1), field('offset_field', 6)],
        }],
    }


def dashboard():
    document = CHUNK12.dashboard()
    # The relay sweeps the full uint16 range, so the gauges have to be scaled for it or the needle
    # would sit at maximum from the first frame. Chunk 14 learned this the other way round: a
    # source sweeping 0 to 1 against a gauge declared 0 to 100 moves the needle two pixels.
    for component in ('dial', 'arc', 'bar'):
        document['components'][component]['properties']['maximum'] = FULL_SCALE
    document['components']['dial']['missing_data'] = CHUNK11.missing(
        ('dash', 'state_stale'), ('hidden', 'state_unavailable'),
        ('last_value_dimmed', 'state_invalid'), ('icon', 'state_age_unknown'))
    return document


def generate():
    document = dashboard()
    png = CHUNK12.face_png()
    manifest = {
        'schema_version': 1, 'package_id': 'unrealdash.connector.gate', 'revision': 1,
        'runtime_compatibility': 'stage0',
        'assets': {'assets/face.png': {'width': 60, 'height': 60, 'format': 'rgba8',
                                       'bytes': len(png)}},
    }

    directory = CHUNK12.PACKAGES / NAME
    directory.mkdir(parents=True, exist_ok=True)
    CHUNK11.write_json(directory / 'definition-pack.json', definition_pack())
    outputs = CHUNK12.write_package(NAME, document)
    # write_package rebuilds the archive, so the pack has to be in place before it runs. Written
    # again here because write_package created the directory contents from scratch.
    CHUNK11.write_json(directory / 'definition-pack.json', definition_pack())
    outputs += CHUNK12.write_package(NAME, document)

    target = DOCUMENTS / (NAME + '.json')
    CHUNK11.write_json(target, {'dashboard': document, 'signals': CHUNK12.signals(),
                                'manifest': manifest, 'definition-pack': definition_pack()})
    outputs.append(target)
    print('Generated ' + NAME + ' with a definition pack')
    return outputs


if __name__ == '__main__':
    generate()
