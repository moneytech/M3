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

#include <m3/cap/VPE.h>
#include <m3/RecvBuf.h>
#include <m3/ChanMng.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

namespace m3 {

void RecvBuf::RecvBufWorkItem::work() {
    ChanMng &mng = ChanMng::get();
    assert(_chanid != UNBOUND && mng.uses_header(_chanid));
    if(mng.fetch_msg(_chanid))
        mng.notify(_chanid);
}

void RecvBuf::attach(size_t i) {
    if(i != UNBOUND) {
        // first reserve the channel; we might need to invalidate it
        ChanMng::get().reserve(i);
        // TODO the kernel needs to do that
        DTU::get().set_receiving(i, reinterpret_cast<word_t>(_buf), _order, _msgorder,
            _flags & ~DELETE_BUF);

        if(coreid() != KERNEL_CORE && i > ChanMng::DEF_RECVCHAN) {
            // TODO let the user choose whether replies are allowed
            if(Syscalls::get().attachrb(VPE::self().sel(), i,
                reinterpret_cast<word_t>(_buf), (size_t)1 << _order, true) != Errors::NO_ERROR)
                PANIC("Attaching receive buffer to " << i << " failed: " << Errors::to_string(Errors::last));
        }

        // if we may receive messages from the channel, create a worker for it
        if(ChanMng::get().uses_header(i)) {
            if(_workitem == nullptr) {
                _workitem = new RecvBufWorkItem(i);
                WorkLoop::get().add(_workitem, i == ChanMng::MEM_CHAN || i == ChanMng::DEF_RECVCHAN);
            }
            else
                _workitem->chanid(i);
        }
    }
    // if it's UNBOUND now, don't use the worker again, if there is any
    else if(_workitem)
        detach();

    _chanid = i;
}

void RecvBuf::detach() {
    if(coreid() != KERNEL_CORE && _chanid > ChanMng::DEF_RECVCHAN && _chanid != UNBOUND) {
        Syscalls::get().detachrb(VPE::self().sel(), _chanid);
        _chanid = UNBOUND;
    }

    if(_workitem) {
        WorkLoop::get().remove(_workitem);
        delete _workitem;
        _workitem = nullptr;
    }
}

}