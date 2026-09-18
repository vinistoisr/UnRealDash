"""Build the criterion 3 mutation of the chunk 11 gate fixture.

The mutation is named in docs/build/chunk-11-primitives.md so it is evaluable rather than a matter
of judgement: change the fill token of the readout panel from its theme value to #ff00ff, recapture
and compare against the same reference with the same comparator. It must fail, and `moved` must
exceed 0.01, that is more than ten times the pass threshold.

The pointer changed is /dashboard/components/gauge_fill/properties/colour/token, which becomes a
literal colour. The gauge fill is 320 by 180, so recolouring it moves 57,600 of a 921,600 pixel
frame: `moved` about 0.0625, six times the threshold rather than scraping past it. That margin is
the point. A mutation that only just crossed the line would prove the comparator is twitchy, not
that the gate works.

The mutation is written to a separate package rather than edited into the fixture in place, so an
interrupted gate run cannot leave a mutated fixture behind for the next run to pass against.
"""
import pathlib
import stat
import sys
import zipfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))

import importlib.util

SPEC = importlib.util.spec_from_file_location(
    'gen_chunk11_fixture',
    pathlib.Path(__file__).resolve().parents[1] / 'tools/gen-chunk11-fixture.py')
FIXTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURE)

MUTATED_COLOUR = '#ff00ff'


def main(argv):
    if len(argv) != 3:
        sys.stderr.write('usage: mutate-chunk11-fixture.py <output-directory> <output-archive>\n')
        return 2
    directory = pathlib.Path(argv[1])
    archive = pathlib.Path(argv[2])

    document = FIXTURE.dashboard()
    colour = document['components']['gauge_fill']['properties']['colour']
    if colour != {'token': 'panel_fill'}:
        # The mutation has to change the thing the spec names. If the fixture moved, say so rather
        # than mutating whatever happens to be there, which could recolour far fewer pixels and let
        # the gate pass.
        sys.stderr.write('gauge_fill colour is {0!r}, expected the panel_fill token. '
                         'The fixture changed; redo the chunk 11 arithmetic.\n'.format(colour))
        return 1
    document['components']['gauge_fill']['properties']['colour'] = MUTATED_COLOUR

    png = FIXTURE.badge_png()
    directory.mkdir(parents=True, exist_ok=True)
    FIXTURE.write_json(directory / 'dashboard.json', document)
    FIXTURE.write_json(directory / 'signals.json', FIXTURE.signals())
    FIXTURE.write_json(directory / 'manifest.json', FIXTURE.manifest(len(png)))
    asset = directory / 'assets/badge.png'
    asset.parent.mkdir(parents=True, exist_ok=True)
    asset.write_bytes(png)

    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(directory.rglob('*')):
            if path.is_file():
                info = zipfile.ZipInfo(path.relative_to(directory).as_posix(),
                                       date_time=(2020, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | 0o644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, path.read_bytes(), compresslevel=9)
    print('Mutated gauge_fill colour to ' + MUTATED_COLOUR + ' in ' + str(archive))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
