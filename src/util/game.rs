// --- external ---
use winapi::um::winnt::HANDLE;
// --- custom ---
use super::windows::*;

fn get_mouse_angles(proc: HANDLE, ptr: u64) -> Result<(Vec<f32>, Vec<u64>), Error> {
    let mut mouse_angles = Vec::with_capacity(2);
    let ptrs = vec![
        get_ptr(proc, vec![ptr, 0x398])?,
        get_ptr(proc, vec![ptr, 0x39C])?
    ];

    for &ptr in ptrs.iter() {
        let buffer: *mut f32 = &mut 0.;
        read_process_memory(proc, ptr as _, buffer as _, 4)?;
        mouse_angles.push(unsafe { *buffer });
    }

    Ok((mouse_angles, ptrs))
}

fn calc_angle(coordinates: &[f32]) -> (f32, f32) {
    // --- std ---
    use std::f32::consts::PI;

    let delta_x = coordinates[0] - coordinates[3];
    let delta_y = coordinates[1] - coordinates[4];
    let delta_z = coordinates[2] - coordinates[5];

    let angle_x = {
        let mut angle = (delta_y / delta_x).atan() * 180. / PI;
        // I Quadrant
        if coordinates[3] > coordinates[0] && coordinates[4] > coordinates[1] {
            println!("At I Quadrant, raw angle: {}", angle);
        }
        // II Quadrant
        else if coordinates[3] < coordinates[0] && coordinates[4] > coordinates[1] {
            angle += 180.;
            println!("At II Quadrant, raw angle: {}", angle);
        }
        // III Quadrant
        else if coordinates[3] < coordinates[0] && coordinates[4] < coordinates[1] {
            angle += 180.;
            println!("At III Quadrant, raw angle: {}", angle);
        }
        // IV Quadrant
        else if coordinates[3] > coordinates[0] && coordinates[4] < coordinates[1] {
            angle += 360.;
            println!("At IV Quadrant, raw angle: {}", angle);
        }
        // unreachable
        else { println!("{:?}", coordinates); };

        angle
    };
    let angle_y = {
        let angle = (delta_z / (delta_x.powi(2) + delta_y.powi(2)).sqrt()).atan() * 180. / PI;
        if angle < 0. { angle.abs() } else { 360. - angle }
    };

    (angle_x, angle_y)
}

pub fn get_boss_hp(proc: HANDLE, exe_address: u64) -> Result<f32, Error> {
    let ptr = get_ptr(proc, vec![exe_address + 0x4076AF0, 0x148, 0x440, 0, 0x758, 0x190, 0, 0x30])?;
    let buffer: *mut f32 = &mut 0.;
    read_process_memory(proc, ptr as _, buffer as _, 4)?;

    Ok(unsafe { *buffer })
}

pub fn get_player_boss_coordinates(proc: HANDLE, exe_address: u64, ptrs: &mut Vec<u64>) -> Result<Vec<f32>, Error> {
    if ptrs.is_empty() {
        *ptrs = vec![
            // player coordinates base address
            get_ptr(proc, vec![exe_address + 0x3C49B40, 0x8, 0x268, 0x110, 0x140, 0x3A0, 0x190])?,
            get_ptr(proc, vec![exe_address + 0x3C49B40, 0x8, 0x268, 0x110, 0x140, 0x3A0, 0x194])?,
            get_ptr(proc, vec![exe_address + 0x3C49B40, 0x8, 0x268, 0x110, 0x140, 0x3A0, 0x198])?,
            // boss coordinates base address
            get_ptr(proc, vec![exe_address + 0x3AE9B90, 0x8, 0, 0x28, 0x30, 0x110, 0xB0, 0x90])?,
            get_ptr(proc, vec![exe_address + 0x3AE9B90, 0x8, 0, 0x28, 0x30, 0x110, 0xB0, 0x94])?,
            get_ptr(proc, vec![exe_address + 0x3AE9B90, 0x8, 0, 0x28, 0x30, 0x110, 0xB0, 0x98])?,
        ];
    }

    let mut coordinates = Vec::with_capacity(6);
    for &ptr in ptrs.iter() {
        let buffer: *mut f32 = &mut 0.;
        read_process_memory(proc, ptr as _, buffer as _, 4)?;

        coordinates.push(unsafe { *buffer });
    }

    Ok(coordinates)
}

pub fn aim_bot(proc: HANDLE, exe_address: u64, coordinates: &[f32], ptr: &mut u64) -> Result<(), Error> {
    if *ptr == 0 { *ptr = get_ptr(proc, vec![exe_address + 0x3A90650, 0x58, 0x90, 0xC8, 0x30, 0x78])?; }
    let (_angles, angles_ptrs) = get_mouse_angles(proc, *ptr)?;
    let (angle_x, angle_y) = calc_angle(coordinates);

    write_process_memory(proc, angles_ptrs[1] as _, &angle_x as *const f32, 4)?;
    write_process_memory(proc, angles_ptrs[0] as _, &angle_y as *const f32, 4)?;

    Ok(())
}
