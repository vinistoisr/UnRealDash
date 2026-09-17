"""Write deterministic PNG headers and compressed scanlines with the standard library."""
import struct
import zlib
from fixture_support import PACKAGES, LIMITS, dashboard, component, manifest, write_json


def png(width, height, colour_type=6, depth=8):
    channels = {6: 4, 2: 3, 0: 1, 3: 1}[colour_type]
    def chunk(name, data):
        return struct.pack('>I', len(data)) + name + data + struct.pack('>I', zlib.crc32(name + data))
    compressor = zlib.compressobj(9)
    row = b'\0' * (1 + width * channels)
    blocks = [compressor.compress(row) for _ in range(height)]
    blocks.append(compressor.flush())
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, depth, colour_type, 0, 0, 0))
            + chunk(b'IDAT', b''.join(blocks)) + chunk(b'IEND', b''))


def generate():
    outputs = []
    specs = [('well-formed', 8, 8, 6, 1), ('over-wide-image', LIMITS['image_dimension'] + 1, 1, 6, 1),
             ('image-header-mismatch', 9, 8, 6, 1), ('image-format-unsupported', 8, 8, 3, 1),
             ('mobile-texture-budget', LIMITS['image_dimension'], LIMITS['image_dimension'], 6,
              LIMITS['mobile_texture'] // (LIMITS['image_dimension'] ** 2 * 4) + 1),
             ('desktop-texture-budget', LIMITS['image_dimension'], LIMITS['image_dimension'], 6,
              LIMITS['desktop_texture'] // (LIMITS['image_dimension'] ** 2 * 4) + 1),
             ('rgb-image', 8, 8, 2, 1), ('gray-image', 8, 8, 0, 1)]
    for case, width, height, colour, count in specs:
        directory = PACKAGES / case
        d, m = dashboard(), manifest(); m['assets'] = {}
        data = png(width, height, colour)
        for index in range(count):
            name = 'assets/shared.png' if index == 0 else f'assets/image{index}.png'
            path = directory / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data); outputs.append(path)
            m['assets'][name] = dict(width=8 if case == 'image-header-mismatch' else width, height=height,
                                   format={6: 'rgba8', 2: 'rgb8', 0: 'gray8', 3: 'rgba8'}[colour], bytes=len(data))
            d['components'][f'image{index}'] = component('image', 'root')
            d['components'][f'image{index}']['properties']['asset'] = name
        if case == 'well-formed':
            d['components']['shared-again'] = component('image', 'root')
        for name, value in [('dashboard', d), ('manifest', m)]:
            path = directory / (name + '.json'); write_json(path, value); outputs.append(path)
    print(f'Generated {sum(p.suffix == ".png" for p in outputs)} PNG fixtures')
    return outputs


if __name__ == '__main__':
    generate()
