"""Bound input before the standard decoder builds any objects."""
import json
import math
import sys
from pathlib import Path

from .errors import Failure, LIMITS
from .pointers import join


def loads(data):
    if isinstance(data, str):
        data = data.encode('utf-8')
    if len(data) > LIMITS['document_size']:
        raise Failure('E_DOC_TOO_LARGE', '', 'document byte limit exceeded')
    try:
        text = data.decode('utf-8')
    except UnicodeError as error:
        raise Failure('E_JSON_SYNTAX', '', str(error)) from None
    decoder = json.JSONDecoder()
    position = 0
    nodes = 0

    def whitespace():
        nonlocal position
        while position < len(text) and text[position] in ' \r\n\t':
            position += 1

    def fail(code, pointer, message):
        raise Failure(code, pointer, message)

    def string(pointer):
        nonlocal position
        if position >= len(text) or text[position] != '"':
            fail('E_JSON_SYNTAX', '', 'expected string')
        value, position = decoder.raw_decode(text, position)
        if len(value.encode('utf-8')) > LIMITS['string_length']:
            fail('E_JSON_STRING_TOO_LONG', pointer, 'string byte limit exceeded')
        return value

    def value(pointer, depth):
        nonlocal position, nodes
        whitespace()
        nodes += 1
        if nodes > LIMITS['json_nodes']:
            fail('E_JSON_TOO_MANY_NODES', pointer, 'JSON node limit exceeded')
        if position >= len(text):
            fail('E_JSON_SYNTAX', '', 'expected value')
        char = text[position]
        if char in '{[':
            if depth > LIMITS['json_depth']:
                fail('E_JSON_TOO_DEEP', pointer, 'JSON container depth exceeded')
            position += 1
            whitespace()
            closing = '}' if char == '{' else ']'
            seen = set()
            index = 0
            if position < len(text) and text[position] == closing:
                position += 1
                return
            while True:
                whitespace()
                if char == '{':
                    key = string(pointer)
                    child = join(pointer, key)
                    if key in seen:
                        fail('E_DUPLICATE_KEY', child, f'duplicate key {key}')
                    seen.add(key)
                    whitespace()
                    if position >= len(text) or text[position] != ':':
                        fail('E_JSON_SYNTAX', '', 'expected colon')
                    position += 1
                else:
                    child = join(pointer, index)
                value(child, depth + 1)
                index += 1
                whitespace()
                if position >= len(text):
                    fail('E_JSON_SYNTAX', '', 'unterminated container')
                if text[position] == closing:
                    position += 1
                    return
                if text[position] != ',':
                    fail('E_JSON_SYNTAX', '', 'expected comma')
                position += 1
        elif char == '"':
            string(pointer)
        else:
            scalar, position = decoder.raw_decode(text, position)
            if (isinstance(scalar, float) and not math.isfinite(scalar) or
                    isinstance(scalar, int) and abs(scalar) > sys.float_info.max):
                fail('E_JSON_SYNTAX', '', 'non-finite number')

    def pairs_hook(pairs):
        result = {}
        for key, item in pairs:
            if key in result:
                fail('E_DUPLICATE_KEY', join('', key), f'duplicate key {key}')
            result[key] = item
        return result

    try:
        value('', 1)
        whitespace()
        if position != len(text):
            fail('E_JSON_SYNTAX', '', 'trailing input')
        return json.loads(text, object_pairs_hook=pairs_hook)
    except (ValueError, UnicodeError, RecursionError) as error:
        raise Failure('E_JSON_SYNTAX', '', str(error)) from None


def load(path):
    with Path(path).open('rb') as stream:
        return loads(stream.read(LIMITS['document_size'] + 1))
