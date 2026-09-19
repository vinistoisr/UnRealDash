"""Author the PLAN 4.10 control dashboard and its package and corpus forms.

The source JSON is indented because PLAN 4.11 edits it without rebuilding the player.
Colours belong to the dashboard, not to a measurement palette; the gate uses declared rects.
"""
import importlib.util
import json
import math
import pathlib
import stat
import struct
import sys
import zipfile
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'examples/control-dashboard'
NAME = 'control-dashboard'
SPEC = importlib.util.spec_from_file_location('chunk11', ROOT / 'tools/gen-chunk11-fixture.py')
CHUNK11 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHUNK11)
component = CHUNK11.component
DAY = {
    'background': '#101820', 'panel': '#192630', 'track': '#344650',
    'text': '#eef3f4', 'muted': '#a3b5bf', 'speed': '#64c7ce',
    'engine': '#efb66c', 'healthy': '#7dc59b',
    'state_stale': '#d99948', 'state_unavailable': '#77818c',
    'state_invalid': '#db6870', 'state_age_unknown': '#84a9dc',
}
NIGHT = {key: '#' + ''.join(f'{int(int(value[i:i + 2], 16) * 0.6):02x}'
                           for i in (1, 3, 5)) for key, value in DAY.items()}
MISSING = CHUNK11.missing(('dash', 'state_stale'), ('hidden', 'state_unavailable'),
                          ('icon', 'state_invalid'), ('icon', 'state_age_unknown'))


def dashboard():
    components = {'root': component('container', (0, 0, 1280, 720), {}, z_order=0)}
    bindings = {}

    def add(name, kind, rect, properties, layer=2, signal=None, precision='.1f'):
        components[name] = component(kind, rect, properties, parent='root', z_order=layer,
                                     missing_data=MISSING if signal else None)
        if signal:
            bindings[name] = {'property': 'value', 'signal': signal}
            if kind == 'readout':
                bindings[name]['format'] = precision

    def box(name, rect, token, layer=0):
        add(name, 'shape', rect, {'kind': 'rectangle', 'colour': {'token': token}}, layer)

    def label(name, text, rect, token='muted'):
        add(name, 'readout', rect, {'text': text, 'colour': {'token': token}}, 3)

    box('background', (0, 0, 1280, 720), 'background', -10)
    label('title', 'CONTROL / TOURING', (24, 12, 360, 80), 'text')
    for index, (signal, title) in enumerate([
            ('ignition_on', 'IGNITION'), ('abs_ready', 'ABS'),
            ('traction_ready', 'TRACTION'), ('cruise_enabled', 'CRUISE')]):
        x = 632 + index * 156
        add(signal + '_lamp', 'indicator', (x, 20, 20, 48),
            {'colour': {'token': 'healthy'}}, signal=signal)
        label(signal + '_label', title, (x + 24, 16, 128, 72))

    for index, (name, signal, title, maximum, token) in enumerate([
            ('speed', 'vehicle_speed', 'ROAD SPEED / km/h', 240, 'speed'),
            ('engine', 'engine_speed', 'ENGINE / rpm', 8000, 'engine')]):
        x = 24 + index * 304
        box(name + '_panel', (x, 92, 288, 436), 'panel')
        label(name + '_title', title, (x + 4, 100, 280, 80))
        # Shared artwork stays behind the needles even after the parser sorts component ids.
        add(name + '_face', 'image', (x + 24, 164, 240, 240),
            {'asset': 'assets/dial-face.png'}, 1)
        add(name + '_dial', 'analog_dial', (x + 24, 164, 240, 240),
            {'minimum': 0, 'maximum': maximum, 'colour': {'token': token}}, signal=signal)
        label(name + '_range', '0                 ' + str(maximum), (x + 20, 426, 248, 80))

    box('systems_panel', (632, 92, 624, 436), 'panel')
    for index, (name, title, low, high, token) in enumerate([
            ('coolant_temperature', 'COOLANT / degC     40 - 120', 40, 120, 'speed'),
            ('oil_pressure', 'OIL PRESSURE / bar     0 - 8', 0, 8, 'engine'),
            ('fuel_level', 'FUEL / fraction     0 - 1', 0, 1, 'healthy')]):
        y = 104 + index * 86
        label(name + '_label', title, (656, y, 576, 72))
        box(name + '_track', (656, y + 44, 576, 24), 'track', 1)
        add(name + '_bar', 'bar_gauge', (656, y + 44, 576, 24),
            {'minimum': low, 'maximum': high, 'circular': False,
             'colour': {'token': token}}, signal=name)

    label('history_label', 'ENGINE TREND / auto scale', (656, 360, 576, 72))
    add('engine_history', 'history_graph', (656, 404, 576, 100),
        {'history_samples': 128, 'colour': {'token': 'engine'}}, signal='engine_speed')

    # Numeric duplicates are intentional: a needle gives trend, while a readout gives precision.
    # Height includes the runtime's fixed 48 px state band, even when the reading is valid.
    for index, (signal, title, precision) in enumerate([
            ('vehicle_speed', 'km/h', '.0f'), ('engine_speed', 'rpm', '.0f'),
            ('coolant_temperature', 'coolant / degC', '.1f'),
            ('oil_pressure', 'oil / bar', '.2f'), ('fuel_level', 'fuel / fraction', '.2f'),
            ('engine_load', 'load / fraction', '.2f')]):
        x = 24 + index * 208
        name = 'value_' + str(index + 1)
        box(name + '_panel', (x, 544, 192, 160), 'panel')
        label(name + '_label', title, (x + 4, 552, 184, 72))
        add(name, 'readout', (x + 4, 604, 184, 100),
            {'text': '0', 'colour': {'token': 'text'}}, signal=signal, precision=precision)

    return {'reference_viewport': {'width': 1280, 'height': 720},
            'components': components, 'theme': {'day': DAY, 'night': NIGHT},
            'bindings': bindings, 'rules': {}, 'pages': {}}


