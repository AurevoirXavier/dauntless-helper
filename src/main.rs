extern crate inputbot;
extern crate ws;
extern crate winapi;

mod util;

static HTML: &'static [u8] = br#"
<html>
<head>
    <script type="text/javascript">
        function WebSocketTest() {
            document.getElementById("hp").setAttribute("href", "\#");

            if ("WebSocket" in window) {
                alert("WebSocket is supported by your Browser!");

                var ws = new WebSocket("ws://" + window.location.host + "/ws");

                ws.onmessage = function (evt) {
                    document.getElementById("hp").innerHTML = evt.data;
                };

                ws.onclose = function () {
                    alert("Connection is closed...");
                };
            } else {
                alert("WebSocket NOT supported by your Browser!");
            }
        }
    </script>
</head>
<body style="background-color: gray">
    <div>
        <a id="hp" href="javascript:WebSocketTest()" style="color: white; width: 400px; height: 100px; position: absolute; left: 0; top: 0; bottom: 0; right: 0; margin: auto; text-align: center; line-height: 100px; font-size: 40px; text-decoration:none">Boss HP</a>
    </div>
</body>
</html>
"#;

struct Server { out: ws::Sender }

impl ws::Handler for Server {
    fn on_request(&mut self, req: &ws::Request) -> ws::Result<(ws::Response)> {
        match req.resource() {
            "/ws" => ws::Response::from_request(req),
            "/" => Ok(ws::Response::new(200, "OK", HTML.to_vec())),
            _ => Ok(ws::Response::new(404, "Not Found", b"404 - Not Found".to_vec())),
        }
    }
}

fn main() {
    // --- std ---
    use std::{
        io::{self, Write},
        thread,
        time,
    };
    // --- custom ---
    use util::{
        game::*,
        windows::find_game,
    };

    let mut address = String::new();
    print!("Author: AurevoirXavier\nIp address: ");
    io::stdout().flush().unwrap();
    io::stdin().read_line(&mut address).unwrap();

    let socket = ws::Builder::new()
        .build(|out| { Server { out } })
        .unwrap();
    let handle = socket.broadcaster();
    thread::spawn(move || socket.listen(address.trim_end()));

    match find_game() {
        Ok((proc, exe_address)) => {
            println!("Init succeed. Enjoy the game.");

            let mut duration = 100;
            let mut msg;
            let (mut coordinates, mut coordinates_ptrs) = (vec![], vec![]);
            let mut mouse_angles_ptr = 0;
            loop {
                if let Ok(hp) = get_boss_hp(proc, exe_address) {
                    msg = hp.to_string();

                    if inputbot::KeybdKey::LControlKey.is_pressed() {
                        if let Ok(c) = get_player_boss_coordinates(proc, exe_address, &mut coordinates_ptrs) { coordinates = c; } else { continue; }
                        if let Err(_) = aim_bot(proc, exe_address, &coordinates, &mut mouse_angles_ptr) { continue; }

                        duration = 10;
                    } else { duration = 100; }
                } else {
                    msg = String::from("Not yet started");

                    coordinates.clear();
                    coordinates_ptrs.clear();

                    mouse_angles_ptr = 0;
                }

                let _ = handle.send(msg.clone());

                thread::sleep(time::Duration::from_millis(duration));
            }
        }
        Err(e) => println!("{:?}", e)
    }
}
