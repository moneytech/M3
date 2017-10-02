#![no_std]

#[macro_use]
extern crate m3;

use m3::syscalls;
use m3::time;
use m3::env;

#[no_mangle]
pub fn main() -> i32 {
    for (i, a) in env::args().enumerate() {
        println!("arg {}: {}", i, a);
    }

    {
        let res = syscalls::create_mgate(5, 0, 0x1000, 0x3);
        println!("res: {:?}", res);
    }

    {
        let res = syscalls::create_mgate(5, 0, 0x1000, 0x3);
        println!("res: {:?}", res);
    }

    let mut total = 0;
    for _ in 0..10 {
        let start = time::start(0);
        syscalls::noop().unwrap();
        let end = time::stop(0);
        total += end - start;
    }

    println!("per call: {}", total / 10);

    0
}
