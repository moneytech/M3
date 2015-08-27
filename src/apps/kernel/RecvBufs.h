/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#pragma once

#include <m3/Common.h>
#include <m3/util/Math.h>
#include <m3/Subscriber.h>
#include <m3/Config.h>
#include <m3/Errors.h>

namespace m3 {

class RecvBufs {
    RecvBufs() = delete;

    enum Flags {
        F_ATTACHED  = 1 << 0,
        F_REPLIES   = 1 << 1,
    };

    struct RBuf {
        uintptr_t addr;
        size_t size;
        int flags;
        Subscriptions<bool> waitlist;
    };

public:
    static bool is_attached(size_t coreid, size_t chanid) {
        RBuf &rbuf = get(coreid, chanid);
        return rbuf.flags & F_ATTACHED;
    }

    static void subscribe(size_t coreid, size_t chanid, const Subscriptions<bool>::callback_type &cb) {
        RBuf &rbuf = get(coreid, chanid);
        assert(~rbuf.flags & F_ATTACHED);
        rbuf.waitlist.subscribe(cb);
    }

    static Errors::Code attach(size_t coreid, size_t chanid, uintptr_t addr, size_t size, bool replies) {
        RBuf &rbuf = get(coreid, chanid);
        if(rbuf.flags & F_ATTACHED)
            return Errors::EXISTS;
        if(replies) {
            for(size_t i = 0; i < CHAN_COUNT; ++i) {
                if(i != chanid) {
                    RBuf &rb = get(coreid, i);
                    if((rb.flags & F_ATTACHED) &&
                        Math::overlap(rb.addr, rb.addr + rb.size, addr, addr + size))
                        return Errors::INV_ARGS;
                }
            }
        }

        rbuf.addr = addr;
        rbuf.size = size;
        rbuf.flags = replies ? F_REPLIES | F_ATTACHED : F_ATTACHED;
        notify(rbuf, true);
        return Errors::NO_ERROR;
    }

    static void detach(size_t coreid, size_t chanid) {
        RBuf &rbuf = get(coreid, chanid);
        rbuf.flags = 0;
        notify(rbuf, false);
    }

private:
    static void notify(RBuf &rbuf, bool success) {
        for(auto sub = rbuf.waitlist.begin(); sub != rbuf.waitlist.end(); ) {
            auto old = sub++;
            old->callback(success, nullptr);
            rbuf.waitlist.unsubscribe(&*old);
        }
    }
    static RBuf &get(size_t coreid, size_t chanid) {
        return _rbufs[(coreid - APP_CORES) * CHAN_COUNT + chanid];
    }

    static RBuf _rbufs[AVAIL_PES * CHAN_COUNT];
};

}