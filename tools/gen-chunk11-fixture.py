"""Build the PLAN 4.5 gate fixture: one document exercising all six Stage 0 primitives.

Why this fixture exists, given that nine-primitives.json already covers every type: the screenshot
gate's thresholds only mean something in proportion to the thing being measured. A 1280x720 frame is
921,600 pixels, the comparator passes at `moved <= 0.001` (922 pixels), criterion 4 needs two states
to differ by more than 0.005 (4,608), and criterion 3 needs the mutation to move more than 0.01
(9,216). No existing fixture pins geometry big enough to clear those by a margin, so recolouring a
small readout could pass the very gate it exists to break.

This fixture pins it:

  * The readout panel is 320 by 180, which is 57,600 pixels. Recolouring its fill token moves all of
    them: `moved` about 0.0625, six times the mutation threshold rather than scraping past it.
  * The status band is the readout's full width by 48, which is 15,360 pixels, over three times the
    distinguishability threshold. The band, not glyph coverage, is what makes criterion 4 hold:
    glyph coverage is a font metric this repository cannot pin.

Changing either number invalidates the arithmetic in docs/build/chunk-11-primitives.md and in
scripts/run-capture-gate.ps1. Both say so.

Output: the document fixture for the validator corpus, and the package (unpacked and .udash) the
engine loads. Package fixtures are generated rather than committed, like every other case under
tests/fixtures/packages.
"""
import json
import pathlib
import struct
import stat
import zipfile
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
DOCUMENTS = ROOT / 'tests/fixtures/documents/valid'
PACKAGES = ROOT / 'tests/fixtures/packages'
NAME = 'all-primitives-stage0'
ELLIPSE = 'shape-ellipse-refused'

VIEWPORT = (1280, 720)
READOUT = (320, 180)          # 57,600 px of a 921,600 px frame
BADGE = 240                   # the image primitive's square

# Day tokens. Deliberately far apart in every channel, because the comparator counts a pixel as
# moved only past a per-channel delta of 8, and two state colours that differ by less than that
# would make criterion 4 unfalsifiable.
DAY = {
    'panel_fill': '#16202c',
    'value': '#f2f6fa',
    'rule': '#3d4a5a',
    'warn': '#ffb000',
    'state_stale': '#c8501e',
    'state_age_unknown': '#2f8fd0',
    'state_unavailable': '#6a6a6a',
    'state_invalid': '#b02030',
}
NIGHT = {
    'panel_fill': '#0a0f15',
    'value': '#9fb0c0',
    'rule': '#1e2732',
    'warn': '#a06f00',
    'state_stale': '#7a3212',
    'state_age_unknown': '#1d5a84',
    'state_unavailable': '#3c3c3c',
    'state_invalid': '#6e141f',
}


def missing(stale, unavailable, invalid, age_unknown):
    """The four states are all required by the schema on every component, including the ones that
    bind no signal. See chunk-11 revision 4 C1: a document cannot omit them, so their presence can
    never be an error and the primitives that bind nothing parse and ignore them."""
    return {
        'stale': {'presentation': stale[0], 'token': stale[1]},
        'unavailable': {'presentation': unavailable[0], 'token': unavailable[1]},
        'invalid': {'presentation': invalid[0], 'token': invalid[1]},
        'age_unknown': {'presentation': age_unknown[0], 'token': age_unknown[1]},
    }


INERT = missing(('hidden', 'state_stale'), ('hidden', 'state_unavailable'),
                ('hidden', 'state_invalid'), ('hidden', 'state_age_unknown'))


def component(kind, rect, properties, parent=None, anchor='top_left', clipping=True,
              aspect='preserve', scaling='uniform', missing_data=None):
    value = {
        'type': kind,
        'rect': {'x': rect[0], 'y': rect[1], 'width': rect[2], 'height': rect[3]},
        'anchor': anchor,
        'scaling': scaling,
        'clipping': clipping,
        'aspect_policy': aspect,
        'missing_data': missing_data or INERT,
        'properties': properties,
    }
    if parent is not None:
        value['parent'] = parent
    return value


