"""Generate only size-driven documents. Small authored cases stay in the corpus."""
from fixture_support import DOCUMENTS, LIMITS, dashboard, component, rule, chain, tree, literal, write_json


def generate():
    generated = []
    def emit(name, value, code=None, pointer='', raw=False):
        directory = DOCUMENTS / ('invalid' if code else 'valid')
        path = directory / (name + '.json')
        if raw:
            path.write_text(value, encoding='utf-8', newline='\n')
        else:
            write_json(path, value)
        generated.append(path)
        if code:
            expected = path.with_suffix('.expected')
            expected.write_text(code + '\n' + pointer + '\n', encoding='utf-8', newline='\n')
            generated.append(expected)
    depth = LIMITS['json_depth']
    emit('generated-too-deep', '[' * (depth + 1) + '0' + ']' * (depth + 1),
         'E_JSON_TOO_DEEP', '/0' * depth, True)
    emit('generated-too-many-nodes', '[' + ','.join('0' for _ in range(LIMITS['json_nodes'])) + ']',
         'E_JSON_TOO_MANY_NODES', '/' + str(LIMITS['json_nodes'] - 1), True)
    emit('generated-long-string', '"' + 'x' * (LIMITS['string_length'] + 1) + '"', 'E_JSON_STRING_TOO_LONG', '', True)
    emit('generated-large-document', ' ' * LIMITS['document_size'] + '{}', 'E_DOC_TOO_LARGE', '', True)
    for count in (LIMITS['components'], LIMITS['components'] + 1):
        d = dashboard()
        d['components'].update({f'c{i:04}': component(parent='root') for i in range(count - 1)})
        emit('generated-components-' + str(count), d, 'E_TOO_MANY_COMPONENTS' if count > LIMITS['components'] else None, '/components')
    d = dashboard(); d['components']['root'] = component('history_graph')
    d['components']['root']['properties']['history_samples'] = LIMITS['history_samples']
    emit('generated-history-at-limit', d)
    # 29 binary levels plus two unary levels place the deepest object at depth 64.
    expression = chain(3)
    for _ in range((LIMITS['json_depth'] - 6) // 2):
        expression = {'op': 'add', 'args': [expression, literal()]}
    d = dashboard(); d['rules']['r'] = rule(expression)
    emit('generated-nesting-at-limit', d)
    d = dashboard()
    full, remainder = divmod(LIMITS['expression_nodes'], LIMITS['rule_nodes'])
    for i in range(full):
        d['rules'][f'r{i:03}'] = rule(tree(LIMITS['rule_nodes']))
    d['rules'][f'r{full:03}'] = rule(tree(remainder))
    d['rules'][f'r{full+1:03}'] = rule(literal())
    emit('generated-expression-document-limit', d, 'E_EXPRESSION_TOO_MANY_NODES', f'/rules/r{full+1:03}/expression')
    invalid_names = [p.name for p in generated if p.parent.name == 'invalid']
    (DOCUMENTS / 'invalid/.gitignore').write_text('\n'.join(invalid_names) + '\n', encoding='utf-8', newline='\n')
    (DOCUMENTS / 'valid/.gitignore').write_text('\n'.join(p.name for p in generated if p.parent.name == 'valid') + '\n', encoding='utf-8', newline='\n')
    print(f'Generated {sum(p.suffix == ".json" for p in generated)} document fixtures')
    return generated


if __name__ == '__main__':
    generate()
