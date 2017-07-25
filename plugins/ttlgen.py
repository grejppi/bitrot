#!/usr/bin/env python3

import ctypes
import glob
import os
import shutil
import sys

import os.path


def create_ttls():
    path = os.path.abspath(sys.argv[1])
    basename = sys.argv[2]

    handle = ctypes.CDLL(path)
    ttl_fn = handle.lv2_generate_ttl

    if not ttl_fn:
        print("Failed to find lv2_generate_ttl()")
        raise SystemExit(1)

    print("Generating ttl data for '{0}', basename: '{1}'".format(path, basename))
    ttl_fn(basename.encode())

    # Rename audio ports to match the original C version
    with open(basename + '.ttl', 'r+') as f:
        ttl = f.read()
        f.seek(0)
        f.truncate()
        ttl = (ttl.replace('lv2_audio_in_1', 'lin')
                  .replace('lv2_audio_in_2', 'rin')
                  .replace('lv2_audio_out_1', 'lout')
                  .replace('lv2_audio_out_2', 'rout'))
        f.write(ttl)

    os.rename('manifest.ttl', 'manifest.{0}.ttl'.format(basename))


def create_manifest():
    print("Creating manifest.ttl for real")
    ttls = []
    for file in glob.glob('manifest.*.ttl'):
        with open(file) as f:
            ttls.append(f.read())
    lines = [ttl.splitlines() for ttl in ttls]
    prefix = os.path.commonprefix(lines)
    prefix = '\n'.join(prefix)
    result = [prefix]
    for ttl in ttls:
        result.append(ttl[len(prefix):])
    with open('manifest.ttl', 'w') as f:
        f.write('\n'.join(result))
    print("Done! ({0})".format(os.path.abspath('manifest.ttl')))


if __name__ == '__main__':
    if len(sys.argv) == 1:
        create_manifest()
    elif len(sys.argv) == 3:
        create_ttls()
    else:
        raise SystemExit('wtf am i gonna do with {0}'.format(sys.argv))
