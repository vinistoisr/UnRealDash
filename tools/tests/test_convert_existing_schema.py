import importlib.util
from pathlib import Path
from types import SimpleNamespace
import xml.etree.ElementTree as ET
import pytest


def module():
    spec = importlib.util.spec_from_file_location("converter_test", Path(__file__).parents[1] / "convert-existing-schema.py")
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def convert(values='<value name="Test Value" offset="0"/>', attributes='', reference=None):
    reference = reference or SimpleNamespace(_TARGETID_NAMES={1: "target_name"}, _DISPLAY_ONLY={0xC92})
    return module().convert(ET.fromstring(f'<schema><frame id="0xC80" {attributes}>{values}</frame></schema>'), reference, "test", "0.1.0")


def test_signed_default_and_unsigned_override():
    pack, _ = convert('<value name="Signed" offset="0"/><value name="Unsigned" offset="2" signed="false"/>', 'signed="true"')
    a, b = pack['frames'][0]['signals']
    assert a['signed'] and not b['signed']
    assert a['type'] == b['type'] == 'uint16'


@pytest.mark.parametrize('attribute,scale', [('', 1.0), ('conversion="V*0.1"', 0.1)])
def test_affine_conversion(attribute, scale):
    pack, _ = convert(f'<value name="Value" offset="0" {attribute}/>')
    assert pack['frames'][0]['signals'][0]['scale'] == scale
    assert pack['frames'][0]['signals'][0]['offset'] == 0


@pytest.mark.parametrize('expression', ['V+1', 'V/10', 'eval(V)', 'V*0', 'V*1e999'])
def test_unsupported_conversion(expression):
    with pytest.raises(ValueError, match='frame 0xC80 offset 0'):
        convert(f'<value name="Value" offset="0" conversion="{expression}"/>')


def test_skip_write_and_display_frames_and_values():
    root = ET.fromstring('<schema><frame id="0xC90" writeInterval="1"/><frame id="0xC92"/><frame id="0xC80"><value name="Hidden" offset="0" displayOnly="true"/></frame></schema>')
    pack, notes = module().convert(root, SimpleNamespace(_TARGETID_NAMES={}, _DISPLAY_ONLY={0xC92}), 'test', '1')
    assert len(pack['frames']) == 1 and not pack['frames'][0]['signals']
    assert 'skipped 0xC90: write-direction' in notes
    assert 'skipped 0xC92: display-only frame' in notes
    assert any(n.startswith('empty-frame') for n in notes)


@pytest.mark.parametrize('key', ['endianness', 'endianess'])
def test_endianness_spellings(key):
    assert convert(attributes=f'{key}="little"')[0]['frames']
    with pytest.raises(ValueError, match='must be little'):
        convert(attributes=f'{key}="big"')


def test_unknown_target_and_duplicate_names():
    with pytest.raises(ValueError, match='unknown targetId'):
        convert('<value targetId="9999" offset="0"/>')
    with pytest.raises(ValueError, match='duplicate'):
        convert('<value name="The Value" offset="0"/><value name="the_value" offset="2"/>')
    pack, _ = convert('<value name="Name Wins" targetId="9999" offset="0"/>')
    assert pack['frames'][0]['signals'][0]['name'] == 'name_wins'
    assert convert('<value targetId="1" offset="0"/>')[0]['frames'][0]['signals'][0]['name'] == 'target_name'


def test_deterministic_bytes_and_comment_stripping(tmp_path):
    path = tmp_path / 'schema.xml'
    path.write_text('<schema><!-- invalid -- comment --><frame id="0xC80"><value name="A" offset="0"/></frame></schema>', encoding='utf-8')
    m = module()
    reference = SimpleNamespace(_TARGETID_NAMES={}, _DISPLAY_ONLY=set())
    a = m.encode_pack(m.convert(m.read_xml(path), reference, 'test', '1')[0])
    b = m.encode_pack(m.convert(m.read_xml(path), reference, 'test', '1')[0])
    assert a == b and a.endswith(b'\n') and b'\r' not in a


def test_units_status_enum_timeout_and_width():
    root = ET.fromstring('<schema><frame id="0xC82" timeout="500"><value name="Status" offset="0" length="1" units="V" enum="0:ok"/></frame></schema>')
    pack, notes = module().convert(root, SimpleNamespace(_TARGETID_NAMES={}, _DISPLAY_ONLY=set()), 'test', '1')
    f = pack['frames'][0]
    assert f['role'] == 'status' and f['signals'][0]['acquisition'] == 'held'
    assert f['signals'][0]['type'] == 'uint8' and f['signals'][0]['unit'] == 'dimensionless'
    assert f['signals'][0]['sentinels'] == []
    assert 'timeout 0xC82: 500' in notes and 'unit-unrepresented status: V' in notes
    assert 'enum-awaiting-sentinel-decision status' in notes


def test_missing_reference_attribute_is_named(tmp_path):
    reference = tmp_path / 'reference.py'
    reference.write_text('_TARGETID_NAMES = {}\n', encoding='utf-8')
    with pytest.raises(ValueError, match='_DISPLAY_ONLY'):
        module().load_reference(reference.resolve())
    assert not (tmp_path / '__pycache__').exists()


@pytest.mark.parametrize('attributes', ['size="7"', 'endianness="little" endianess="big"'])
def test_invalid_frame_metadata(attributes):
    with pytest.raises(ValueError):
        convert(attributes=attributes)


def test_target_names_use_the_same_normalization_and_duplicate_check():
    reference = SimpleNamespace(_TARGETID_NAMES={1: "  Mixed Case / Value! "}, _DISPLAY_ONLY=set())
    pack, _ = convert('<value targetId="1" offset="0"/>', reference=reference)
    assert pack['frames'][0]['signals'][0]['name'] == 'mixed_case_value'
    with pytest.raises(ValueError, match='duplicate'):
        convert('<value targetId="1" offset="0"/><value name="mixed_case_value" offset="2"/>', reference=reference)


def test_report_is_path_free_and_reproducible(monkeypatch):
    monkeypatch.setenv('SOURCE_DATE_EPOCH', '0')
    m = module()
    pack, notes = convert()
    first = m.conversion_report(pack, notes)
    second = m.conversion_report(pack, notes)
    assert first == second
    assert "Source: the owner's schema XML (path withheld)." in first
    assert 'Conversion date (UTC): 1970-01-01.' in first
    assert 'C:' not in first and '\\' not in first
    monkeypatch.delenv('SOURCE_DATE_EPOCH')
    assert 'Conversion date (UTC): ' + str(m.datetime.now(m.timezone.utc).date()) in m.conversion_report(pack, notes)


@pytest.mark.parametrize('source,expected', [("\u00b0C", "degC"), ("C", "degC"), ("\u00b0F", "degF"), ("F", "degF")])
def test_temperature_unit_aliases(source, expected):
    pack, notes = convert(f'<value name="Temperature" offset="0" units="{source}"/>')
    assert pack['frames'][0]['signals'][0]['unit'] == expected
    assert not any(note.startswith('unit-unrepresented') for note in notes)
