import importlib.util
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import xml.etree.ElementTree as ET
import pytest


def module():
    spec = importlib.util.spec_from_file_location('differential_test', Path(__file__).parents[1] / 'differential-check.py')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


@pytest.mark.parametrize('kind,width', [('uint8', 1), ('uint16', 2), ('uint32', 4)])
@pytest.mark.parametrize('signed', [False, True])
def test_boundary_record_set(kind, width, signed):
    pack = {'frames': [{'id': 0xC80, 'signals': [{'name': 'value', 'byte_offset': 2, 'type': kind, 'signed': signed, 'byte_order': 'little'}]}, {'id': 0xC82, 'signals': []}]}
    records = module().boundary_records(pack)
    values = set()
    for r in records:
        wire = bytes.fromhex(r['bytes'])
        assert len(wire) == 16 and wire[:4] == bytes.fromhex('44332211')
        assert int.from_bytes(wire[4:8], 'little') == r['frame_id']
        if r['frame_id'] == 0xC80:
            values.add(int.from_bytes(wire[10:10+width], 'little'))
    assert values == {0, (1 << (8*width))-1, (1 << (8*width-1))-1, 1 << (8*width-1)}
    assert {r['frame_id'] for r in records} == {0xC80, 0xC82}


def test_jsonl_codec():
    rows = [{'frame_id': 3200, 'bytes': '00'*16}, {'physical': -12.5, 'quality': 'valid'}]
    m = module()
    assert m.decode_jsonl(m.encode_jsonl(rows)) == rows
    assert m.encode_jsonl(rows).endswith('\n')
    with pytest.raises(json.JSONDecodeError):
        m.decode_jsonl('{bad}')


@pytest.mark.parametrize('change', ['none', 'value', 'missing', 'duplicate', 'coverage', 'status', 'missing_status'])
def test_comparison_detects_disagreement_and_coverage(monkeypatch, change):
    m = module()
    fid = 0xC82 if change in ('status', 'missing_status') else 0xC80
    root = ET.fromstring(f'<schema><frame id="{fid}"><value name="Value" offset="0" conversion="V*0.1"/></frame></schema>')
    schema = SimpleNamespace(frames={fid: ('value',)}, decode=lambda fid, payload: {'value': int.from_bytes(payload[:2], 'little') * 0.1})
    reference = SimpleNamespace(_TARGETID_NAMES={}, _DISPLAY_ONLY=set(), load=lambda path: schema)
    pack, _ = m.converter_module().convert(root, reference, 'test', '1')
    records = m.boundary_records(pack)
    rows = []
    for i, record in enumerate(records):
        raw = int.from_bytes(bytes.fromhex(record['bytes'])[8:10], 'little')
        rows.append(dict(frame_id=fid, field='value', raw=raw, physical=raw*0.1, unit='dimensionless', quality='valid', age_evidence='unknown' if fid == 0xC82 else 'measured', stream_offset=i*16))
    if change == 'value': rows[0]['physical'] = 123
    if change == 'missing': rows.pop()
    if change == 'duplicate': rows.append(rows[0])
    if change == 'coverage': pack['frames'][0]['signals'][0]['name'] = 'other'
    counters = {'frames_emitted': len(records), 'samples_published': len(records)}
    if change == 'missing_status':
        rows.clear()
    monkeypatch.setattr(m.subprocess, 'run', lambda *a, **kw: SimpleNamespace(returncode=0, stdout=m.encode_jsonl(rows), stderr=json.dumps(counters)))
    if change in ('duplicate', 'coverage'):
        with pytest.raises(ValueError):
            m.compare(pack, root, reference, 'unused.exe', 'unused.json', 'unused.xml')
    else:
        report, mismatches, unexplained = m.compare(pack, root, reference, 'unused.exe', 'unused.json', 'unused.xml')
        assert mismatches == (len(records) if change == 'missing_status' else 0 if change in ('none', 'status') else 1)
        assert unexplained == mismatches
        assert ('MISMATCH:' in report) == (change not in ('none', 'status'))


def test_reference_comparison_optional():
    # Standalone opt-in below keeps the normal suite independent of owner inputs.
    pytest.skip('reference comparison requires --reference-decoder in this test file standalone invocation')


if __name__ == '__main__':
    # Accept the same explicit paths as the production command without adding a pytest plugin.
    if '--reference-decoder' not in sys.argv:
        raise SystemExit(pytest.main([__file__, '-q']))
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).parents[1] / 'differential-check.py'), *sys.argv[1:]]))
