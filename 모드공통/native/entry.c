/* One optional bootstrap slot; either module works when installed alone. */
#include "win.h"
W HANDLE API LoadLibraryW(const wch *);
static wch root[600];
__declspec(dllexport) const char *LOH_SocialMods_Entry(void){return "LOH_SocialMods entry 1";}
static void load(const wch *relative){wch path[800];int i=0,j=0;
    while(root[i]&&i<590){path[i]=root[i];i++;}
    while(relative[j]&&i<799)path[i++]=relative[j++];path[i]=0;
    if(GetFileAttributesW(path)!=0xffffffff)LoadLibraryW(path);
}
static u32 API boot(void *unused){(void)unused;
    load(L"LOH_GiftHighlight\\LOH_GiftHighlight.dll");
    load(L"LOH_GoldenOrchidJade\\LOH_GoldenOrchidJade.dll");return 0;
}
int API DllMain(HANDLE h,u32 reason,void *unused){(void)unused;
    if(reason==1){wch exe[600];u32 n,k;DisableThreadLibraryCalls(h);
        n=GetModuleFileNameW(0,exe,600);k=n;while(k&&exe[k-1]!='\\')k--;
        if(n&&lstrcmpiW(exe+k,L"ThreeKingdom.exe")==0){
            n=GetModuleFileNameW(h,root,590);while(n&&root[n-1]!='\\')n--;root[n]=0;
            if(n){HANDLE th=CreateThread(0,0,boot,0,0,0);if(th)CloseHandle(th);}
        }
    }return 1;
}
