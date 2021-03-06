/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

use base::col::DList;
use base::errors::{Code, Error};
use base::goff;
use base::util;
use core::fmt;

struct Area {
    addr: goff,
    size: usize,
}

impl Area {
    pub fn new(addr: goff, size: usize) -> Self {
        Area {
            addr: addr,
            size: size,
        }
    }
}

impl fmt::Debug for Area {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Area[addr={:#x}, size={:#x}]", self.addr, self.size)
    }
}

pub struct MemMap {
    areas: DList<Area>,
}

impl MemMap {
    pub fn new(addr: goff, size: usize) -> Self {
        let mut areas = DList::new();
        areas.push_back(Area::new(addr, size));
        MemMap {
            areas: areas,
        }
    }

    pub fn allocate(&mut self, size: usize, align: usize) -> Result<goff, Error> {
        // find an area with sufficient space
        let mut it = self.areas.iter_mut();
        let a: Option<&mut Area> = loop {
            match it.next() {
                None    => break None,
                Some(a) => {
                    let diff = util::round_up64(a.addr, align as goff) - a.addr;
                    if a.size > diff as usize && a.size - diff as usize >= size {
                        break Some(a)
                    }
                },
            }
        };

        match a {
            None    => Err(Error::new(Code::OutOfMem)),
            Some(a) => {
                // if we need to do some alignment, create a new area in front of a
                let diff = util::round_up64(a.addr, align as goff) - a.addr;
                if diff != 0 {
                    it.insert_before(Area::new(a.addr, diff as usize));
                    a.addr += diff;
                    a.size -= diff as usize;
                }

                // take it from the front
                let res = a.addr;
                a.size -= size;
                a.addr += size as goff;

                // if the area is empty now, remove it
                if a.size == 0 {
                    it.remove();
                }

                Ok(res)
            }
        }
    }

    pub fn free(&mut self, addr: goff, size: usize) {
        // find the area behind ours
        let mut it = self.areas.iter_mut();
        let n: Option<&mut Area> = loop {
            match it.next() {
                None    => break None,
                Some(n) => if addr <= n.addr { break Some(n) },
            }
        };

        let res = {
            let p: Option<&mut Area> = it.peek_prev();
            match (p, n) {
                // merge with prev and next
                (Some(ref mut p), Some(ref n)) if p.addr + p.size as goff == addr &&
                                                  addr + size as goff == n.addr => {
                    p.size += size + n.size;
                    1
                },

                // merge with prev
                (Some(ref mut p), _) if p.addr + p.size as goff == addr => {
                    p.size += size;
                    0
                }

                // merge with next
                (_, Some(ref mut n)) if addr + size as goff == n.addr => {
                    n.addr -= size as goff;
                    n.size += size;
                    0
                }

                (_, _) => 2,
            }
        };

        if res == 1 {
            it.remove();
        }
        else if res == 2 {
            it.insert_before(Area::new(addr, size));
        }
    }

    pub fn size(&self) -> (usize, usize) {
        let mut total = 0;
        for a in self.areas.iter() {
            total += a.size;
        }
        (total, self.areas.len())
    }
}

impl fmt::Debug for MemMap {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "[\n")?;
        for a in &self.areas {
            write!(f, "    {:?}\n", a)?;
        }
        write!(f, "  ]")
    }
}
