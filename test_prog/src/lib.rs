#![no_std]
use core::arch::asm;
use core::fmt;

pub struct Printer;

impl fmt::Write for Printer {
    fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
        print(s);
        Ok(())
    }
}

/// Prints to the serial port, with a newline. Works just like the standard `println`.
#[macro_export]
macro_rules! printf {
    () => {
        print("");
    };
    ($e:expr) => {
        print($e);
    };
    ($fmt:expr, $($arg:tt)*) => {
        use core::fmt::Write;
        Printer.write_fmt(format_args!($fmt, $($arg)*)).ok();
    };
}



pub unsafe fn ecall(mut syscall_num: u32, mut a0: u32) -> Result<u32, u32> {
    unsafe {
        asm!(
            "ecall",
            inout("a0") a0,
            inout("a7") syscall_num,
        );
    }
    if syscall_num != 0 {
        Err(syscall_num)
    } else {
        Ok(a0)
    }
}

pub unsafe fn ecall2(mut syscall_num: u32, mut a0: u32, a1: u32) -> Result<u32, u32> {
    unsafe {
        asm!(
            "ecall",
            inout("a0") a0,
            in("a1") a1,
            inout("a7") syscall_num,
        );
    }
    if syscall_num != 0 {
        Err(syscall_num)
    } else {
        Ok(a0)
    }
}

pub unsafe fn ecall3(mut syscall_num: u32, mut a0: u32, a1: u32, a2: u32) -> Result<u32, u32> {
    unsafe {
        asm!(
            "ecall",
            inout("a0") a0,
            in("a1") a1,
            in("a2") a2,
            inout("a7") syscall_num,
        );
    }
    if syscall_num != 0 {
        Err(syscall_num)
    } else {
        Ok(a0)
    }
}

pub unsafe fn ecall5(mut syscall_num: u32, mut a0: u32, a1: u32, a2: u32, a3: u32, a4: u32) -> Result<u32, u32> {
    unsafe {
        asm!(
            "ecall",
            inout("a0") a0,
            in("a1") a1,
            in("a2") a2,
            in("a3") a3,
            in("a4") a4,
            inout("a7") syscall_num,
        );
    }
    if syscall_num != 0 {
        Err(syscall_num)
    } else {
        Ok(a0)
    }
}

pub fn print(l: &str) {
    unsafe {
        ecall2(1, l.as_ptr() as u32, l.len() as u32).ok();
    }
}

pub fn strcmp(l1: &str, l2: &str) -> i32 {
	let smallest_len = l1.len().min(l2.len());
    let result = unsafe {
        ecall3(2, l1.as_ptr() as u32, l2.as_ptr() as u32, smallest_len as u32).ok().unwrap()
    };
    result as i32
}

pub fn delay(ms: u32) {
    unsafe {
        ecall(0x10, ms).ok();
    }
}

pub fn exit(code: u32) {
    unsafe {
        ecall(0, code).ok();
    }
}

pub mod wifi {
    pub fn get_mode() -> u32 {
        unsafe {
            super::ecall(0x100, 0).ok().unwrap()
        }
    }

    pub fn set_mode(mode: u32) -> bool {
        let ret = unsafe {
            super::ecall(0x101, mode).ok().unwrap()
        };
        ret != 0
    }

    pub fn get_channel() -> u32 {
        unsafe {
            super::ecall(0x102, 0).ok().unwrap()
        }
    }

    pub fn is_connected() -> bool {
        let ret = unsafe {
            super::ecall(0x103, 0).ok().unwrap()
        };
        ret != 0
    }

    pub fn connect_as_station(ssid: &str, pwd: &str, channel: u32) -> bool {
        let ret = unsafe {
            super::ecall5(0x104,
                ssid.as_ptr() as u32,
                ssid.len() as u32,
                pwd.as_ptr() as u32,
                pwd.len() as u32,
                channel,
            )
        };
        ret.ok().unwrap() != 0
    }

    pub fn ipv4() -> u32 {
        unsafe {
            super::ecall(0x105, 0).ok().unwrap()
        }
    }

}

pub mod io {
    pub struct Fd(u8);

    impl Fd {
        pub fn write(&self, data: &str) -> Result<u32, ()> {
            match unsafe {
                super::ecall3(0x131, self.0 as u32, data.as_ptr() as u32, data.len() as u32)
            } {
                Ok(size) => Ok(size),
                Err(_) => Err(()),
            }
        }

        pub fn accept(&self) -> Result<Fd, ()> {
            match unsafe {
                super::ecall(0x141, self.0 as u32)
            } {
                Ok(fd) => if fd == u32::MAX {
                    Err(())
                } else {
                    Ok(Fd(fd as u8))
                },
                Err(_) => Err(()),
            }
        }

        pub fn close(self) {
            unsafe {
                super::ecall(0x130, self.0 as u32).ok(); // close
            }
        }
    }

    pub fn listen(port: u32) -> Result<Fd, ()> {
        match unsafe { super::ecall(0x140, port) } {
            Ok(fd) => Ok(Fd(fd as u8)),
            Err(_) => Err(()),
        }
    }
}