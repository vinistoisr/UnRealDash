from fixture_support import DOCUMENTS, LIMITS
from dashboard_spec.report import corpus
from dashboard_spec.loader import load


def test_corpus():
    rows, mismatches, valid, invalid = corpus(DOCUMENTS)
    assert mismatches == []
    assert valid >= 20
    assert invalid >= 30
    assert all(len(row.split('\t')) == 4 for row in rows)
    assert rows == sorted(rows)


def test_generated_nesting_is_exact():
    value = load(DOCUMENTS / 'valid/generated-nesting-at-limit.json')
    def depth(node):
        if isinstance(node, dict):
            return 1 + max((depth(x) for x in node.values()), default=0)
        if isinstance(node, list):
            return 1 + max((depth(x) for x in node), default=0)
        return 0
    assert depth(value) == LIMITS['json_depth']
