#!/usr/bin/env python3
"""이 저장소의 소스로 DLL을 빌드해 릴리스 압축 안의 파일과 바이트까지 비교합니다.

쓰는 법:
  python verify_build.py --zig <zig.exe 경로> --zip <릴리스 압축 경로>

빌드 결과는 임시 폴더에만 만들고 끝나면 지웁니다. 게임 폴더는 건드리지 않습니다.
시험 프로그램은 실행하지 않습니다. 필요한 것: Python 3, Zig 0.14.1.
"""
import argparse
import hashlib
import os
import sys
import tempfile
import zipfile
from pathlib import Path

MODS = [('금란선옥모드', 'LOH_GoldenOrchidJade', 'jade.c'),
        ('선물선호표시모드', 'LOH_GiftHighlight', 'gift.c')]
ENTRIES = ['version.dll', 'hid.dll', 'winhttp.dll', 'winmm.dll']


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def read_shipped(zip_path, module, password):
    """압축 안에서 모드 DLL과 입구 파일을 찾습니다. 같은 이름의 내용이 다르면 멈춥니다."""
    wanted = {f'/{module}/{module}.dll': f'{module}.dll'}
    wanted.update({f'/entry/{name}': name for name in ENTRIES})
    found = {}
    with zipfile.ZipFile(zip_path) as archive:
        for info in archive.infolist():
            path = '/' + info.filename.replace('\\', '/')
            for suffix, name in wanted.items():
                if path.endswith(suffix):
                    if info.flag_bits & 1 and not password:
                        raise SystemExit('압축에 비밀번호가 걸려 있습니다. --password 를 주세요.')
                    data = archive.read(info, pwd=password.encode() if password else None)
                    if name in found and found[name] != data:
                        raise SystemExit(f'압축 안에 내용이 다른 {name} 이 있습니다.')
                    found[name] = data
    missing = [name for name in wanted.values() if name not in found]
    if missing:
        raise SystemExit('압축에서 찾지 못한 파일: ' + ', '.join(missing))
    return found


def main():
    parser = argparse.ArgumentParser(description='소스로 빌드한 DLL과 릴리스 압축 안 DLL을 비교합니다.')
    parser.add_argument('--zig', required=True, help='zig.exe 경로 (Zig 0.14.1)')
    parser.add_argument('--zip', required=True, help='릴리스 압축 경로')
    parser.add_argument('--password', default='', help='압축 비밀번호(비밀번호 없는 GitHub 압축은 필요 없음)')
    args = parser.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')

    root = Path(__file__).resolve().parent
    mods = [m for m in MODS if (root / m[0] / 'src' / m[2]).is_file()]
    if len(mods) != 1:
        raise SystemExit('이 폴더에서 모드 소스를 찾지 못했습니다.')
    folder, module, source = mods[0]

    sys.dont_write_bytecode = True
    os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
    sys.path.insert(0, str(root / '모드공통'))
    import build_native
    build_native.ZIG = Path(args.zig).resolve()
    if not build_native.ZIG.is_file():
        raise SystemExit(f'zig 를 찾지 못했습니다: {build_native.ZIG}')

    shipped = read_shipped(args.zip, module, args.password)
    with tempfile.TemporaryDirectory(prefix='loh_verify_') as tmp:
        out = Path(tmp) / 'out'
        build_native.build(root / folder, module, source, (), output=out)
        pairs = [(f'{module}.dll', out / f'{module}.dll')] + [(n, out / 'entry' / n) for n in ENTRIES]
        same = True
        print()
        for name, path in pairs:
            built = path.read_bytes()
            match = built == shipped[name]
            same = same and match
            print(f"{'일치' if match else '다름'}  {name}")
            print(f'  빌드 {sha256(built)}')
            print(f'  압축 {sha256(shipped[name])}')
    print()
    print('결과: 모두 일치합니다.' if same else '결과: 다른 파일이 있습니다.')
    print('일치는 이 소스로 만든 파일이라는 뜻입니다. 소스 내용은 따로 확인해 주세요.')
    return 0 if same else 1


if __name__ == '__main__':
    sys.exit(main())
