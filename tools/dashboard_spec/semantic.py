"""Shared ordered semantic checks. Keep SemanticPass.cpp in step."""
from .errors import Failure, LIMITS
from .pointers import join


def forbidden(path):
    return path.startswith('/') or '\\' in path or ':' in path or '..' in path.split('/')


def asset_key(path):
    return '/'.join(part for part in path.split('/') if part not in ('', '.'))


def dimension(unit):
    groups = (('K', 'degC', 'degF'), ('Pa', 'kPa', 'bar', 'psi'),
              ('m/s', 'km/h', 'mph'), ('rad/s', 'rpm'))
    return tuple(int(unit in group) for group in groups)


def semantic(documents):
    docs = {name: (value, pointer) for name, value, pointer in documents}
    def fail(code, pointer, message):
        raise Failure(code, pointer, message)
    signals = {s['id']: s for s in docs.get('signals', ({'signals': []}, ''))[0]['signals']}
    manifest, mp = docs.get('manifest', ({'assets': {}}, ''))
    for asset in sorted(manifest['assets']):
        if forbidden(asset):
            fail('E_ASSET_REFERENCE_FORBIDDEN', join(join(mp, 'assets'), asset), asset)
    if 'definition-pack' in docs:
        pack, pp = docs['definition-pack']
        for i, frame in enumerate(pack['frames']):
            for j, field in enumerate(frame['signals']):
                if field['byte_offset'] + {'uint8': 1, 'uint16': 2, 'uint32': 4}[field['type']] > frame['length']:
                    fail('E_FIELD_PAST_FRAME_LENGTH', f'{pp}/frames/{i}/signals/{j}/byte_offset', field['name'])
    if 'dashboard' not in docs:
        return
    dashboard, dp = docs['dashboard']
    components = dashboard['components']
    cp = join(dp, 'components')
    if len(components) > LIMITS['components']:
        fail('E_TOO_MANY_COMPONENTS', cp, 'component count exceeded')
    parents = {}
    for key in sorted(components):
        component = components[key]
        pointer = join(cp, key)
        parent = component.get('parent')
        parents[key] = parent
        if parent is not None and parent not in components:
            fail('E_UNRESOLVED_PARENT', join(pointer, 'parent'), parent)
    roots = sorted(k for k, p in parents.items() if p is None)
    # A disconnected cycle is an orphan subtree; a rootless cycle is a cycle.
    for key in sorted(components):
        seen = set()
        node = key
        while node is not None:
            if node in seen:
                code = 'E_ORPHAN_SUBTREE' if roots and key != parents[key] else 'E_COMPONENT_CYCLE'
                fail(code, join(join(cp, key), 'parent'), key)
            seen.add(node)
            node = parents[node]
    if not roots:
        fail('E_NO_ROOT', cp, 'hierarchy has no root')
    if len(roots) > 1:
        fail('E_MULTIPLE_ROOTS', cp, ', '.join(roots))
    for key in sorted(components):
        component = components[key]
        pointer = join(cp, key)
        policy = component['aspect_policy']
        parent = parents[key]
        while policy == 'inherit' and parent is not None:
            ancestor = components[parent]
            if ancestor['type'] == 'container':
                policy = ancestor['aspect_policy']
            parent = parents[parent]
        if (component['type'] == 'analog_dial' or
            component['type'] == 'bar_gauge' and component['properties']['circular']) and policy == 'stretch':
            fail('E_ASPECT_POLICY_FORBIDDEN', join(pointer, 'aspect_policy'), key)
        if component['type'] == 'history_graph' and component['properties']['history_samples'] > LIMITS['history_samples']:
            fail('E_TOO_MANY_HISTORY_SAMPLES', pointer + '/properties/history_samples', key)
        if component['type'] == 'image':
            asset = component['properties']['asset']
            ap = pointer + '/properties/asset'
            if forbidden(asset):
                fail('E_ASSET_REFERENCE_FORBIDDEN', ap, asset)
            if asset_key(asset) not in {asset_key(name) for name in manifest['assets']}:
                fail('E_IMAGE_NOT_IN_MANIFEST', ap, asset)
        if component['type'] == 'page_switch':
            for i, page in enumerate(component['properties']['pages']):
                if page not in dashboard['pages']:
                    fail('E_UNRESOLVED_PAGE', f'{pointer}/properties/pages/{i}', page)
            page = component['properties']['initial_page']
            if page not in dashboard['pages'] or page not in component['properties']['pages']:
                fail('E_UNRESOLVED_PAGE', pointer + '/properties/initial_page', page)
    def token(name, pointer, depth):
        if depth > LIMITS['json_depth']:
            fail('E_JSON_TOO_DEEP', pointer, 'theme reference depth exceeded')
        if not isinstance(name, str):
            fail('E_SCHEMA', pointer, 'theme reference must be a string')
        if any(name not in dashboard['theme'][mode] for mode in ('day', 'night')):
            fail('E_UNRESOLVED_THEME_TOKEN', pointer, name)

    def colour(value, pointer, depth):
        if depth > LIMITS['json_depth']:
            fail('E_JSON_TOO_DEEP', pointer, 'colour depth exceeded')
        if isinstance(value, dict):
            if 'token' not in value:
                fail('E_SCHEMA', pointer + '/token', 'colour needs a token')
            token(value['token'], pointer + '/token', depth + 1)

    depth = 4 if dp else 3
    for key in sorted(components):
        component = components[key]
        pointer = join(cp, key)
        for state in sorted(component['missing_data']):
            token(component['missing_data'][state]['token'], pointer + '/missing_data/' + state + '/token', depth + 3)
        if component['type'] in ('readout', 'shape', 'analog_dial', 'bar_gauge', 'indicator', 'history_graph'):
            if 'colour' in component['properties']:
                colour(component['properties']['colour'], pointer + '/properties/colour', depth + 2)
    for page in sorted(dashboard['pages']):
        for i, target in enumerate(dashboard['pages'][page]):
            if target not in components:
                fail('E_UNRESOLVED_PAGE', join(join(dp + '/pages', page), i), target)
    for key in sorted(dashboard['bindings']):
        binding = dashboard['bindings'][key]
        pointer = join(dp + '/bindings', key)
        if key not in components:
            fail('E_UNRESOLVED_COMPONENT', pointer, key)
        if binding['signal'] not in signals:
            fail('E_UNRESOLVED_SIGNAL', pointer + '/signal', binding['signal'])
    total = 0
    for key in sorted(dashboard['rules']):
        rule = dashboard['rules'][key]
        pointer = join(dp + '/rules', key)
        count = 0
        def expression(node, ep, depth):
            nonlocal count, total
            count += 1
            total += 1
            if depth > LIMITS['expression_depth']:
                fail('E_EXPRESSION_TOO_DEEP', ep, key)
            if count > LIMITS['rule_nodes'] or total > LIMITS['expression_nodes']:
                fail('E_EXPRESSION_TOO_MANY_NODES', ep, key)
            op = node['op']
            if op == 'literal':
                return dimension(node['unit'])
            if op == 'signal':
                if node['signal'] not in signals:
                    fail('E_UNRESOLVED_SIGNAL', ep + '/signal', node['signal'])
                return dimension(signals[node['signal']]['unit'])
            children = ([expression(node['a'], ep + '/a', depth + 1)] if 'a' in node else
                        [expression(child, f'{ep}/args/{i}', depth + 1) for i, child in enumerate(node['args'])])
            if op in ('multiply', 'divide'):
                return tuple(a + (b if op == 'multiply' else -b) for a, b in zip(*children))
            if op in ('logical_and', 'logical_or', 'logical_not'):
                if any(any(d) for d in children):
                    fail('E_RULE_UNIT_MISMATCH', ep, op)
            elif any(d != children[0] for d in children):
                fail('E_RULE_UNIT_MISMATCH', ep, op)
            if op in ('less', 'less_or_equal', 'greater', 'greater_or_equal', 'equal', 'not_equal', 'logical_and', 'logical_or', 'logical_not'):
                return (0, 0, 0, 0)
            return children[0]
        expression(rule['expression'], pointer + '/expression', 1)
        for i, target in enumerate(rule['targets']):
            if target not in components:
                fail('E_UNRESOLVED_COMPONENT', f'{pointer}/targets/{i}', target)