def signals():
    return {'signals': [
        {'id': name, 'type': 'boolean' if discrete else 'number', 'unit': unit,
         'freshness_deadline_ms': deadline, 'discrete': discrete}
        for name, unit, deadline, discrete in [
            ('vehicle_speed', 'km/h', 200, False), ('engine_speed', 'rpm', 200, False),
            ('coolant_temperature', 'degC', 1000, False), ('oil_pressure', 'bar', 250, False),
            ('fuel_level', 'dimensionless', 2000, False),
            ('engine_load', 'dimensionless', 250, False),
            ('ignition_on', 'dimensionless', 500, True), ('abs_ready', 'dimensionless', 500, True),
            ('traction_ready', 'dimensionless', 500, True),
            ('cruise_enabled', 'dimensionless', 500, True)]]}


def dial_face_png():
    """Bake the tick band into an asset so the runtime only draws the moving needle."""
    size = 240
    rows = bytearray()
    for y in range(size):
        rows.append(0)
        for x in range(size):
            dx, dy = x + 0.5 - size / 2, y + 0.5 - size / 2
            radius = math.hypot(dx, dy)
            angle = (math.degrees(math.atan2(dx, -dy)) - 225) % 360
            tick = round(angle / 22.5)
            on_tick = angle <= 270 and abs(angle - tick * 22.5) < 0.9
            marked = (108 <= radius <= 110 or radius <= 5 or
                      (on_tick and (94 if tick % 2 == 0 else 101) <= radius <= 110))
            colour = (163, 181, 191, 255) if marked else (25, 38, 48, 255)
            rows.extend(colour)

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data +
                struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))

    return (b'\x89PNG\r\n\x1a\n' +
            chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(bytes(rows), 9)) + chunk(b'IEND', b''))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n',
                    encoding='utf-8', newline='\n')


def write_package(paths):
    archive = ROOT / 'tests/fixtures/packages/control-dashboard.udash'
    archive.parent.mkdir(parents=True, exist_ok=True)
    # Explicit members exclude editor backups from an otherwise valid package.
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as target:
        for path in sorted(paths):
            info = zipfile.ZipInfo(path.relative_to(SOURCE).as_posix(), (2020, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            target.writestr(info, path.read_bytes(), compresslevel=9)
    return archive


def generate():
    try:
        png = dial_face_png()
        bundle = {'dashboard': dashboard(), 'signals': signals(), 'manifest': {
            'schema_version': 1, 'package_id': 'unrealdash.control-dashboard', 'revision': 1,
            'runtime_compatibility': 'stage0', 'assets': {'assets/dial-face.png': {
                'width': 240, 'height': 240, 'format': 'rgba8', 'bytes': len(png)}}}}
        paths = []
        for name, value in bundle.items():
            path = SOURCE / (name + '.json')
            write_json(path, value)
            paths.append(path)
        asset = SOURCE / 'assets/dial-face.png'
        asset.parent.mkdir(parents=True, exist_ok=True)
        asset.write_bytes(png)
        paths.append(asset)
        paths.append(write_package(paths))
        corpus = ROOT / 'tests/fixtures/documents/valid/control-dashboard.json'
        write_json(corpus, bundle)
        paths.append(corpus)
        return paths, None
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        return [], str(error)


if __name__ == '__main__':
    outputs, error = generate()
    if error:
        print('Generation failed: ' + error, file=sys.stderr)
        sys.exit(1)
    for output in outputs:
        print(output.relative_to(ROOT).as_posix())