def dashboard():
    components = {}
    components['root'] = component('container', (0, 0, VIEWPORT[0], VIEWPORT[1]), {})

    # The gauge: a container holding the fill and the readout at the same rect. Nesting rather than
    # using two siblings makes the draw order local and obvious; within a canvas, document order is
    # paint order, so the fill is declared first.
    components['gauge'] = component('container', (40, 40, READOUT[0], READOUT[1]), {}, parent='root')
    # The mutation target of criterion 3. Its pointer is
    # /dashboard/components/gauge_fill/properties/colour/token.
    components['gauge_fill'] = component('shape', (0, 0, READOUT[0], READOUT[1]),
                                         {'kind': 'rectangle', 'colour': {'token': 'panel_fill'}},
                                         parent='gauge')
    # The primitive criteria 3 and 4 are measured on. All four presentations appear, one per state,
    # so the gate exercises the whole matrix rather than the two states it captures.
    components['speed'] = component(
        'readout', (0, 0, READOUT[0], READOUT[1]), {'text': '88.4', 'colour': {'token': 'value'}},
        parent='gauge',
        missing_data=missing(('dash', 'state_stale'), ('hidden', 'state_unavailable'),
                             ('last_value_dimmed', 'state_invalid'), ('icon', 'state_age_unknown')))

    # The image primitive consumes artwork and is never tinted; see revision 4 C5. dash on an image
    # has no text to replace, so it hides the content and the band alone carries the state, which is
    # the rendering revision 4 C2 defines.
    components['badge'] = component(
        'image', (960, 40, BADGE, BADGE), {'asset': 'assets/badge.png'}, parent='root',
        missing_data=missing(('last_value_dimmed', 'state_stale'), ('hidden', 'state_unavailable'),
                             ('icon', 'state_invalid'), ('dash', 'state_age_unknown')))

    # A straight line, which is inside the shape primitive's rule: axis-aligned, one uniform fill.
    components['divider'] = component('shape', (40, 300, 1200, 4),
                                      {'kind': 'line', 'colour': {'token': 'rule'}}, parent='root')

    # A lamp, drawn off because no rule has been evaluated in this chunk. Off is visibly off rather
    # than absent, so the cluster does not read as a display that failed to draw.
    components['lamp'] = component(
        'indicator', (40, 340, 96, 96), {'colour': {'token': 'warn'}}, parent='root',
        missing_data=missing(('last_value_dimmed', 'state_stale'), ('hidden', 'state_unavailable'),
                             ('icon', 'state_invalid'), ('dash', 'state_age_unknown')))

    components['pages'] = component('page_switch', (40, 460, 1200, 220),
                                    {'pages': ['primary', 'secondary'], 'initial_page': 'primary'},
                                    parent='root')
    components['page_primary'] = component('readout', (0, 0, 600, 220),
                                           {'text': 'PRIMARY', 'colour': {'token': 'value'}},
                                           parent='pages')
    components['page_secondary'] = component('readout', (0, 0, 600, 220),
                                             {'text': 'SECONDARY', 'colour': {'token': 'value'}},
                                             parent='pages')

    return {
        'reference_viewport': {'width': VIEWPORT[0], 'height': VIEWPORT[1]},
        'components': components,
        'theme': {'day': DAY, 'night': NIGHT},
        # The binding carries the format, which is where a readout's precision lives; the component
        # has only text and colour. See revision 4 C4.
        'bindings': {'speed': {'property': 'value', 'signal': 'vehicle_speed', 'format': '.1f'}},
        'rules': {},
        'pages': {'primary': ['page_primary'], 'secondary': ['page_secondary']},
    }


def manifest(png_bytes):
    return {
        'schema_version': 1, 'package_id': 'unrealdash.chunk11.gate', 'revision': 1,
        'runtime_compatibility': 'stage0',
        'assets': {'assets/badge.png': {'width': 64, 'height': 64, 'format': 'rgba8',
                                        'bytes': png_bytes}},
    }


def signals():
    return {'signals': [
        {'id': 'vehicle_speed', 'type': 'number', 'unit': 'km/h',
         'freshness_deadline_ms': 500, 'discrete': False},
    ]}


def badge_png():
    """A 64 by 64 opaque RGBA PNG, written by hand so the fixture needs no image library.

    Deliberately plain. This chunk proves the primitives consume artwork correctly; authoring
    beautiful artwork is a non-goal, and a hand-written PNG is one fewer dependency in the gate.
    """
    size = 64
    rows = bytearray()
    for y in range(size):
        rows.append(0)  # filter type 0, once per scanline
        for x in range(size):
            # A plain two-tone square with a border, so a wrong aspect policy is visible as a
            # stretched border rather than as a uniform block nobody can read.
            edge = x < 4 or y < 4 or x >= size - 4 or y >= size - 4
            rows += bytes((0xF2, 0xF6, 0xFA, 0xFF) if edge else (0x24, 0x38, 0x4C, 0xFF))

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data
                + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))

    header = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)  # 8-bit RGBA
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header)
            + chunk(b'IDAT', zlib.compress(bytes(rows), 9)) + chunk(b'IEND', b''))


