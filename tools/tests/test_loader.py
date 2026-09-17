import pytest
from dashboard_spec.loader import loads
from dashboard_spec.errors import Failure, LIMITS


@pytest.mark.parametrize('text,code,pointer', [
    ('{"a/b":{"~x":1,"~x":2}}', 'E_DUPLICATE_KEY', '/a~1b/~0x'),
    ('{"x":', 'E_JSON_SYNTAX', ''), ('{}x', 'E_JSON_SYNTAX', ''),
    ('[NaN]', 'E_JSON_SYNTAX', ''), ('"\\ud800"', 'E_JSON_SYNTAX', ''),
    ('[1,]', 'E_JSON_SYNTAX', ''), ('{"a":1,}', 'E_JSON_SYNTAX', ''),
    ('{"a\\u0000b":1,"a\\u0000b":2}', 'E_DUPLICATE_KEY', '/a\0b'),
    ('{}\0x', 'E_JSON_SYNTAX', ''), ('1' + '0' * 309, 'E_JSON_SYNTAX', ''),
])
def test_rejections(text, code, pointer):
    with pytest.raises(Failure) as caught:
        loads(text)
    assert (caught.value.code, caught.value.pointer) == (code, pointer)


def test_container_depth_boundary():
    depth = LIMITS['json_depth']
    assert loads('[' * depth + '0' + ']' * depth)
    with pytest.raises(Failure) as caught:
        loads('[' * (depth + 1) + '0' + ']' * (depth + 1))
    assert caught.value.code == 'E_JSON_TOO_DEEP'


def test_utf8_string_bytes():
    value = 'é' * (LIMITS['string_length'] // 2)
    assert loads('"' + value + '"') == value
    with pytest.raises(Failure) as caught:
        loads('"' + value + 'é"')
    assert caught.value.code == 'E_JSON_STRING_TOO_LONG'


def test_report_framing():
    from dashboard_spec.report import field
    assert field('/a\t\n\0 \\') == '/a\\u0009\\u000a\\u0000\\u0020\\u005c'
