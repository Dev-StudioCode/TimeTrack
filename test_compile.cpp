#include <windows.h>
#include <windhawk_api.h>

BOOL Wh_ModInit() {
    Wh_Log(L"TimeTrack initialized");
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"TimeTrack uninitialized");
}
