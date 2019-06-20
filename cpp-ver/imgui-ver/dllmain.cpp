#include <d3d11.h>
#include <detours.h>
#include <helper.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "detours.lib")
#pragma comment(lib, "helper")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

typedef HRESULT(__fastcall *PresentHook)(IDXGISwapChain *, UINT, UINT);

bool g_initialised = false;
bool g_activeMenu = false;
bool g_showMenu = true;

HWND dauntlessWindow = FindWindow(nullptr, "Dauntless  ");
DWORD64 processAddress = (DWORD64) GetModuleHandle(nullptr);

Helper helper;

static PresentHook presentHook;
static ID3D11DeviceContext *pContext = nullptr;
static ID3D11Device *pDevice = nullptr;
static ID3D11RenderTargetView *pRenderTargetView;
static IDXGISwapChain *pSwapChain = nullptr;
static HWND window = nullptr;
static WNDPROC originalWndProc = nullptr;

//void launchConsole() {
//    AllocConsole();
//    SetConsoleTitle("[+] Hooking DirectX 11 by Xavier");
//    freopen("CONOUT$", "w", stdout);
//    freopen("CONOUT$", "w", stderr);
//    freopen("CONIN$", "r", stdin);
//}

LRESULT CALLBACK hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ImGuiIO &io = ImGui::GetIO();
    POINT mPos;
    GetCursorPos(&mPos);
    ScreenToClient(window, &mPos);
    ImGui::GetIO().MousePos.x = (float) mPos.x;
    ImGui::GetIO().MousePos.y = (float) mPos.y;

    if (uMsg == WM_KEYUP) {
        if (wParam == VK_F1) g_activeMenu = !g_activeMenu;
        if (wParam == VK_INSERT) g_showMenu = !g_showMenu;
    }
    if (g_activeMenu) {
        SetCursor(io.MouseDrawCursor ? nullptr : LoadCursor(nullptr, IDC_ARROW));
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

        return true;
    }

    return CallWindowProc(originalWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __fastcall hookPresent(IDXGISwapChain *pIDXGISwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_initialised) {
        g_initialised = true;

        pIDXGISwapChain->GetDevice(__uuidof(ID3D11Device), (LPVOID *) &pDevice);
        pDevice->GetImmediateContext(&pContext);
        pSwapChain = pIDXGISwapChain;
        DXGI_SWAP_CHAIN_DESC sd;
        pIDXGISwapChain->GetDesc(&sd);

        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad

        io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\Arial.ttf)", 16);

        window = sd.OutputWindow;

        originalWndProc = (WNDPROC) SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR) hWndProc);

        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX11_Init(pDevice, pContext);
        ImGui::GetIO().ImeWindowHandle = window;

        ID3D11Texture2D *pBackBuffer;

        pIDXGISwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *) &pBackBuffer);
        pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRenderTargetView);
        pBackBuffer->Release();

        CreateThread(nullptr, NULL, Helper::run, nullptr, NULL, nullptr);
    }

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();

    ImGui::NewFrame();
    if (g_showMenu) Helper::draw();
    ImGui::EndFrame();

    ImGui::Render();

    pContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return presentHook(pIDXGISwapChain, SyncInterval, Flags);
}

void detourPresent() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(LPVOID &) presentHook, (LPVOID) hookPresent);
    DetourTransactionCommit();
}

void getVMT() {
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
    presentHook = (PresentHook) pVMT[8];
}

DWORD WINAPI start(LPVOID pDll) {
//    launchConsole();
    getVMT();
    detourPresent();

    return NULL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            CreateThread(nullptr, NULL, start, hModule, NULL, nullptr);
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
        default:
            return TRUE;
    }

    return TRUE;
}
