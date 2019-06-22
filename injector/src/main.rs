extern crate winapi;

mod byte;

use std::{
    fs::File,
    io::Write,
    ptr::{null, null_mut},
};
use dirs::home_dir;
use winapi::{
    shared::minwindef::LPVOID,
    um::{
        libloaderapi::{GetModuleHandleA, GetProcAddress},
        memoryapi::{VirtualAllocEx, VirtualFreeEx, WriteProcessMemory},
        processthreadsapi::{CreateRemoteThread, OpenProcess},
        synchapi::WaitForSingleObject,
        winnt::{MEM_COMMIT, MEM_DECOMMIT, PAGE_EXECUTE_READWRITE, PROCESS_ALL_ACCESS},
        winuser::{FindWindowW, GetWindowThreadProcessId},
    },
};

fn main() {
    let h_process;
    unsafe {
        let h_wnd = FindWindowW(null(), "Dauntless  \0".encode_utf16().collect::<Vec<_>>().as_ptr());
        let mut process_id = 0;
        GetWindowThreadProcessId(h_wnd, &mut process_id);
        h_process = OpenProcess(PROCESS_ALL_ACCESS, 0, process_id);
    }

    let path = home_dir()
        .unwrap()
        .join("AppData")
        .join("Local")
        .join("dauntless_helper.dll")
        .to_str()
        .unwrap()
        .to_string();

    {
        let mut temp_dll = File::create(&path).unwrap();
        temp_dll.write_all(&byte::RAW_DATA).unwrap();
        temp_dll.flush().unwrap();
        temp_dll.sync_all().unwrap();
    }

    {
        unsafe {
            let lp_remote_address = VirtualAllocEx(h_process, null_mut(), 1, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            WriteProcessMemory(h_process, lp_remote_address, format!("{}\0", &path).encode_utf16().collect::<Vec<_>>().as_ptr() as _, path.len() * 2 + 2, null_mut());
            let f = GetProcAddress(GetModuleHandleA("KERNEL32.DLL\0".as_ptr() as _), "LoadLibraryW\0".as_ptr() as _);
            let h_handle = CreateRemoteThread(h_process, null_mut(), 0, Some(*(&f as *const _ as *const unsafe extern "system" fn(LPVOID) -> u32)), lp_remote_address, 0, null_mut());
            WaitForSingleObject(h_handle, 100);
            VirtualFreeEx(h_process, lp_remote_address, 1, MEM_DECOMMIT);
        }
    }
}
