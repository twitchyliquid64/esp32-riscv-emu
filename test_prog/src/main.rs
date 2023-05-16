#![no_main]
#![no_std]

extern crate panic_halt;

use riscv_minimal_rt::entry;

use test_prog::*;

#[entry]
fn main() -> ! {
    printf!("hi!\n");
    delay(1000);

    printf!("connecting...");
    wifi::connect_as_station("YOUR_SSID", "YOUR_WIFI_PASSWORD", 0);

    while !wifi::is_connected() {
        delay(500);
        printf!(".");
    }

    let ip = wifi::ipv4();
    printf!("Connected. local IP = {}.{}.{}.{}\n", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, ip >> 24);

    let l = io::listen(8080).ok().unwrap();
    loop {
        let conn = l.accept();
        if let Ok(c) = conn {
            c.write("hi");
            c.close();
        }
    };

    exit(0);
    
    loop { }
}
