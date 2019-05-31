extern crate winapi;
extern crate ws;

mod util;

use std::{io, thread, time};
use std::io::Write;

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
    let mut address = String::new();
    print!("Ip address: ");
    io::stdout().flush().unwrap();
    io::stdin().read_line(&mut address).unwrap();

    let socket = ws::Builder::new()
        .build(|out| { Server { out } })
        .unwrap();
    let handle = socket.broadcaster();
    thread::spawn(move || socket.listen(address.trim_end()));

    match util::find_game() {
        Ok((proc, exe_address)) => {
            println!("Author: AurevoirXavier\nInit succeed. Enjoy the game.");

            loop {
                if let Ok(ptr) = util::get_ptr(proc, vec![exe_address, 0x404BA90, 0x148, 0x440, 0, 0x758, 0x80, 0x190, 0, 0x30]) {
                    let buffer: *mut f32 = &mut 0.;
                    if let Ok(_) = util::read_process_memory(proc, ptr as _, buffer as _, 4) {
                        let hp = unsafe { *buffer };
                        let _ = handle.send(hp.to_string());
                    }
                }

                thread::sleep(time::Duration::from_millis(500));
            }
        }
        Err(e) => println!("{:?}", e)
    }
}
