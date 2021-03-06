/*
 * Copyright (C) 2017-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Common.h>
#include <base/stream/IStringStream.h>
#include <base/util/Time.h>
#include <base/PEDesc.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/AccelPipeReader.h>
#include <m3/pipe/AccelPipeWriter.h>
#include <m3/session/Pager.h>
#include <m3/vfs/VFS.h>

#include <accel/stream/StreamAccel.h>

#include "swfilter.h"

using namespace m3;
using namespace accel;

static const cycles_t FFT1D_CYCLES      = 16942 / 2; // for 2k

static Errors::Code execute(size_t arrsize, RecvGate &rgate, StreamAccel::ChainMember **chain) {
    SendGate sgate = SendGate::create(&chain[0]->rgate);
    StreamAccel::update(sgate, rgate, 0, arrsize);

    return Errors::NONE;
}

static Errors::Code execute_indirect(char *buffer, size_t arrsize, RecvGate &rgate,
                                     StreamAccel::ChainMember **chain, size_t num, size_t bufsize) {
    Errors::Code err = Errors::NONE;

    SendGate **sgates = new SendGate*[num];
    for(size_t i = 0; i < num; ++i)
        sgates[i] = new SendGate(SendGate::create(&chain[i]->rgate));

    MemGate buf1 = chain[0]->vpe->mem().derive(StreamAccel::BUF_ADDR, StreamAccel::BUF_SIZE);
    MemGate bufn = chain[num - 1]->vpe->mem().derive(StreamAccel::BUF_ADDR, StreamAccel::BUF_SIZE);

    size_t total = 0, seen = 0;
    buf1.write(buffer, bufsize, 0);
    StreamAccel::sendUpdate(*sgates[0], 0, bufsize);
    total += bufsize;

    while(seen < total) {
        GateIStream is = receive_msg(rgate);
        label_t label = is.label<label_t>();

        // cout << "got msg from " << label << "\n";

        if(label == num - 1) {
            auto *upd = reinterpret_cast<const StreamAccel::UpdateCommand*>(is.message().data);
            bufn.read(buffer + seen, upd->len, 0);
            // cout << "write " << upd->len << " bytes\n";
            seen += upd->len;
        }

        if(label == 0) {
            if(num > 1)
                send_msg(*sgates[1], is.message().data, is.message().length);

            if(total < arrsize) {
                buf1.write(buffer + total, bufsize, 0);
                StreamAccel::sendUpdate(*sgates[0], 0, bufsize);
                total += bufsize;
            }
        }
        else if(label != num - 1)
            send_msg(*sgates[label + 1], is.message().data, is.message().length);

        // cout << seen << " / " << total << "\n";
    }

    for(size_t i = 0; i < num; ++i)
        delete sgates[i];
    delete[] sgates;
    return err;
}

static void execchain(size_t arrsize, size_t bufsize, bool direct) {
    RecvGate rgate = RecvGate::create(nextlog2<8 * 64>::val, nextlog2<64>::val);
    rgate.activate();

    enum {
        IFFT    = 2,
        MUL     = 1,
        SFFT    = 0,
    };

    goff_t buffer = 0x20000000;
    if(VPE::self().pager()->map_anon(&buffer, arrsize, Pager::RW, Pager::MAP_ANON) != Errors::NONE)
        exitmsg("Unable to map buffer");

    memset((void*)buffer, 0 , arrsize);

    StreamAccel::ChainMember *chain[3];

    // IFFT
    auto ifft = StreamAccel::create(PEISA::ACCEL_FFT, true);
    chain[IFFT] = new StreamAccel::ChainMember(ifft, StreamAccel::getRBAddr(*ifft),
                                               StreamAccel::RB_SIZE, rgate, IFFT);

    // multiplier
    auto mul = new VPE("mul", PEDesc(PEType::COMP_IMEM, VPE::self().pe().isa()));
    mul->fds(*VPE::self().fds());
    mul->obtain_fds();
    chain[MUL] = new StreamAccel::ChainMember(mul, 0, StreamAccel::MSG_SIZE * 16,
                                              direct ? chain[IFFT]->rgate : rgate, MUL);

    // SFFT
    auto sfft = StreamAccel::create(PEISA::ACCEL_FFT, true);
    chain[SFFT] = new StreamAccel::ChainMember(sfft, StreamAccel::getRBAddr(*sfft),
                                               StreamAccel::RB_SIZE,
                                               direct ? chain[MUL]->rgate : rgate, SFFT);

    for(auto *m : chain) {
        m->send_caps();
        m->activate_recv();
    }

    for(size_t i = 0; i < ARRAY_SIZE(chain); ++i) {
        if(i != MUL)
            chain[i]->vpe->start();
        else {
            chain[i]->vpe->fds()->set(STDIN_FD, new AccelPipeReader());
            chain[i]->vpe->fds()->set(STDOUT_FD, new AccelPipeWriter());
            chain[i]->vpe->obtain_fds();

            const char *args[] = {"/bin/swfilter"};
            chain[i]->vpe->exec(ARRAY_SIZE(args), args);
        }
        chain[i]->activate_send();
    }

    MemGate sfftin = VPE::self().mem().derive(buffer, arrsize, MemGate::R);
    sfftin.activate_for(*chain[SFFT]->vpe, StreamAccel::EP_INPUT);

    MemGate sfftout = chain[MUL]->vpe->mem().derive(SWFIL_BUF_ADDR, SWFIL_BUF_SIZE);
    sfftout.activate_for(*chain[SFFT]->vpe, StreamAccel::EP_OUTPUT);

    MemGate mulout = chain[IFFT]->vpe->mem().derive(StreamAccel::BUF_ADDR, bufsize);
    mulout.activate_for(*chain[MUL]->vpe, StreamAccel::EP_OUTPUT);

    MemGate ifftout = VPE::self().mem().derive(buffer, arrsize, MemGate::W);
    ifftout.activate_for(*chain[IFFT]->vpe, StreamAccel::EP_OUTPUT);

    Errors::Code res;
    if(direct) {
        chain[SFFT]->init(bufsize, bufsize, FFT1D_CYCLES);
        chain[MUL]->init(bufsize, bufsize / 2, FFT1D_CYCLES);
        chain[IFFT]->init(static_cast<size_t>(-1), static_cast<size_t>(-1), FFT1D_CYCLES);

        cycles_t start = Time::start(1);
        res = execute(arrsize, rgate, chain);
        cycles_t end = Time::stop(1);
        cout << "Exec time: " << (end - start) << " cycles\n";
    }
    else {
        chain[SFFT]->init(bufsize, bufsize, FFT1D_CYCLES);
        chain[MUL]->init(bufsize, bufsize, FFT1D_CYCLES);
        chain[IFFT]->init(bufsize, bufsize, FFT1D_CYCLES);

        cycles_t start = Time::start(1);
        res = execute_indirect((char*)buffer, arrsize, rgate, chain, 3, bufsize);
        cycles_t end = Time::stop(1);
        cout << "Exec time: " << (end - start) << " cycles\n";
    }
    if(res != Errors::NONE)
        errmsg("Operation failed: " << Errors::to_string(res));

    for(auto *c : chain)
        delete c;
}

int main(int argc, char **argv) {
    if(argc < 4)
        exitmsg("Usage: " << argv[0] << " <direct> <arrsize> <bufsize>");

    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    bool direct = IStringStream::read_from<int>(argv[1]);
    size_t arrsize = IStringStream::read_from<size_t>(argv[2]);
    size_t bufsize = IStringStream::read_from<size_t>(argv[3]);

    cycles_t start = Time::start(0);
    execchain(arrsize, bufsize, direct);
    cycles_t end = Time::stop(0);

    cout << "Total time: " << (end - start) << " cycles\n";
    return 0;
}
