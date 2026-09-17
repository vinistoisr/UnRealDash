import hashlib
import runpy
from fixture_support import ROOT


def test_generators_are_byte_deterministic():
    for name in ('image', 'document', 'package'):
        generate = runpy.run_path(str(ROOT / f'tools/gen-{name}-fixtures.py'))['generate']
        first = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in generate()}
        second = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in generate()}
        assert first == second
        assert first
