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

#include <m3/com/GateStream.h>
#include <m3/pipe/PipeWriter.h>

namespace m3 {

size_t PipeWriter::write(const void *buffer, size_t count) {
    if(_eof)
        return 0;

    assert((reinterpret_cast<uintptr_t>(buffer) & (DTU_PKG_SIZE - 1)) == 0);

    ssize_t rem = count;
    const char *buf = reinterpret_cast<const char*>(buffer);
    do {
        size_t aligned_amount = Math::round_up(rem, static_cast<ssize_t>(DTU_PKG_SIZE));
        size_t off = find_spot(&aligned_amount);
        if(_capacity == 0 || off == static_cast<size_t>(-1)) {
            size_t len;
            receive_vmsg(_rgate, len);
            DBG_PIPE("[write] got len=" << len << "\n");
            _rdpos = (_rdpos + len) % _size;
            _free += len;
            _capacity++;
            if(len == 0) {
                _eof |= Pipe::READ_EOF;
                return 0;
            }
            if(_capacity == 0 || off == static_cast<size_t>(-1)) {
                off = find_spot(&aligned_amount);
                if(off == static_cast<size_t>(-1))
                    return 0;
            }
        }

        size_t amount = Math::min(static_cast<ssize_t>(aligned_amount), rem);
        DBG_PIPE("[write] send pos=" << off << ", len=" << amount << "\n");

        if(aligned_amount) {
            _mgate.write_sync(buf, aligned_amount, off);
            _wrpos = (off + aligned_amount) % _size;
        }
        _free -= aligned_amount;
        _capacity--;
        send_vmsg(_sgate, off, amount);
        rem -= aligned_amount;
        buf += aligned_amount;
    }
    while(rem > 0);
    return buf - reinterpret_cast<const char*>(buffer);
}

size_t PipeWriter::find_spot(size_t *len) {
    if(_free == 0)
        return -1;
    if(_wrpos >= _rdpos) {
        if(_wrpos < _size) {
            *len = Math::min(*len, _size - _wrpos);
            return _wrpos;
        }
        if(_rdpos > 0) {
            *len = Math::min(*len, _rdpos);
            return 0;
        }
        return -1;
    }
    if(_rdpos - _wrpos > 0) {
        *len = Math::min(*len, _rdpos - _wrpos);
        return _wrpos;
    }
    return -1;
}

void PipeWriter::read_replies() {
    // read all expected responses
    if(~_eof & Pipe::READ_EOF) {
        size_t len = 1;
        int cap = Pipe::MSG_BUF_SIZE / Pipe::MSG_SIZE;
        while(len && _capacity < cap) {
            receive_vmsg(_rgate, len);
            DBG_PIPE("[shutdown] got len=" << len << "\n");
            _capacity++;
        }
    }
}

}
