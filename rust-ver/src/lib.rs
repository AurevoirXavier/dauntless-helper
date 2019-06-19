#[macro_use]
extern crate detour;
extern crate winapi;

mod d3d11_vmt_tables;

#[allow(dead_code)]
mod debug {
    use std::{
        fmt::Display,
        ptr::null_mut,
    };

    pub unsafe fn clear_last_error() {
        use winapi::um::errhandlingapi::SetLastError;
        SetLastError(0);
    }

    pub unsafe fn show_last_error() {
        use winapi::um::{
            errhandlingapi::GetLastError,
            winuser::{MB_OK, MessageBoxA},
        };

        let e = GetLastError();
        let e = format!("{}\0", e);
        MessageBoxA(null_mut(), e.as_ptr() as _, "\0".as_ptr() as _, MB_OK);
    }

    pub unsafe fn print_in_message_box<T: Display>(s: T) {
        use winapi::um::winuser::{MessageBoxW, MB_OK};

        let s = format!("{}\0", s);
        MessageBoxW(
            null_mut(),
            s.encode_utf16().collect::<Vec<_>>().as_ptr(),
            "\0".encode_utf16().collect::<Vec<_>>().as_ptr(),
            MB_OK,
        );
    }
}

use std::ptr::null_mut;
use winapi::{
    shared::{
        dxgi::IDXGISwapChain,
        minwindef::LPVOID,
    },
    um::d3d11::{ID3D11Device, ID3D11DeviceContext},
};

struct PresentPointers {
    context: *mut ID3D11DeviceContext,
    device: *mut ID3D11Device,
    swap_chain: *mut IDXGISwapChain,

    present: *mut Present,
}

impl PresentPointers {
    const INIT: Self = PresentPointers {
        context: null_mut(),
        device: null_mut(),
        swap_chain: null_mut(),

        present: null_mut(),
    };

    unsafe fn get_present(&mut self) {
        use winapi::{
            shared::{
                dxgi::{DXGI_SWAP_CHAIN_DESC, DXGI_SWAP_EFFECT_DISCARD},
                dxgiformat::DXGI_FORMAT_R8G8B8A8_UNORM,
                dxgitype::{
                    DXGI_MODE_DESC,
                    DXGI_MODE_SCALING_UNSPECIFIED,
                    DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                    DXGI_RATIONAL, DXGI_SAMPLE_DESC,
                    DXGI_USAGE_RENDER_TARGET_OUTPUT,
                },
            },
            um::{
                d3d11::{D3D11CreateDeviceAndSwapChain, D3D11_SDK_VERSION},
                d3dcommon::D3D_DRIVER_TYPE_HARDWARE,
                winuser::GetForegroundWindow,
            },
        };

        let swap_chain_desc = DXGI_SWAP_CHAIN_DESC {
            BufferDesc: DXGI_MODE_DESC {
                Width: 1,
                Height: 1,
                RefreshRate: DXGI_RATIONAL {
                    Numerator: 0,
                    Denominator: 1,
                },
                Format: DXGI_FORMAT_R8G8B8A8_UNORM,
                ScanlineOrdering: DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                Scaling: DXGI_MODE_SCALING_UNSPECIFIED,
            },
            SampleDesc: DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            BufferUsage: DXGI_USAGE_RENDER_TARGET_OUTPUT,
            BufferCount: 1,
            OutputWindow: GetForegroundWindow(),
            Windowed: 1,
            SwapEffect: DXGI_SWAP_EFFECT_DISCARD,
            Flags: 0,
        };
        D3D11CreateDeviceAndSwapChain(
            null_mut(),
            D3D_DRIVER_TYPE_HARDWARE,
            null_mut(),
            0,
            null_mut(),
            0,
            D3D11_SDK_VERSION,
            &swap_chain_desc,
            &mut self.swap_chain,
            null_mut(),
            null_mut(),
            null_mut(),
        );

        let p_vmt = *(self.swap_chain as *mut *mut *mut ());
        self.present = p_vmt.offset(d3d11_vmt_tables::IDXGISwapChainVMT::Present as _) as _;

        release(&mut self.swap_chain);
    }

    unsafe fn get_device_and_context(&mut self) {
        use winapi::Interface;

        (&*self.swap_chain).GetDevice(&ID3D11Device::uuidof(), &mut self.device as *mut _ as _);
        (&*self.device).GetImmediateContext(&mut self.context as *mut _ as _);
    }
}

type Present = unsafe extern "system" fn(*mut IDXGISwapChain, u32, u32) -> i32;

struct Detour {
    present: Option<&'static detour::StaticDetour<Present>>,
    pointers: PresentPointers,
}

impl Detour {
    const INIT: Self = Detour {
        present: None,
        pointers: PresentPointers::INIT,
    };
}

static mut DETOUR: Detour = Detour::INIT;

static_detour! {
    static PresentHook: unsafe extern "system" fn(*mut IDXGISwapChain, u32, u32) -> i32;
}

unsafe fn release<T>(com_ptr: &mut *mut T) {
    use winapi::um::unknwnbase::IUnknown;

    if !(*com_ptr).is_null() {
        (&*((*com_ptr) as *mut IUnknown)).Release();
        *com_ptr = null_mut();
    }
}

static mut INIT: bool = true;

fn hook_present(p_swap_chain: *mut IDXGISwapChain, sync_interval: u32, flags: u32) -> i32 {
    unsafe {
        if INIT {
            debug::print_in_message_box("Inject succeed by Xavier");

            DETOUR.pointers.swap_chain = p_swap_chain;
            DETOUR.pointers.get_device_and_context();

            INIT = false;
        }

        PresentHook.call(p_swap_chain, sync_interval, flags)
    }
}

unsafe fn detour_present() {
    DETOUR.pointers.get_present();
    DETOUR.present = Some(PresentHook.initialize(*DETOUR.pointers.present, hook_present).unwrap());
    DETOUR.present.unwrap().enable().unwrap();
}

unsafe extern "system" fn free_library(lp_thread_parameter: LPVOID) -> u32 {
    use winapi::um::libloaderapi::FreeLibrary;
    FreeLibrary(lp_thread_parameter as _);

    0
}

unsafe extern "system" fn init_hook(p_handle: LPVOID) -> u32 {
    use winapi::um::{
        processthreadsapi::CreateThread,
        winuser::{GetAsyncKeyState, VK_END},
    };

    detour_present();
    while (GetAsyncKeyState(VK_END) & -32767) == 0 {}

    if let Some(present) = DETOUR.present { present.disable().unwrap(); }
    CreateThread(null_mut(), 0, Some(free_library), p_handle, 0, null_mut());

    0
}

#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "system" fn DllMain(h_dll: winapi::shared::minwindef::HINSTANCE, reason: u32, _: LPVOID) -> i32 {
    use winapi::um::{
        libloaderapi::DisableThreadLibraryCalls,
        processthreadsapi::CreateThread,
    };

    match reason {
        1 => {
            DisableThreadLibraryCalls(h_dll);
            CreateThread(null_mut(), 0, Some(init_hook), h_dll as _, 0, null_mut());
        }
//        0 => (),
        _ => (),
    }

    1
}

#[test]
fn test() {
    unsafe {
        let jmp = [0xFF, 0x25, 0x00, 0x00, 0x00, 0x00];
        println!("{}", std::mem::size_of_val(&jmp));
        println!("{}", jmp.len());
    }
}
