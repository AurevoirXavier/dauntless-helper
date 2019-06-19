#include <Windows.h>
#include <iostream>
#include <string>

using namespace std;

void inject(HANDLE hProcess, LPCWSTR dllPath) {
    LPVOID lpRemoteAddress = VirtualAllocEx(hProcess, nullptr, 1, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    WriteProcessMemory(hProcess, lpRemoteAddress, dllPath, wcslen(dllPath) * 2 + 2, nullptr);
    HANDLE hHandle = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE) GetProcAddress(LoadLibrary("kernel32"), "LoadLibraryW"), lpRemoteAddress, NULL, nullptr);
    WaitForSingleObject(hHandle, -1);
    VirtualFreeEx(hProcess, lpRemoteAddress, 1, MEM_DECOMMIT);
}

int main() {
    HWND hWnd = FindWindow(nullptr, "Dauntless  ");
    DWORD processId;
    GetWindowThreadProcessId(hWnd, &processId);
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    wstring dir = path;
    while (dir[dir.length() - 1] != L'\\') dir.pop_back();

    inject(hProcess, (dir + L"FW1FontWrapper.dll").c_str());
    inject(hProcess, (dir + L"hack.dll").c_str());

    return 0;
}
