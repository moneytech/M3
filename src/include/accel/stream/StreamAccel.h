/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#pragma once

#include <m3/VPE.h>

namespace accel {

/**
 * Base class for the stream accelerators
 */
class StreamAccel {
public:
    static const size_t RB_SIZE     = 64;
    static const capsel_t RBUF_SEL  = 2;
    static const size_t EP_RECV     = 7;
    static const size_t EP_INPUT    = 8;
    static const size_t EP_OUTPUT   = 9;

    static const size_t BUF_SIZE;
    static const size_t BUF_ADDR;

    /**
     * The format of the request
     */
    struct Request {
        uint64_t inoff;
        uint64_t outoff;
        uint64_t len;
        uint64_t autonomous;
    } PACKED;

    /**
     * Creates an accelerator, depending on which exists
     *
     * @param isa the ISA (fft, toupper)
     * @return the accelerator
     */
    static StreamAccel *create(m3::PEISA isa);

    virtual ~StreamAccel() {
    }

    /**
     * @return the VPE for the accelerator
     */
    virtual m3::VPE &vpe() = 0;
    /**
     * @return the address of the receive buffer
     */
    virtual uintptr_t getRBAddr() = 0;
};

/**
 * A stream accelerator with SPM, i.e., internal memory.
 */
class StreamIAccel : public StreamAccel {
public:
    explicit StreamIAccel(m3::PEISA isa, bool muxable);

    m3::VPE &vpe() override {
        return _vpe;
    }
    uintptr_t getRBAddr() override;

private:
    m3::VPE _vpe;
};

/**
 * A stream accelerator with cache, i.e., external memory.
 */
class StreamEAccel : public StreamAccel {
public:
    explicit StreamEAccel(m3::PEISA isa, bool muxable);

    m3::VPE &vpe() override {
        return _vpe;
    }
    uintptr_t getRBAddr() override;

private:
    m3::VPE _vpe;
};

}