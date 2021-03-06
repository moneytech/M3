/*
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
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

#include <base/Config.h>
#include <base/col/SList.h>

namespace kernel {

struct Timeout;
class VPE;

class ContextSwitcher {
    enum State {
        // normal state, no context switch happening
        S_IDLE,

        // inject IRQ to store state
        S_STORE_WAIT,

        // on rctmux's ACK: save DTU state
        S_STORE_DONE,

        // schedule and reset
        S_SWITCH,

        // wakeup to start application
        S_RESTORE_WAIT,

        // on rctmux's ACK: finished restore phase
        S_RESTORE_DONE
    };

    static const cycles_t MAX_WAIT_TIME     = 50000;
    static const cycles_t INIT_WAIT_TIME    = 1000;
    static const int SIGNAL_WAIT_COUNT      = 50;

public:
    explicit ContextSwitcher(peid_t pe);

    void init();

    peid_t pe() const {
        return _pe;
    }
    size_t count() const {
        return _count;
    }
    size_t ready() const {
        return _ready.length();
    }
    VPE *current() {
        return _cur;
    }
    size_t global_ready() {
        return _global_ready[_isa];
    }

    bool can_mux() const {
        return _muxable;
    }
    bool has_pinned() const {
        return _pinned > 0;
    }

    bool can_unblock_now(VPE *vpe);
    bool can_switch() const;

    void add_vpe(VPE *vpe);
    void remove_vpe(VPE *vpe, bool migrate = false);

    bool yield_vpe(VPE *vpe);
    bool unblock_vpe(VPE *vpe, bool force);

    void start_vpe(VPE *vpe);
    void stop_vpe(VPE *vpe, bool force = false, bool migrate = false);

    VPE *steal_vpe();

    void update_yield();

private:
    void schedule();

    void enqueue(VPE *vpe);
    void dequeue(VPE *vpe);

    bool current_is_idling() const;

    bool start_switch(bool timedout = false);
    void continue_switch();

    bool next_state(uint64_t flags);

private:
    bool _muxable;
    peid_t _pe;
    size_t _isa;
    State _state;
    size_t _count;
    size_t _pinned;
    m3::SList<VPE> _ready;
    Timeout *_timeout;
    cycles_t _wait_time;
    VPE *_idle;
    VPE *_cur;
    bool _set_yield;
    static size_t _global_ready[];
};

}
