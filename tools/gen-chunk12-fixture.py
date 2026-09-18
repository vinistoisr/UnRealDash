"""Build the PLAN 4.6 gate fixture: the analog dial, bar gauge and history graph.

The gate is: render the same document at 1280x720 and at 1280x660, and the dial's measured width to
height ratio stays within 1 percent of 1.0 in both. That is a measurement, not a comparison, and its
tolerance only means something in proportion to the size of the thing measured.

The arithmetic, done rather than assumed
----------------------------------------
Under `scaling: uniform` the root maps through EStretch::ScaleToFit, so the whole document scales by
min(1280/1280, 660/720) = 11/12 = 0.916666 on BOTH axes, and every ratio is preserved. Under
`scaling: stretch` it maps through EStretch::Fill, so y alone scales by 11/12 and a square renders
at a ratio of 12/11 = 1.0909, which is 9.09 percent off.

A bounding box measured from a capture carries up to one pixel of quantisation error at each edge,
so width and height each carry up to two pixels. That sets the smallest defensible band:

    face  600 units -> 550 px at 660.  1 percent is 5.50 px.  Worst quantisation error 0.73 percent.
    face  480 units -> 440 px at 660.  1 percent is 4.40 px.  Worst quantisation error 0.91 percent.
    face  300 units -> 275 px at 660.  1 percent is 2.75 px.  Worst quantisation error 1.47 percent.

So a 300 unit face cannot carry a 1 percent band: quantisation alone could fail a correct render.
The face is 600 units, where the worst case is 0.73 percent, comfortably inside 1 percent, and a
stretch shows 9.09 percent, a margin of more than twelve times.

600 and its position are both divisible by 12, so the scaled edges land on whole pixels at 660 as
well as at 720 and the quantisation above is a bound rather than the usual case.

The needle is measured too, because the face is an `image` and proving the image primitive is not
the same as proving the dial primitive. Its bounding box at a 45 degree deflection is
(length + width) / sqrt(2) = (252 + 18) / 1.41421 = 190.9 units, so 175 px at 660, where 1 percent
is 1.75 px and quantisation alone reaches 2.31 percent. The needle therefore carries a 3 percent
band, stated here rather than borrowed from the face, and a stretch still shows 9.09 percent, a
margin of three times.

Why a rotated rectangle is a valid ratio measurement: the axis-aligned bounding box of a rectangle
of length L and width W rotated by theta is (L*|sin|+W*|cos|) by (L*|cos|+W*|sin|). At 45 degrees
both are (L+W)/sqrt(2), so the ratio is exactly 1.0 for any L and W, and a non-uniform scale on one
axis moves it by exactly the scale factor. The sweep starts at 225 degrees, which is a diagonal, so
the default deflection of 0 is already the measurement position.

Colours are unique per component so a measurement can only be of the thing it names, and every pair
differs by far more than the measuring tolerance of 8 per channel.
"""
import importlib.util
import json
import pathlib
import stat
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
PACKAGES = ROOT / 'tests/fixtures/packages'
DOCUMENTS = ROOT / 'tests/fixtures/documents/valid'
NAME = 'dials-stage0'
# The two halves of criterion 4's control, identical but for one field.
STRETCHED = 'dials-stage0-stretched'   # face aspect_policy stretch, must FAIL the band
PRESERVED = 'dials-stage0-preserved'   # face aspect_policy preserve, must PASS it
REFUSED = 'dial-stretch-refused'       # a dial under a stretch aspect policy, must not load

SPEC = importlib.util.spec_from_file_location('gen_chunk11_fixture', ROOT / 'tools/gen-chunk11-fixture.py')
CHUNK11 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHUNK11)

VIEWPORT = (1280, 720)
FACE = 600            # divisible by 12, so 11/12 of it is a whole number of pixels
FACE_AT = (120, 60)   # both divisible by 12, for the same reason