def ellipse_dashboard():
    """A document the schema accepts and this runtime refuses.

    `kind` may be line, rectangle or ellipse, but a curve that is not a corner radius is artwork and
    drawing it in code produces ornament a document cannot restyle. So the builder refuses it by
    name, and this is the only one of criterion 6's three error cases that reaches a builder at all:
    an unknown component type is stopped by schema validation and an unresolved theme token by the
    semantic pass, both before the package finishes loading.

    That makes this the fixture that proves a schema-legal document this runtime cannot honour gives
    a readable error naming its JSON pointer rather than a crash or a silent skip.
    """
    return {
        'reference_viewport': {'width': VIEWPORT[0], 'height': VIEWPORT[1]},
        'components': {
            'root': component('container', (0, 0, VIEWPORT[0], VIEWPORT[1]), {}),
            'curve': component('shape', (40, 40, 200, 200),
                               {'kind': 'ellipse', 'colour': {'token': 'panel_fill'}}, parent='root'),
        },
        'theme': {'day': DAY, 'night': NIGHT},
        'bindings': {},
        'rules': {},
        'pages': {},
    }


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, separators=(',', ':'), ensure_ascii=False) + '\n',
                    encoding='utf-8', newline='\n')


def write_package(name, document, assets=True):
    """Writes one package as an unpacked directory and as a .udash archive.

    Fixed timestamps and deflate level, matching gen-package-fixtures.py, so two runs produce
    byte-identical archives and a regenerated fixture is not a spurious diff.
    """
    outputs = []
    directory = PACKAGES / name
    directory.mkdir(parents=True, exist_ok=True)
    write_json(directory / 'dashboard.json', document)
    outputs.append(directory / 'dashboard.json')
    if assets:
        png = badge_png()
        asset = directory / 'assets/badge.png'
        asset.parent.mkdir(parents=True, exist_ok=True)
        asset.write_bytes(png)
        write_json(directory / 'manifest.json', manifest(len(png)))
        outputs += [asset, directory / 'manifest.json']
    else:
        write_json(directory / 'manifest.json', {
            'schema_version': 1, 'package_id': 'unrealdash.chunk11.' + name, 'revision': 1,
            'runtime_compatibility': 'stage0', 'assets': {}})
        outputs.append(directory / 'manifest.json')
    archive = PACKAGES / (name + '.udash')
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(directory.rglob('*')):
            if path.is_file():
                info = zipfile.ZipInfo(path.relative_to(directory).as_posix(),
                                       date_time=(2020, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | 0o644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, path.read_bytes(), compresslevel=9)
    outputs.append(archive)
    return outputs


def generate():
    outputs = []

    # The validator corpus takes the sidecar form, so the semantic pass can resolve the binding's
    # signal. test_corpus.cpp walks the directory, so there is no list to update.
    png = badge_png()
    document = DOCUMENTS / (NAME + '.json')
    # The manifest travels with the document as a sidecar section, the way shared-image.json does
    # it. Without one the semantic pass rejects the image reference: an asset with no declared
    # metadata is an unbudgeted decode, not a missing convenience.
    write_json(document, {'dashboard': dashboard(), 'signals': signals(), 'manifest': manifest(len(png))})
    outputs.append(document)

    directory = PACKAGES / NAME
    write_json(directory / 'dashboard.json', dashboard())
    write_json(directory / 'signals.json', signals())
    write_json(directory / 'manifest.json', manifest(len(png)))
    asset = directory / 'assets/badge.png'
    asset.parent.mkdir(parents=True, exist_ok=True)
    asset.write_bytes(png)
    outputs += [directory / 'dashboard.json', directory / 'signals.json',
                directory / 'manifest.json', asset]

    # Fixed timestamps and deflate level, matching gen-package-fixtures.py, so two runs of this
    # script produce byte-identical archives and a regenerated fixture is not a spurious diff.
    archive = PACKAGES / (NAME + '.udash')
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(directory.rglob('*')):
            if path.is_file():
                info = zipfile.ZipInfo(path.relative_to(directory).as_posix(),
                                       date_time=(2020, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | 0o644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, path.read_bytes(), compresslevel=9)
    outputs.append(archive)
    outputs += write_package(ELLIPSE, ellipse_dashboard(), assets=False)
    print('Generated ' + NAME + ' and ' + ELLIPSE + ': documents, package directories and archives')
    return outputs


if __name__ == '__main__':
    generate()
