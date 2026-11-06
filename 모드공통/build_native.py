"""Build independent DLLs using the same checked Windows/IL2CPP runtime."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

COMMON = Path(__file__).resolve().parent
WORKSPACE = COMMON.parent
NATIVE = COMMON / 'native'
ZIG = WORKSPACE / '.tools/zig-x86_64-windows-0.14.1/zig.exe'
CF = ['-target', 'x86_64-windows-msvc', '-nostdlib', '-nostdinc', '-O2',
      '-ffreestanding', '-fno-stack-protector', '-mno-stack-arg-probe',
      '-fno-builtin', '-Wall', '-Wextra', '-Wno-pointer-sign',
      '-Wno-misleading-indentation', '-Wno-unused-function', '-finput-charset=UTF-8']

def build(root, module, source, tests=(), output=None, manager=False):
    root = Path(root)
    out = Path(output) if output else root / 'build'
    out.mkdir(parents=True, exist_ok=True)
    temp_root = Path(os.environ.get('LOH_GIFT_BUILD_TEMP', out/'_tmp'))
    temp_root.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(temp_root / 'loh_social_zig'))
    with tempfile.TemporaryDirectory(prefix='loh_social_', dir=temp_root) as tmp:
        work = Path(tmp)
        def run(args, exe=ZIG):
            r = subprocess.run([str(exe), *map(str, args)], cwd=work, env=env,
                               capture_output=True, text=True, encoding='utf-8', errors='replace')
            if r.stdout or r.stderr: print(r.stdout + r.stderr)
            if r.returncode: raise SystemExit(r.returncode)
        for path in [*NATIVE.glob('*.h'), *NATIVE.glob('*.inc'), *NATIVE.glob('*.def'), *root.joinpath('src').glob('*')]:
            if path.is_file(): shutil.copy2(path, work/path.name)
        for name in ['kernel32', 'user32']:
            shutil.copy2(NATIVE/'lib'/f'{name}.def', work/f'{name}.def')
            run(['dlltool', '-m', 'i386:x86-64', '-d', f'{name}.def', '-l', f'{name}.lib'])
        flags = [*CF, '-I', '.'] + (['-DLOH_MOD_MANAGER=1'] if manager else [])
        run(['cc', *flags, '-c', source, '-o', 'mod.obj'])
        loader = (NATIVE/'loader.c').read_text('utf-8').replace('LOH_GiftHighlight', module)
        loader = loader.replace('선물 선호 표시', '금란선옥' if module == 'LOH_GoldenOrchidJade' else '선물·화제 취향 표시')
        (work/'loader.c').write_text(loader, 'utf-8')
        run(['cc', *flags, '-c', 'loader.c', '-o', 'loader.obj'])
        for test in tests:
            if test == 'test_hooks':
                orchid = (root/'src/orchid.inc').read_text('utf-8')
                hook = orchid[orchid.index('static int o_hook('):orchid.index('static int orchid_resolve(')]
                (work/'orchid_hook_under_test.inc').write_text(hook, 'utf-8')
            run(['cc', *flags, '-c', f'{test}.c', '-o', f'{test}.obj'])
            run(['lld-link', '/nodefaultlib', '/entry:Entry', '/subsystem:console',
                 f'/out:{test}.exe', f'{test}.obj', 'kernel32.lib'])
            subprocess.run([str(work/f'{test}.exe')], check=True)
            print('PASS:', module, test)
        run(['lld-link', '/dll', '/nodefaultlib', '/entry:DllMain', f'/out:{module}.dll',
             'mod.obj', 'kernel32.lib', 'user32.lib', '/Brepro'])
        run(['lld-link', '/nodefaultlib', '/entry:Entry', '/subsystem:console',
             f'/out:{module}Loader.exe', 'loader.obj', 'kernel32.lib', 'user32.lib', '/Brepro'])
        shutil.copy2(NATIVE/'entry.c', work/'entry.c')
        run(['cc', *flags, '-c', 'entry.c', '-o', 'entry.obj'])
        # 공개 묶음은 입구 자리 넷(hid/version/winhttp/winmm) 중 설치 때 빈 곳을 쓴다. build/version.dll 은 예전처럼(관리창 빌드가 읽음)
        (out/'entry').mkdir(exist_ok=True)
        for slot in ['version', 'hid', 'winhttp', 'winmm']:
            run(['lld-link', '/dll', '/nodefaultlib', '/entry:DllMain', f'/def:entry_{slot}.def',
                 f'/out:{slot}.dll', 'entry.obj', 'kernel32.lib', '/Brepro'])
            run([NATIVE/'fix_exports.py', work/f'{slot}.dll', slot, NATIVE/'sys_exports.json'], sys.executable)
            shutil.copy2(work/f'{slot}.dll', out/'entry'/f'{slot}.dll')
        for name in [f'{module}.dll', f'{module}Loader.exe', 'version.dll']:
            shutil.copy2(work/name, out/name)
            print(name, hashlib.sha256((out/name).read_bytes()).hexdigest())
