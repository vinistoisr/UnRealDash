"""Measure the control example by declared regions, never by a special colour palette."""
import argparse
from collections import Counter
import json
from pathlib import Path
import sys
import zipfile

VALUE_TYPES = ('analog_dial', 'bar_gauge', 'history_graph', 'readout')


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def check_document(source, corpus, package):
    bundle = {name: read_json(source / (name + '.json'))
              for name in ('dashboard', 'signals', 'manifest')}
    errors = []
    if bundle != read_json(corpus):
        errors.append('source and corpus bundle disagree')
    with zipfile.ZipFile(package) as archive:
        for name, value in bundle.items():
            if json.loads(archive.read(name + '.json')) != value:
                errors.append('source and package disagree: ' + name)
        for name in bundle['manifest']['assets']:
            if archive.read(name) != (source / name).read_bytes():
                errors.append('source and package asset disagree: ' + name)
    document = bundle['dashboard']
    if document['reference_viewport'] != {'width': 1280, 'height': 720}:
        errors.append('reference viewport must be 1280x720')
    components, bindings = document['components'], document['bindings']
    declared = {signal['id'] for signal in bundle['signals']['signals']}
    counts = Counter()
    for name, binding in bindings.items():
        if binding['signal'] not in declared:
            errors.append(name + ': undeclared signal ' + binding['signal'])
        if name not in components:
            errors.append(name + ': binding has no component')
        elif binding['property'] == 'value':
            counts[components[name]['type']] += 1
    for name, entry in components.items():
        kind, properties = entry['type'], entry['properties']
        if kind in ('analog_dial', 'bar_gauge'):
            if not {'minimum', 'maximum'} <= properties.keys():
                errors.append(name + ': missing range')
            elif properties['minimum'] >= properties['maximum']:
                errors.append(name + ': range must increase')
        if kind == 'history_graph' and {'minimum', 'maximum'} & properties.keys():
            errors.append(name + ': history range is forbidden')
    for kind, minimum in [('analog_dial', 2), ('bar_gauge', 3), ('history_graph', 1),
                          ('readout', 4), ('indicator', 4)]:
        print(f'bound {kind}={counts[kind]} minimum={minimum}')
        if counts[kind] < minimum:
            errors.append(kind + ': census below minimum')
    instruments = sum(counts[kind] for kind in VALUE_TYPES)
    print(f'value instruments={instruments}; indicators={counts["indicator"]}')
    if instruments < 12:
        errors.append('fewer than 12 value instruments, excluding indicators')
    return errors


def regions(document, kinds):
    """Reject unsupported layouts rather than silently measure the wrong pixels after an edit."""
    components = document['components']
    result, errors = {}, []
    root = components.get('root', {})
    if (root.get('type') != 'container' or root.get('parent') is not None or
            root.get('anchor') != 'top_left' or
            root.get('rect') != {'x': 0, 'y': 0, 'width': 1280, 'height': 720}):
        return {}, ['region measurement requires the full-viewport root']
    for name in document['bindings']:
        entry = components[name]
        if entry['type'] not in kinds:
            continue
        if entry.get('parent') != 'root' or entry['anchor'] != 'top_left':
            errors.append(name + ': region measurement requires a top-left child of root')
            continue
        rect = entry['rect']
        x, y, width, height = (rect[key] for key in ('x', 'y', 'width', 'height'))
        if (any(int(v) != v for v in (x, y, width, height)) or
                x < 0 or y < 0 or width <= 0 or height <= 0 or
                x + width > 1280 or y + height > 720):
            errors.append(name + ': rect must occupy whole pixels inside the viewport')
            continue
        result[name] = tuple(int(v) for v in (x, y, x + width, y + height))
    if not result:
        errors.append('no bound regions to measure')
    return result, errors


def check_capture(document, first_path, second_path, mode):
    from PIL import Image, ImageChops

    def load(path):
        with Image.open(path) as image:
            return image.convert('RGB')

    first = load(first_path)
    if first.size != (1280, 720):
        return [f'capture dimensions {first.size}, expected 1280x720']
    if all(low == high for low, high in first.getextrema()):
        return ['capture is blank or a single flat colour']
    print('capture: 1280x720, non-flat RGB')
    if mode == 'capture':
        return []
    second = load(second_path)
    if second.size != first.size:
        return [f'second capture dimensions {second.size}, expected 1280x720']
    rects, errors = regions(document, ('indicator',) if mode == 'indicators' else VALUE_TYPES)
    for name, rect in rects.items():
        difference = ImageChops.difference(first.crop(rect), second.crop(rect))
        red, green, blue = difference.split()
        delta = ImageChops.lighter(ImageChops.lighter(red, green), blue)
        # The existing comparator's noise floor prevents rounding from standing in for motion.
        moved = sum(delta.histogram()[9:])
        print(f'{mode}: {name} rect={rect} changed_pixels={moved} worst={delta.getextrema()[1]}')
        if not moved:
            errors.append(name + ': no pixel changed above the RGB noise floor of 8')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('document', 'capture', 'instruments', 'indicators'))
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--corpus', type=Path)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--first', type=Path)
    parser.add_argument('--second', type=Path)
    args = parser.parse_args()
    try:
        if args.mode == 'document':
            if args.corpus is None or args.package is None:
                return ['document mode requires --corpus and --package']
            return check_document(args.source, args.corpus, args.package)
        if args.first is None or (args.mode != 'capture' and args.second is None):
            return ['capture mode requires --first; comparison also requires --second']
        return check_capture(read_json(args.source / 'dashboard.json'),
                             args.first, args.second, args.mode)
    except (OSError, ValueError, KeyError, TypeError, ImportError, zipfile.BadZipFile) as error:
        return [str(error)]


if __name__ == '__main__':
    failures = main()
    for failure in failures:
        print('FAIL ' + failure, file=sys.stderr)
    sys.exit(1 if failures else 0)
