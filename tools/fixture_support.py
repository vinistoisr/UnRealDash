"""Synthetic documents shared by deterministic fixture generators."""
import json
from pathlib import Path
from dashboard_spec.errors import LIMITS

ROOT = Path(__file__).resolve().parents[1]
DOCUMENTS = ROOT / 'tests/fixtures/documents'
PACKAGES = ROOT / 'tests/fixtures/packages'


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, separators=(',', ':'), ensure_ascii=False) + '\n', encoding='utf-8', newline='\n')


def component(kind='container', parent=None):
    props = {
        'container': {}, 'readout': {'text': 'value'}, 'image': {'asset': 'assets/shared.png'},
        'shape': {'kind': 'line', 'colour': '#abcdef'},
        'analog_dial': {'minimum': 0, 'maximum': 100, 'colour': {'token': 'foreground'}},
        'bar_gauge': {'minimum': 0, 'maximum': 100, 'circular': True, 'colour': '#ffffff'},
        'indicator': {'colour': '#ffffff'},
        'history_graph': {'history_samples': 100, 'colour': '#ffffff'},
        'page_switch': {'pages': ['one', 'two'], 'initial_page': 'one'},
    }
    value = dict(type=kind, rect=dict(x=0, y=0, width=100, height=100), anchor='center',
                 scaling='uniform', clipping=True, aspect_policy='preserve',
                 missing_data={q: dict(presentation=p, token='foreground') for q, p in
                               [('stale', 'last_value_dimmed'), ('unavailable', 'dash'),
                                ('invalid', 'icon'), ('age_unknown', 'hidden')]}, properties=props[kind])
    if parent is not None:
        value['parent'] = parent
    return value


def dashboard():
    return dict(reference_viewport=dict(width=1280, height=720), components={'root': component()},
                theme={'day': {'foreground': '#ffffff'}, 'night': {'foreground': '#888888ff'}},
                bindings={}, rules={}, pages={})


def manifest():
    return dict(schema_version=1, package_id='synthetic.example', revision=1,
                runtime_compatibility='stage0', assets={'assets/shared.png': dict(width=8, height=8, format='rgba8', bytes=0)})


def signals():
    return {'signals': [dict(id='temperature', type='number', unit='K', freshness_deadline_ms=250, discrete=False),
                        dict(id='warning', type='boolean', unit='dimensionless', freshness_deadline_ms=100, discrete=True)]}


def literal(value=1, unit='dimensionless'):
    return dict(op='literal', value=value, unit=unit)


def rule(expression=None, policy='treat_unavailable'):
    r = dict(expression=expression or literal(), hysteresis=dict(value=0, unit='dimensionless'),
             debounce_ms=10, missing_input_policy=policy, targets=['root'])
    if policy == 'hold_last':
        r['maximum_hold_ms'] = 200
    return r


def chain(depth):
    value = literal()
    for _ in range(depth - 1):
        value = {'op': 'absolute_value', 'a': value}
    return value


def tree(nodes):
    if nodes == 1:
        return literal()
    if nodes % 2 == 0:
        return {'op': 'absolute_value', 'a': tree(nodes - 1)}
    left = (nodes - 1) // 2
    return {'op': 'add', 'args': [tree(left), tree(nodes - 1 - left)]}


def definition_pack():
    return dict(id='synthetic.example', version='0.1.0', api=1, input='binary-telemetry-v1', frames=[
        dict(id=1536, id_format='standard', length=8, role='telemetry', signals=[
            dict(name='temperature', byte_offset=0, type='uint16', byte_order='little', signed=True,
                 scale=0.1, offset=-40, unit='degC', acquisition='live', sentinels=[65535]),
            dict(name='pressure', byte_offset=2, type='uint16', byte_order='big', signed=False,
                 scale=1, offset=0, unit='kPa', acquisition='live')]),
        dict(id=1537, id_format='standard', length=1, role='status', signals=[
            dict(name='status', byte_offset=0, type='uint8', byte_order='little', signed=False,
                 scale=1, offset=0, unit='dimensionless', acquisition='held')])])
