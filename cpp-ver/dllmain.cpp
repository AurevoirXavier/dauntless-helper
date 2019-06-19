#include "menu.cpp"

//#pragma warning(disable:4996)

LPVOID presentFromVMT() {
    IDXGISwapChain *swapChain;
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChainDesc.BufferDesc.Width = 1;
    swapChainDesc.BufferDesc.Height = 1;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = GetForegroundWindow();
    swapChainDesc.Windowed = 1;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = NULL;
    D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            NULL,
            nullptr,
            NULL,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &swapChain,
            nullptr,
            nullptr,
            nullptr
    );

    LPVOID *pVMT = *((void ***) swapChain);
    return pVMT[8];
}

HRESULT __fastcall hookPresent(IDXGISwapChain *pSwapChain, UINT syncInterval, UINT flags) {
    if (init) {
        MessageBoxW(
                nullptr,
                L""
                "Insert: [show menu] [显示菜单]\n"
                "Home: [teleport to Boss] [传送 Boss]\n"
                "Delete: [attack from far] [远程攻击]\n"
                "End: [cancel the injection] [取消注入]\n\n"
                "Other features: use the arrow keys\n"
                "其它功能: 使用方向键\n\n"
                "I post ONLY on my github. ALL FREE & USE AT YOUR OWN RISK!\n"
                "切勿相信本人 Github 以外的任何来源. 免费 & 风险自担!\n"
                "Github: https://github.com/AurevoirXavier",
                L"dauntless-helper",
                MB_OK
        );

        ID3D11Device *pDevice = nullptr;
        pSwapChain->GetDevice(__uuidof(ID3D11Device), (PVOID *) &pDevice);
        pDevice->GetImmediateContext(&menu.pContext);
//        printf("%p - %p\n", pDevice, menu.pContext);

        ID3D11Texture2D *pBackBuffer;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *) &pBackBuffer);
        pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &menu.pTargetView);
        pBackBuffer->Release();

        IFW1Factory *pFW1Factory = nullptr;
        FW1CreateFactory(FW1_VERSION, &pFW1Factory);
        pFW1Factory->CreateFontWrapper(pDevice, L"Arial", &menu.pFontWrapper);
//        printf("%p - %p\n", pFW1Factory, menu.pFontWrapper);

        pFW1Factory->Release();
        pDevice->Release();

        menu.processAddress = (DWORD64) GetModuleHandle(nullptr);
        menu.options.emplace_back(L"Auto Click", autoClick);
        menu.options.emplace_back(L"Speed Up Attack", speedUpAttack);
        menu.options.emplace_back(L"Speed Up Movement", speedUpMovement);
        menu.options.emplace_back(L"Instant Movement", instantMovement);
        menu.options.emplace_back(L"Infinity Stamina", infinityStamina);

        init = false;

        CreateThread(nullptr, NULL, Menu::run, nullptr, NULL, nullptr);
    }

    menu.draw();

    return menu.present(pSwapChain, syncInterval, flags);
}

PVOID detourPresent() {
    LPVOID pPresent = presentFromVMT();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&pPresent, (LPVOID) hookPresent);
    DetourTransactionCommit();

    return pPresent;
}

DWORD initHook(LPVOID pHandle) {
    menu.present = (Present) detourPresent();

    while ((GetAsyncKeyState(VK_END) & 1) == 0);

    init = true;
    menu.~Menu();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    auto pPresent = (LPVOID) menu.present;
    DetourDetach(&pPresent, (LPVOID) hookPresent);
    DetourTransactionCommit();

    CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE) FreeLibrary, pHandle, NULL, nullptr);

    return NULL;
}

BOOL WINAPI DllMain(HMODULE hDll, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
//        AllocConsole();
//        freopen("CONOUT$", "w", stdout);
        DisableThreadLibraryCalls(hDll);
        CreateThread(nullptr, NULL, initHook, hDll, NULL, nullptr);
    }

    return TRUE;
}
