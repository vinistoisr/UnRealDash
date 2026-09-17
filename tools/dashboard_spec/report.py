"""Fixture expectations and the four-field parity protocol."""
import json
from pathlib import Path
from .loader import load
from .schema import validate
from .errors import Failure


def field(value):
    """Keep control characters and trailing spaces from changing TSV framing."""
    return ''.join(f'\\u{ord(c):04x}' if ord(c) <= 32 or c == '\\' else c for c in value)


def check(path):
    try:
        validate(load(path))
    except Failure as error:
        return error
    except OSError as error:
        return Failure('E_JSON_SYNTAX', '', str(error))
    return None


def corpus(path):
    path = Path(path)
    rows, mismatches = [], []
    valid = invalid = 0
    for file in sorted([*path.glob('valid/*.json'), *path.glob('invalid/*.json')], key=lambda p: p.relative_to(path).as_posix()):
        error = check(file)
        relative = file.relative_to(path).as_posix()
        rows.append(f'{field(relative)}\t' + (f'FAIL\t{error.code}\t{field(error.pointer)}' if error else 'PASS\t\t'))
        if file.parent.name == 'valid':
            valid += 1
            if error:
                mismatches.append(f'{relative}: {error}')
        else:
            invalid += 1
            expected = file.with_suffix('.expected')
            lines = expected.read_text(encoding='utf-8').splitlines() if expected.exists() else []
            if len(lines) != 2 or not error or [error.code, error.pointer] != lines:
                mismatches.append(f'{relative}: expected {lines}, got {error}')
    return rows, mismatches, valid, invalid


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description='Validate dashboard documents')
    parser.add_argument('path', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--profile', choices=('mobile', 'desktop'), default='desktop')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args(argv)
    if args.path.is_dir():
        rows, mismatches, valid, invalid = corpus(args.path)
        if args.report:
            args.report.write_text('\n'.join(rows) + '\n', encoding='utf-8', newline='\n')
        if args.json:
            print(json.dumps(dict(valid=valid, invalid=invalid, mismatches=mismatches)))
        else:
            print(f'valid={valid} invalid={invalid} mismatches={len(mismatches)}')
            for mismatch in mismatches:
                print(mismatch)
        return int(bool(mismatches) or valid == 0 or invalid == 0)
    error = check(args.path)
    row = f'{field(args.path.name)}\t' + (f'FAIL\t{error.code}\t{field(error.pointer)}' if error else 'PASS\t\t')
    if args.report:
        args.report.write_text(row + '\n', encoding='utf-8', newline='\n')
    if args.json:
        print(json.dumps(dict(status='FAIL' if error else 'PASS', code=error.code if error else '', pointer=error.pointer if error else '', message=error.message if error else '')))
    else:
        print(str(error) if error else 'PASS')
    return int(error is not None)
