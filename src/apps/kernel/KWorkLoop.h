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

#include <m3/tracing/Tracing.h>
#include <m3/Common.h>
#include <m3/WorkLoop.h>

#include "SyscallHandler.h"

#if defined(__host__)
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>

static int sigchilds = 0;

static void sigchild(int) {
    sigchilds++;
    signal(SIGCHLD, sigchild);
}

static void check_childs() {
    for(; sigchilds > 0; sigchilds--) {
        int status;
        pid_t pid = wait(&status);
        if(WIFEXITED(status)) {
            LOG(DEF, "Child " << pid << " exited with status " << WEXITSTATUS(status));
        }
        else if(WIFSIGNALED(status)) {
            LOG(DEF, "Child " << pid << " was killed by signal " << WTERMSIG(status));
            if(WCOREDUMP(status))
                LOG(DEF, "Child " << pid << " core dumped");
        }
    }
}
#endif

namespace kernel {

class KWorkLoop : public m3::WorkLoop {
public:
    virtual void run() override {
#if defined(__host__)
        signal(SIGCHLD, sigchild);
#endif
        EVENT_TRACER_KWorkLoop_run();

        m3::DTU &dtu = m3::DTU::get();
        SyscallHandler &sysch = SyscallHandler::get();
        int sysep = sysch.epid();
        int srvep = sysch.srvepid();
        while(has_items()) {
            m3::DTU::get().wait();

            if(dtu.fetch_msg(sysep)) {
                // we know the subscriber here, so optimize that a bit
                m3::DTU::Message *msg = dtu.message(sysep);
                m3::RecvGate *rgate = reinterpret_cast<m3::RecvGate*>(msg->label);
                sysch.handle_message(*rgate, nullptr);
                dtu.ack_message(sysep);
                EVENT_TRACE_FLUSH_LIGHT();
            }
            if(dtu.fetch_msg(srvep)) {
                m3::DTU::Message *msg = dtu.message(srvep);
                m3::RecvGate *gate = reinterpret_cast<m3::RecvGate*>(msg->label);
                gate->notify_all();
                dtu.ack_message(srvep);
            }

#if defined(__host__)
            check_childs();
#endif
        }
    }
};

}
