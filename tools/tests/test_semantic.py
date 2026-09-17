import json
import re
import pytest
from jsonschema import Draft7Validator
from fixture_support import ROOT, dashboard, rule, literal
from dashboard_spec.schema import NAMES, schema, validate
from dashboard_spec.errors import CODES, Failure, LIMITS


def test_vocabulary_and_limits_identical():
    header = (ROOT / 'runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Errors.h').read_text()
    assert {name: int(value) for name, value in re.findall(r'\b(E_[A-Z_]+)\s*=\s*(\d+)', header)} == CODES
    limits = (ROOT / 'runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Limits.h').read_text()
    assert dict((name, int(number)) for name, number in re.findall(r'size_t (\w+) = (\d+)', limits)) == LIMITS


@pytest.mark.parametrize('name', NAMES)
def test_schema_contract(name):
    value = schema(name)
    Draft7Validator.check_schema(value)
    def walk(node):
        if isinstance(node, dict):
            assert 'default' not in node
            if node.get('type') == 'object':
                assert node['additionalProperties'] is False
            for child in node.values():
                walk(child)
        elif isinstance(node, list):
            for child in node:
                walk(child)
    walk(value)


def test_multiplicative_dimensions_compose():
    d = dashboard()
    pressure = literal(1, 'Pa')
    d['rules']['r'] = rule({'op': 'equal', 'args': [
        {'op': 'divide', 'args': [pressure, pressure]}, literal()]})
    validate(d)
    d['rules']['r']['expression']['args'][1] = literal(1, 'K')
    with pytest.raises(Failure) as caught:
        validate(d)
    assert caught.value.code == 'E_RULE_UNIT_MISMATCH'


@pytest.mark.parametrize('field,value', [('transition_ms', -1), ('transition_ms', 10001), ('page', -1), ('page', 16)])
def test_showcase_ranges(field, value):
    d = dict(material_scalars={'glow': .5}, transition_ms=100, page=0, colours={})
    d[field] = value
    with pytest.raises(Failure) as caught:
        validate(d)
    assert caught.value.pointer == '/' + field


def test_new_component_enum_requires_a_property_schema():
    value = schema('dashboard')
    component_schema = next(iter(value['properties']['components']['patternProperties'].values()))
    component_schema['properties']['type']['enum'].append('future')
    document = dashboard()
    document['components']['root']['type'] = 'future'
    assert not Draft7Validator(value).is_valid(document)