# Measured colours. Every pair differs by more than 8 in at least one channel by a wide margin, and
# none is close to the black background or to any other component.
DAY = {
    'face': '#2244aa',
    'needle': '#ff2200',
    'arc': '#00cc66',
    'bar': '#ffcc00',
    'trace': '#cc00ff',
    'state_stale': '#c8501e',
    'state_age_unknown': '#2f8fd0',
    'state_unavailable': '#6a6a6a',
    'state_invalid': '#b02030',
}
NIGHT = {key: '#' + ''.join('{0:02x}'.format(int(int(value[i:i + 2], 16) * 0.55))
                            for i in (1, 3, 5))
         for key, value in DAY.items()}

MEASURED = {'face': DAY['face'], 'needle': DAY['needle'], 'arc': DAY['arc'],
            'bar': DAY['bar'], 'trace': DAY['trace']}


def face_png():
    """A flat square in the face colour. Flat, because this is measured by colour and any gradient
    or border would put part of the face outside the tolerance and shrink the measured box."""
    red, green, blue = (int(DAY['face'][i:i + 2], 16) for i in (1, 3, 5))
    size = 60
    import struct
    import zlib
    rows = bytearray()
    for _ in range(size):
        rows.append(0)
        rows += bytes((red, green, blue, 0xFF)) * size

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data
                + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))

    header = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header)
            + chunk(b'IDAT', zlib.compress(bytes(rows), 9)) + chunk(b'IEND', b''))


def dashboard(face_policy='preserve', face_rect=None):
    component = CHUNK11.component
    components = {}
    components['root'] = component('container', (0, 0, VIEWPORT[0], VIEWPORT[1]), {})

    # The dial: a face image with a needle over it, which is how a dial is composed when no schema
    # field can name a face. Both occupy the same rect, so the needle pivots on the face centre.
    # z_order, not declaration order: the parser sorts every object's members by name so that schema
    # diagnostics do not depend on the author's ordering, which means a document's written order is
    # not recoverable at runtime and cannot carry stacking. Without these two numbers the needle is
    # painted under its own face, because "dial" sorts before "face".
    components['face'] = component('image', face_rect or (FACE_AT[0], FACE_AT[1], FACE, FACE),
                                   {'asset': 'assets/face.png'}, parent='root', z_order=0,
                                   aspect=face_policy)
    components['dial'] = component('analog_dial', (FACE_AT[0], FACE_AT[1], FACE, FACE),
                                   {'minimum': 0, 'maximum': 100, 'colour': {'token': 'needle'}},
                                   parent='root', z_order=1)

    components['arc'] = component('bar_gauge', (800, 60, 300, 300),
                                  {'minimum': 0, 'maximum': 100, 'circular': True,
                                   'colour': {'token': 'arc'}}, parent='root')
    components['bar'] = component('bar_gauge', (800, 400, 400, 60),
                                  {'minimum': 0, 'maximum': 100, 'circular': False,
                                   'colour': {'token': 'bar'}}, parent='root')
    components['graph'] = component('history_graph', (800, 500, 400, 160),
                                    {'history_samples': 128, 'colour': {'token': 'trace'}},
                                    parent='root')

    return {
        'reference_viewport': {'width': VIEWPORT[0], 'height': VIEWPORT[1]},
        'components': components,
        'theme': {'day': DAY, 'night': NIGHT},
        # Every gauge is bound, because -udash-fraction only supplies a value to bound signals and
        # chunk 11 made an unbound component present no state at all.
        'bindings': {
            'dial': {'property': 'value', 'signal': 'engine_speed'},
            'arc': {'property': 'value', 'signal': 'engine_speed'},
            'bar': {'property': 'value', 'signal': 'engine_speed'},
            'graph': {'property': 'value', 'signal': 'engine_speed'},
        },
        'rules': {},
        'pages': {},
    }


