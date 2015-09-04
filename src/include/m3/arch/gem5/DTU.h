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
#include <m3/Config.h>
#include <m3/util/Util.h>
#include <assert.h>

#define DTU_PKG_SIZE        (static_cast<size_t>(8))

namespace m3 {

class DTU {
    explicit DTU() {
    }

public:
    typedef uint64_t reg_t;

    struct DtuRegs {
        reg_t status;
        reg_t msgCnt;
    } PACKED;

    struct CmdRegs {
        reg_t command;
        reg_t dataAddr;
        reg_t dataSize;
        reg_t offset;
        reg_t replyEpId;
        reg_t replyLabel;
    } PACKED;

    struct EpRegs {
        // for receiving messages
        reg_t bufAddr;
        reg_t bufMsgSize;
        reg_t bufSize;
        reg_t bufMsgCnt;
        reg_t bufReadPtr;
        reg_t bufWritePtr;
        // for sending messages
        reg_t targetCoreId;
        reg_t targetEpId;
        reg_t maxMsgSize;
        reg_t label;
        reg_t credits;
        // for memory requests
        reg_t reqRemoteAddr;
        reg_t reqRemoteSize;
        reg_t reqFlags;
    } PACKED;

    struct Header {
        uint8_t flags; // if bit 0 is set its a reply, if bit 1 is set we grant credits
        uint8_t senderCoreId;
        uint8_t senderEpId;
        uint8_t replyEpId; // for a normal message this is the reply epId
                           // for a reply this is the enpoint that receives credits
        uint16_t length;

        uint64_t label;
        uint64_t replylabel;
    } PACKED;

    struct Message : Header {
        int send_chanid() const {
            return senderEpId;
        }
        int reply_chanid() const {
            return replyEpId;
        }

        unsigned char data[];
    } PACKED;

    static const uintptr_t BASE_ADDR        = 0xF0000000;
    static const size_t HEADER_SIZE         = sizeof(Header);

    // TODO not yet supported
    static const int FLAG_NO_RINGBUF        = 0;
    static const int FLAG_NO_HEADER         = 0;

    enum MemFlags : reg_t {
        R                   = 1 << 0,
        W                   = 1 << 1,
    };

    enum StatusFlags : reg_t {
        BUSY                = 1 << 0,
        PRIV                = 1 << 0,
    };

    enum class CmdOpCode {
        IDLE                = 0,
        SEND                = 1,
        REPLY               = 2,
        READ                = 3,
        WRITE               = 4,
        INC_READ_PTR        = 5,
        WAKEUP_CORE         = 6,
    };

    static const int MEM_CHAN       = 0;    // unused
    static const int SYSC_CHAN      = 0;
    static const int DEF_RECVCHAN   = 1;

    static DTU &get() {
        return inst;
    }

    void configure(int ep, label_t label, int coreid, int epid, word_t credits) {
        EpRegs *e = ep_regs(ep);
        e->label = label;
        e->targetCoreId = coreid;
        e->targetEpId = epid;
        e->credits = credits;
        // TODO that's not correct
        e->maxMsgSize = credits;
    }

    void configure_recv(int ep, uintptr_t buf, uint order, uint msgorder, int) {
        EpRegs *e = ep_regs(ep);
        e->bufAddr = buf;
        e->bufReadPtr = buf;
        e->bufWritePtr = buf;
        e->bufSize = static_cast<size_t>(1) << (order - msgorder);
        e->bufMsgSize = static_cast<size_t>(1) << msgorder;
        e->bufMsgCnt = 0;
    }

    void configure_mem(int ep, int coreid, uintptr_t addr, size_t size) {
        EpRegs *e = ep_regs(ep);
        e->targetCoreId = coreid;
        e->reqRemoteAddr = addr;
        e->reqRemoteSize = size;
        e->reqFlags = R | W;
    }

    void send(int ep, const void *msg, size_t size, label_t replylbl, int reply_ep);
    void reply(int ep, const void *msg, size_t size, size_t msgidx);
    void read(int ep, void *msg, size_t size, size_t off);
    void write(int ep, const void *msg, size_t size, size_t off);

    void cmpxchg(UNUSED int ep, UNUSED const void *msg, UNUSED size_t msgsize, UNUSED size_t off, UNUSED size_t size) {
        // TODO unsupported
    }
    void sendcrd(UNUSED int ep, UNUSED int crdep, UNUSED size_t size) {
        // TODO unsupported
    }

    bool uses_header(int) {
        return true;
    }

    bool fetch_msg(int chan) {
        volatile EpRegs *ep = ep_regs(chan);
        return ep->bufMsgCnt > 0;
    }

    DTU::Message *message(int chan) const {
        return reinterpret_cast<Message*>(ep_regs(chan)->bufReadPtr);
    }
    Message *message_at(int, size_t) const {
        // TODO unsupported
        return nullptr;
    }

    size_t get_msgoff(int, RecvGate *) const {
        return 0;
    }
    size_t get_msgoff(int, RecvGate *, const Message *) const {
        // TODO unsupported
        return 0;
    }

    void ack_message(int chan) {
        CmdRegs *cmd = cmd_regs();
        cmd->command = buildCommand(chan, CmdOpCode::INC_READ_PTR);
    }

    bool wait() {
        // wait until the DTU wakes us up
        // note that we have a race-condition here. if a message arrives between the check and the
        // hlt, we miss it. this case is handled by a pin at the CPU, which indicates whether
        // unprocessed messages are available. if so, hlt does nothing. in this way, the ISA does
        // not have to be changed.
        volatile DtuRegs *regs = dtu_regs();
        if(regs->msgCnt == 0)
            asm volatile ("hlt");
        return true;
    }
    void wait_until_ready(int) {
        volatile DtuRegs *regs = dtu_regs();
        while(regs->status & BUSY)
            ;
    }
    bool wait_for_mem_cmd() {
        // we've already waited
        return true;
    }

    static DtuRegs *dtu_regs() {
        return reinterpret_cast<DtuRegs*>(BASE_ADDR);
    }
    static CmdRegs *cmd_regs() {
        return reinterpret_cast<CmdRegs*>(BASE_ADDR + sizeof(DtuRegs));
    }
    static EpRegs *ep_regs(int ep) {
        return reinterpret_cast<EpRegs*>(BASE_ADDR + sizeof(DtuRegs) + sizeof(CmdRegs) + ep * sizeof(EpRegs));
    }

    static reg_t buildCommand(int ep, CmdOpCode c) {
        return static_cast<uint>(c) | (ep << 3);
    }

private:
    static DTU inst;
};

}
