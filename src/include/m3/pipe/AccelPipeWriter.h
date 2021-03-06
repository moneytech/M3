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

#pragma once

#if 0
#include <base/Common.h>
#include <base/log/Lib.h>

#include <m3/com/GateStream.h>
#include <m3/pipe/AccelPipeState.h>
#include <m3/vfs/File.h>

#include <accel/stream/StreamAccel.h>

namespace m3 {

/**
 * The writing end of an accelerator pipe, i.e., we write to an accelerator.
 */
class AccelPipeWriter : public File {
    static const size_t BUF_SIZE = 8192;

public:
    explicit AccelPipeWriter()
        : _sgate(SendGate::bind(accel::StreamAccel::SGATE_SEL)),
          _mgate(MemGate::bind(ObjCap::INVALID)),
          _agg() {
        // gates are already activated
        _sgate.ep(accel::StreamAccel::EP_SEND);
        _mgate.ep(accel::StreamAccel::EP_OUTPUT);
    }
    ~AccelPipeWriter() {
        notify_next(0, true);
    }

    virtual Errors::Code stat(FileInfo &) override {
        // not supported
        return Errors::NOT_SUP;
    }
    virtual ssize_t seek(size_t, int) override {
        // not supported
        return Errors::NOT_SUP;
    }

    virtual ssize_t read(void *, size_t) override {
        // not supported
        return 0;
    }

    virtual ssize_t write(const void *buffer, size_t size) override {
        using namespace accel;

        auto state = AccelPipeState::get();

        LLOG(ACCEL, "AccelPipeWriter: write(" << size << ")");

        _mgate.write(buffer, size, _off);
        _agg += size;

        bool eof = state->pos == state->len && state->eof;
        if(eof || _agg >= state->report_size) {
            notify_next(size, eof);
            _agg = 0;
        }

        if(eof)
            _off = 0;
        else
            _off = (_off + size) % state->out_size;

        return static_cast<ssize_t>(size);
    }

    virtual bool needs_flush() override {
        // this is required here, because we stream the data through the whole pipeline in steps.
        // if one step is finished, we receive EOF from our predecessor. in this case, we have to
        // notify our successor (if we're done with the data). if we are using a buffer for this
        // file, we need to make sure that write() is actually called to notify the successor.
        auto state = AccelPipeState::get();
        return state->eof && state->pos == state->len;
    }

    virtual char type() const override {
        return 'B';
    }
    virtual void delegate(VPE &) override {
    }
    virtual void serialize(Marshaller &) override {
    }
    static File *unserialize(Unmarshaller &) {
        return new AccelPipeWriter();
    }

private:
    void notify_next(size_t size, bool eof) {
        accel::StreamAccel::UpdateCommand updcmd;
        updcmd.cmd = static_cast<uint64_t>(accel::StreamAccel::Command::UPDATE);
        updcmd.off = _off + size - _agg;
        updcmd.len = _agg;
        updcmd.eof = eof;
        LLOG(ACCEL, "AccelPipeWriter: sending update(off="
            << updcmd.off << ", len=" << updcmd.len << ", eof=" << eof << ")");
        send_receive_msg(_sgate, &updcmd, sizeof(updcmd));
    }

    SendGate _sgate;
    MemGate _mgate;
    size_t _off;
    size_t _agg;
};

}
#endif