def refused_dashboard():
    """A dial under a directly declared stretch aspect policy.

    The schema accepts it: `aspect_policy` is an enum of preserve, stretch and inherit on every
    component. The semantic pass rejects it at SemanticPass.cpp:273 with E_ASPECT_POLICY_FORBIDDEN,
    because a circular gauge that is scaled non-uniformly is no longer circular.

    So this never reaches a builder, and `RefuseStretchedCircle` in the dial builder cannot fire
    through LoadPackage. It is kept there anyway, because a caller can build a component the
    validator never saw, and a silently distorted dial is the defect PLAN 4.6 exists to prevent.
    """
    component = CHUNK11.component
    return {
        'reference_viewport': {'width': VIEWPORT[0], 'height': VIEWPORT[1]},
        'components': {
            'root': component('container', (0, 0, VIEWPORT[0], VIEWPORT[1]), {}),
            'dial': component('analog_dial', (120, 60, FACE, FACE),
                              {'minimum': 0, 'maximum': 100, 'colour': {'token': 'needle'}},
                              parent='root', aspect='stretch'),
        },
        'theme': {'day': DAY, 'night': NIGHT},
        'bindings': {},
        'rules': {},
        'pages': {},
    }


def signals():
    return {'signals': [
        {'id': 'engine_speed', 'type': 'number', 'unit': 'rpm',
         'freshness_deadline_ms': 200, 'discrete': False},
    ]}


def write_package(name, document):
    outputs = []
    directory = PACKAGES / name
    directory.mkdir(parents=True, exist_ok=True)
    png = face_png()
    CHUNK11.write_json(directory / 'dashboard.json', document)
    CHUNK11.write_json(directory / 'signals.json', signals())
    CHUNK11.write_json(directory / 'manifest.json', {
        'schema_version': 1, 'package_id': 'unrealdash.chunk12.' + name, 'revision': 1,
        'runtime_compatibility': 'stage0',
        'assets': {'assets/face.png': {'width': 60, 'height': 60, 'format': 'rgba8',
                                       'bytes': len(png)}},
    })
    asset = directory / 'assets/face.png'
    asset.parent.mkdir(parents=True, exist_ok=True)
    asset.write_bytes(png)
    outputs += [directory / 'dashboard.json', directory / 'signals.json',
                directory / 'manifest.json', asset]

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
    png = face_png()
    manifest = {
        'schema_version': 1, 'package_id': 'unrealdash.chunk12.gate', 'revision': 1,
        'runtime_compatibility': 'stage0',
        'assets': {'assets/face.png': {'width': 60, 'height': 60, 'format': 'rgba8',
                                       'bytes': len(png)}},
    }
    document = DOCUMENTS / (NAME + '.json')
    CHUNK11.write_json(document, {'dashboard': dashboard(), 'signals': signals(),
                                  'manifest': manifest})

    outputs = [document]
    outputs += write_package(NAME, dashboard())

    # Criterion 4's control, in two halves that differ by exactly one field. The face sits in a
    # 600 by 400 rect instead of a square one:
    #
    #   stretch  -> the image fills the rect, 600 by 400, a ratio of 1.5, which is 50 percent off
    #               and must FAIL the 1 percent band.
    #   preserve -> the image is scaled uniformly inside the rect, 400 by 400, a ratio of 1.0,
    #               which must PASS it.
    #
    # Together they prove the measurement can fail, and that aspect_policy is what decides. The
    # semantic pass rejects `stretch` on an analog_dial and on a circular bar_gauge, but says
    # nothing about the image that forms a dial's face, so a document can legally stretch one. That
    # is the gap this control documents and the reason the gate measures at runtime rather than
    # trusting the validator.
    control_rect = (FACE_AT[0], FACE_AT[1], FACE, 400)
    outputs += write_package(STRETCHED, dashboard(face_policy='stretch', face_rect=control_rect))
    outputs += write_package(PRESERVED, dashboard(face_policy='preserve', face_rect=control_rect))
    outputs += write_package(REFUSED, refused_dashboard())
    print('Generated ' + NAME + ', ' + STRETCHED + ', ' + PRESERVED + ' and ' + REFUSED)
    return outputs


if __name__ == '__main__':
    generate()
