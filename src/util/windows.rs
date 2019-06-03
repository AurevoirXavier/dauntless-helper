// --- external ---
use winapi::{
    shared::{
        minwindef::{LPCVOID, LPVOID},
        windef::HWND,
    },
    um::winnt::HANDLE,
};

const WINDOW_NAME: &'static str = "Dauntless  ";

#[derive(Debug)]
pub enum Error {
    FindWindowError,
    GetWindowThreadProcessIdError,
    OpenProcessError,
    ReadProcessMemoryError,
    WriteProcessMemoryError,
    CreateToolhelp32SnapshotError,
    Module32FirstError,
    FindModuleError,
}

fn find_window() -> Result<HWND, Error> {
    // --- external ---
    use winapi::{
        shared::minwindef::LPARAM,
        um::winuser::EnumWindows,
    };

    extern "system" fn enum_windows_proc(hwnd: HWND, l_param: LPARAM) -> i32 {
        // --- std ---
        use std::{
            ffi::OsString,
            os::windows::ffi::OsStringExt,
        };
        // --- external ---
        use winapi::um::winuser::GetWindowTextW;

        let mut buffer = [0; 128];
        let len = unsafe { GetWindowTextW(hwnd, buffer.as_mut_ptr(), buffer.len() as _) };
        let name = OsString::from_wide(&buffer[..len as _]);

        if name.to_str().unwrap() == WINDOW_NAME {
            unsafe { *(l_param as *mut HWND) = hwnd; }
            0
        } else { 1 }
    }

    let mut hwnd = 0 as HWND;
    unsafe { EnumWindows(Some(enum_windows_proc), &mut hwnd as *mut HWND as _); }
    if hwnd.is_null() { return Err(Error::FindWindowError); }

    Ok(hwnd)
}

fn get_window_thread_process_id(hwnd: HWND) -> Result<u32, Error> {
    // --- external ---
    use winapi::{
        shared::minwindef::LPDWORD,
        um::winuser::GetWindowThreadProcessId,
    };

    let ptr: LPDWORD = &mut 0;
    unsafe {
        GetWindowThreadProcessId(hwnd, ptr);
        if ptr == 0 as _ { Err(Error::GetWindowThreadProcessIdError) } else { Ok(*ptr) }
    }
}

fn open_process(process_id: u32) -> Result<HANDLE, Error> {
    // --- external ---
    use winapi::{
        shared::minwindef::TRUE,
        um::{
            processthreadsapi::OpenProcess,
            winnt::PROCESS_ALL_ACCESS,
        },
    };

    let handle = unsafe { OpenProcess(PROCESS_ALL_ACCESS, TRUE, process_id) };
    if handle.is_null() { Err(Error::OpenProcessError) } else { Ok(handle) }
}

fn get_exe_address(proc_id: u32) -> Result<u64, Error> {
    // --- std ---
    use std::{
        mem::size_of,
        ffi::CStr,
    };
    // --- external ---
    use winapi::um::{
        handleapi::{INVALID_HANDLE_VALUE, CloseHandle},
        tlhelp32::{TH32CS_SNAPMODULE, MODULEENTRY32, CreateToolhelp32Snapshot, Module32First},
    };

    let h_module_snap = unsafe { CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, proc_id) };
    if h_module_snap == INVALID_HANDLE_VALUE { Err(Error::CreateToolhelp32SnapshotError) } else {
        let mut me32 = MODULEENTRY32 {
            dwSize: size_of::<MODULEENTRY32>() as _,
            th32ModuleID: 0,
            th32ProcessID: 0,
            GlblcntUsage: 0,
            ProccntUsage: 0,
            modBaseAddr: 0 as _,
            modBaseSize: 0,
            hModule: 0 as _,
            szModule: [0; 256],
            szExePath: [0; 260],
        };
        let lpme32: *mut MODULEENTRY32 = &mut me32;

        unsafe {
            if Module32First(h_module_snap, lpme32) == 0 {
                CloseHandle(h_module_snap);
                Err(Error::Module32FirstError)
            } else {
                let module_name = CStr::from_ptr(me32.szModule.as_ptr() as _).to_str().unwrap();
                if module_name == "Dauntless-Win64-Shipping.exe" {
                    CloseHandle(h_module_snap);
                    Ok(*(&me32.modBaseAddr as *const _ as *const u64))
                } else {
                    CloseHandle(h_module_snap);
                    Err(Error::FindModuleError)
                }
            }
        }
    }
}

pub fn find_game() -> Result<(HANDLE, u64), Error> {
    let proc_id = get_window_thread_process_id(find_window()?)?;
    let proc = open_process(proc_id)?;
    let mono_address = get_exe_address(proc_id)?;

    Ok((proc, mono_address))
}

pub fn read_process_memory(handle: HANDLE, address: LPVOID, buffer: LPVOID, size: usize) -> Result<(), Error> {
    // --- external ---
    use winapi::um::memoryapi::ReadProcessMemory;

    let result = unsafe { ReadProcessMemory(handle, address, buffer, size, 0 as _) };
    if result == 0 { Err(Error::ReadProcessMemoryError) } else { Ok(()) }
}

pub fn write_process_memory<T>(handle: HANDLE, address: LPVOID, buffer: *const T, size: usize) -> Result<(), Error> {
    // --- external ---
    use winapi::um::memoryapi::WriteProcessMemory;

    let result = unsafe { WriteProcessMemory(handle, address, buffer as LPCVOID, size, 0 as _) };
    if result == 0 { Err(Error::WriteProcessMemoryError) } else { Ok(()) }
}

pub fn get_ptr(proc: HANDLE, offsets: Vec<u64>) -> Result<u64, Error> {
    let mut ptr = offsets[0];
    for &offset in offsets[1..].iter() {
        let buffer: *mut u64 = &mut 0;
        read_process_memory(proc, ptr as _, buffer as _, 8)?;
        unsafe { ptr = *buffer + offset; }
    }

    Ok(ptr)
}
