/*
 * Copyright (C) 2018, Sebastian Reimers <sebastian.reimers@mailbox.tu-dresden.de>
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

#include <fs/internal.h>

#include "Buffer.h"

struct InodeExt : public m3::DListItem {
    m3::blockno_t _start;
    size_t _size;

    explicit InodeExt(m3::blockno_t start, size_t size)
        : m3::DListItem(),
          _start(start),
          _size(size) {
    }
};

class FileBufferHead : public BufferHead {
    friend class FileBuffer;

public:
    explicit FileBufferHead(m3::blockno_t bno, size_t size, size_t blocksize);

private:
    m3::MemGate _data;
    m3::DList<InodeExt> _extents;
};

class FileBuffer : public Buffer {
    static constexpr size_t FILE_BUFFER_SIZE    = 16384; // at least 128
    static constexpr size_t LOAD_LIMIT          = 128;

public:
    explicit FileBuffer(size_t blocksize, Backend *backend, size_t max_load);

    size_t get_extent(m3::blockno_t bno, size_t size, capsel_t sel, int perms, size_t accessed,
                      bool load = true, bool dirty = false);
    void flush() override;

private:
    FileBufferHead *get(m3::blockno_t bno) override;
    void flush_chunk(BufferHead *b) override;

    size_t _size;
    size_t _max_load;
};
