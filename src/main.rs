extern crate ws;

use std::sync::{Arc, Mutex};
use std::thread;
use std::time;

const HTML: &'static [u8] = br#"
<html>
<head>
   <script type="text/javascript">
      function WebSocketTest() {
         if ("WebSocket" in window) {
            alert("WebSocket is supported by your Browser!");

            var ws = new WebSocket("ws://localhost:3012/ws");

            ws.onopen = function () {
               ws.send("Message to send");
            };

            ws.onmessage = function (evt) {
               document.getElementById('hp').innerHTML = evt.data;
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
<body>
   <div">
      <a id="hp" href="javascript:WebSocketTest()">Run WebSocket</a>
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
    let count = Arc::new(Mutex::new(0));
    let count_t = count.clone();
    thread::spawn(move || loop { *count_t.lock().unwrap() += 1; });

    let socket = ws::Builder::new()
        .build(|out| { Server { out } })
        .unwrap();
    let handle = socket.broadcaster();
    thread::spawn(|| socket.listen("127.0.0.1:3012"));

    loop {
        handle.send(count.lock().unwrap().to_string().as_str()).unwrap();
        thread::sleep(time::Duration::from_secs(1));
    }
}

