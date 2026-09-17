"""Draft-7 validation with deterministic leaf error pointers."""
import json
from pathlib import Path

from jsonschema import Draft7Validator

from .errors import Failure
from .pointers import join

NAMES = ('dashboard', 'definition-pack', 'manifest', 'showcase', 'signals')


def schema(name):
    path = Path(__file__).resolve().parents[2] / 'packages/dashboard-spec/schema'
    return json.loads((path / f'{name}.schema.json').read_text(encoding='utf-8'))


def documents(value):
    if isinstance(value, dict):
        if 'reference_viewport' in value:
            return [('dashboard', value, '')]
        if 'schema_version' in value:
            return [('manifest', value, '')]
        if 'frames' in value:
            return [('definition-pack', value, '')]
        if 'material_scalars' in value:
            return [('showcase', value, '')]
        if 'signals' in value and isinstance(value['signals'], list):
            return [('signals', value, '')]
        if value and all(key in NAMES for key in value):
            return [(key, value[key], join('', key)) for key in sorted(value)]
    raise Failure('E_SCHEMA', '', 'unrecognized document or document set')


def validate(value):
    docs = documents(value)
    for name, document, prefix in docs:
        errors = list(Draft7Validator(schema(name)).iter_errors(document))
        if errors:
            # Both implementations report the lexically first offending property.
            failures = []
            def collect(error):
                if error.context:
                    for child in error.context:
                        collect(child)
                    return
                pointer = prefix
                for part in error.absolute_path:
                    pointer = join(pointer, part)
                if error.schema is False and list(error.absolute_schema_path)[-1:] == ['properties']:
                    pointer = join(pointer, 'type')
                if error.validator == 'required':
                    missing = sorted(set(error.validator_value) - set(error.instance))
                    pointer = join(pointer, missing[0])
                elif error.validator == 'additionalProperties':
                    import re
                    extra = sorted(k for k in error.instance
                                   if k not in error.schema.get('properties', {})
                                   and not any(re.search(p, k) for p in error.schema.get('patternProperties', {})))
                    if extra:
                        pointer = join(pointer, extra[0])
                failures.append((pointer, error.message))
            for error in errors:
                collect(error)
            pointer, message = min(failures)
            raise Failure('E_SCHEMA', pointer, message)
    from .semantic import semantic
    semantic(docs)
    return value
