/* LOH_GiftHighlightLoader — 켜져 있는 영웅입지전(ThreeKingdom.exe)에 LOH_GiftHighlight.dll 을 올린다.
 * 게임 파일은 건드리지 않는다. 게임을 끄면 흔적이 남지 않는다. (청심단 진단기 loader.c 와 같은 방식) */
#include "win.h"
#define DLLW L"LOH_GiftHighlight.dll"
void *memset(void *d, int c, uptr n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }
void *memcpy(void *d, const void *s, uptr n) { u8 *p = d; const u8 *q = s; while (n--) *p++ = *q++; return d; }
typedef struct { u32 size, usage, pid; uptr heap; u32 mod, threads, ppid; int pri; u32 flags; wch exe[260]; } PE32;
typedef struct { u32 size, modid, pid, g, p; u8 *base; u32 bsize; HANDLE h; wch name[256]; wch path[260]; } ME32;
W int API Process32FirstW(HANDLE, PE32 *); W int API Process32NextW(HANDLE, PE32 *);
W int API Module32FirstW(HANDLE, ME32 *); W int API Module32NextW(HANDLE, ME32 *);
static void say(const wch *s) { u32 n; WriteConsoleW(GetStdHandle((u32)-11), s, (u32)lstrlenW(s), &n, 0); WriteConsoleW(GetStdHandle((u32)-11), L"\r\n", 2, &n, 0); }
static void wcat(wch *d, const wch *s) { while (*d) d++; while ((*d++ = *s++)) ; }

void Entry(void) {
    static wch dll[700]; PE32 pe; ME32 me; HANDLE snap, proc, th; u32 pid = 0, count = 0, code = 0; int i; void *mem, *ll;
    u32 n = GetModuleFileNameW(0, dll, 500);
    for (i = (int)n; i > 0 && dll[i - 1] != '\\'; i--) ;
    dll[i] = 0; wcat(dll, DLLW);
    if (GetFileAttributesW(dll) == 0xFFFFFFFF) { say(DLLW L" 이 이 폴더에 없습니다."); ExitProcess(2); }
    snap = CreateToolhelp32Snapshot(2, 0); pe.size = sizeof pe;
    if (Process32FirstW(snap, &pe)) do { if (lstrcmpiW(pe.exe, L"ThreeKingdom.exe") == 0) { pid = pe.pid; count++; } } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    if (count == 0) { say(L"게임이 켜져 있지 않습니다. 영웅입지전을 먼저 켜고 타이틀 화면이 나온 뒤 다시 실행하세요."); ExitProcess(3); }
    if (count > 1) { say(L"게임이 여러 개 켜져 있습니다. 하나만 남기고 다시 실행하세요."); ExitProcess(3); }
    snap = CreateToolhelp32Snapshot(8 | 0x10, pid); me.size = sizeof me;
    if (snap != INVALID_HANDLE) {
        if (Module32FirstW(snap, &me)) do { if (lstrcmpiW(me.name, DLLW) == 0) { say(L"선물 선호 표시 DLL 이 이미 들어가 있습니다. 다른 판으로 바꾸려면 게임을 껐다 켜세요."); ExitProcess(0); } } while (Module32NextW(snap, &me));
        CloseHandle(snap);
    }
    proc = OpenProcess(0x043A, 0, pid);
    if (!proc) { say(L"게임 프로세스를 열지 못했습니다. 게임을 관리자 권한으로 켰다면 일반 권한으로 다시 켜세요."); ExitProcess(4); }
    n = (u32)(lstrlenW(dll) + 1) * 2;
    mem = VirtualAllocEx(proc, 0, n, 0x3000, 4);
    ll = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!mem || !ll || !WriteProcessMemory(proc, mem, dll, n, 0)) { say(L"게임 메모리에 경로를 쓰지 못했습니다."); ExitProcess(5); }
    th = CreateRemoteThread(proc, 0, 0, ll, mem, 0, 0);
    if (!th) { say(L"DLL 을 올리지 못했습니다(원격 스레드 실패). 백신이 막았을 수 있습니다."); ExitProcess(6); }
    if (WaitForSingleObject(th, 15000) != 0) { say(L"DLL 로딩을 기다리는 중입니다. 잠시 후 로그를 확인하세요."); CloseHandle(th); CloseHandle(proc); ExitProcess(8); } GetExitCodeThread(th, &code); CloseHandle(th);
    VirtualFreeEx(proc, mem, 0, 0x8000); CloseHandle(proc);
    if (!code) { say(L"DLL 이 올라가지 않았습니다. 백신이 막았는지 확인하세요."); ExitProcess(7); }
    say(L"선물 선호 표시 DLL 을 올렸습니다. LOH_GiftHighlight.log 의 READY 줄을 확인하세요.");
    ExitProcess(0);
}
