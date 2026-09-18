"""Build bounded-reader cases from a declarative entry list, without extracting archives."""
import json
import shutil
import stat
import struct
import zipfile
from fixture_support import PACKAGES, DOCUMENTS, LIMITS, write_json


def generate():
    cases = json.loads((PACKAGES / 'cases.json').read_text(encoding='utf-8'))
    outputs = []
    for case in cases:
        if case.get('external'):
            # Built by its own generator, which pins geometry this one knows nothing about.
            # Rebuilding it from a declarative entry list would silently replace it with an
            # empty package and the gate that depends on it would pass against nothing.
            continue
        directory = PACKAGES / case['name']
        directory.mkdir(parents=True, exist_ok=True)
        if case.get('base'):
            source = PACKAGES / case['base']
            for path in sorted(source.rglob('*')):
                if path.is_file():
                    target = directory / path.relative_to(source)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(path, target); outputs.append(target)
        if case.get('document'):
            target = directory / 'dashboard.json'
            shutil.copyfile(DOCUMENTS / 'invalid' / case['document'], target); outputs.append(target)
        if case.get('asset_reference'):
            path = directory / 'dashboard.json'; d = json.loads(path.read_text())
            d['components']['image0']['properties']['asset'] = case['asset_reference']
            write_json(path, d); outputs.append(path)
        if case.get('remove_asset'):
            path = directory / 'manifest.json'; m = json.loads(path.read_text()); m['assets'] = {}
            write_json(path, m); outputs.append(path)
        if case.get('metadata'):
            path = directory / 'manifest.json'; m = json.loads(path.read_text())
            m['assets']['assets/shared.png'].update(case['metadata'])
            write_json(path, m); outputs.append(path)
        if case.get('manifest_key') or case.get('manifest_aliases'):
            path = directory / 'manifest.json'; m = json.loads(path.read_text())
            metadata = m['assets']['assets/shared.png']
            if case.get('manifest_key'):
                del m['assets']['assets/shared.png']
                m['assets'][case['manifest_key']] = metadata
            for alias in case.get('manifest_aliases', []):
                m['assets'][alias] = dict(metadata)
            write_json(path, m); outputs.append(path)
        if case.get('supplementary'):
            path = directory / 'extra/dashboard.json'
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(directory / 'dashboard.json', path); outputs.append(path)
        if case.get('missing_file'):
            missing = directory / case['missing_file']
            missing.unlink()
            outputs = [path for path in outputs if path != missing]
        entries = []
        for entry in case.get('entries', []):
            repeat = entry.get('repeat', 1)
            for index in range(repeat):
                name = entry['name'].replace('{i}', str(index))
                size = entry.get('bytes', 0)
                if isinstance(size, str):
                    size = LIMITS[size] + entry.get('excess', 1)
                data = (entry.get('text', '').encode('utf-8') if 'text' in entry else b'\0' * size)
                entries.append((name, data, entry.get('symlink', False)))
                if not case['archive_only']:
                    path = directory / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data); outputs.append(path)
        archive = PACKAGES / (case['name'] + '.udash')
        with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            def add(name, data, symlink=False):
                info = zipfile.ZipInfo(name, date_time=(2020, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = ((stat.S_IFLNK if symlink else stat.S_IFREG) | 0o644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, data, compresslevel=9)
            for path in sorted(directory.rglob('*')):
                if path.is_file():
                    add(path.relative_to(directory).as_posix(), path.read_bytes())
            if case['archive_only']:
                for name, data, symlink in entries:
                    add(name, data, symlink)
        if case.get('malformed'):
            archive.write_bytes(b'PK\x03\x04truncated')
        if case.get('declared_entries'):
            count = case['declared_entries']
            record = struct.pack('<IQHHIIQQQQ', 0x06064b50, 44, 45, 45, 0, 0, count, count, 0, 0)
            locator = struct.pack('<IIQI', 0x07064b50, 0, 0, 1)
            eocd = struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, 65535, 65535, 0, 0, 0)
            archive.write_bytes(record + locator + eocd)
        if case.get('declared_directory_size'):
            archive.write_bytes(struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, 1, 1, LIMITS['expanded_size'] + 1, 0, 0))
        if case.get('zip64_small_legacy'):
            # No legacy sentinels: miniz still honours the preceding locator.
            record = struct.pack('<IQHHIIQQQQ', 0x06064b50, 44, 45, 45, 0, 0,
                                 100000, 100000, LIMITS['expanded_size'] + 1, 0)
            offset = 1000 if case.get('zip64_outside_file') else 0
            locator = struct.pack('<IIQI', 0x07064b50, 0, offset, 1)
            eocd = struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, 1, 1, 46, 0, 0)
            archive.write_bytes(record + locator + eocd)
        if case.get('central_patch'):
            patch = case['central_patch']
            with zipfile.ZipFile(archive) as z:
                offset = z.start_dir
            data = bytearray(archive.read_bytes())
            while data[offset:offset + 4] == b'PK\x01\x02':
                names, extra, comment = struct.unpack_from('<HHH', data, offset + 28)
                name = data[offset + 46:offset + 46 + names].decode('utf-8')
                if name == patch['name']:
                    if 'uncompressed_size' in patch:
                        struct.pack_into('<I', data, offset + 24, patch['uncompressed_size'])
                    if 'external_attr' in patch:
                        struct.pack_into('<H', data, offset + 4, 20)
                        struct.pack_into('<I', data, offset + 38, patch['external_attr'])
                    break
                offset += 46 + names + extra + comment
            else:
                raise ValueError('central patch entry not found')
            archive.write_bytes(data)
        outputs.append(archive)
    ignored = ['*.udash'] + [case['name'] + '/' for case in cases]
    (PACKAGES / '.gitignore').write_text('\n'.join(ignored) + '\n', encoding='utf-8', newline='\n')
    print(f'Generated {len(cases)} package archives')
    return outputs


if __name__ == '__main__':
    generate()
