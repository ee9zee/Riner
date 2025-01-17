#include "AlgoCuckatoo31Cl.h"

#include <src/common/Endian.h>
#include <src/common/OpenCL.h>
#include <src/crypto/blake2.h>
#include <src/pool/WorkCuckoo.h>

namespace miner {

AlgoCuckatoo31Cl::AlgoCuckatoo31Cl(AlgoConstructionArgs args) :
        terminate_(false), args_(std::move(args)) {

    for (auto &assignedDeviceRef : args_.assignedDevices) {
        auto &assignedDevice = assignedDeviceRef.get();

        optional<cl::Device> deviceOr = args.compute.getDeviceOpenCL(assignedDevice.id);
        if (!deviceOr) {
            continue;
        }
        cl::Device device = deviceOr.value();
        workers_.emplace_back([this, &assignedDevice, device] {
            cl::Context context(device);
            CuckatooSolver::Options opts {assignedDevice};
            opts.n = 31;
            opts.context = context;
            opts.device = device;
            opts.programLoader = &args_.compute.getProgramLoaderOpenCL();

            CuckatooSolver solver(std::move(opts));
            run(context, solver);
        });
    }
}

AlgoCuckatoo31Cl::~AlgoCuckatoo31Cl() {
    terminate_ = true;
    for (auto& worker : workers_) {
        worker.join();
    }
}

/* static */SiphashKeys AlgoCuckatoo31Cl::calculateKeys(const WorkCuckatoo31& header) {
    SiphashKeys keys;
    std::vector<uint8_t> h = header.prePow;
    uint64_t nonce = header.nonce;
    VLOG(0) << "nonce = " << nonce;
    size_t len = h.size();
    h.resize(len + sizeof(nonce));
    *reinterpret_cast<uint64_t*>(&h[len]) = htobe64(nonce);
    uint64_t keyArray[4];
    blake2b(keyArray, sizeof(keyArray), h.data(), h.size(), 0, 0);
    keys.k0 = htole64(keyArray[0]);
    keys.k1 = htole64(keyArray[1]);
    keys.k2 = htole64(keyArray[2]);
    keys.k3 = htole64(keyArray[3]);
    return keys;
}

void AlgoCuckatoo31Cl::run(cl::Context& context, CuckatooSolver& solver) {
    while (!terminate_) {
        auto work = args_.workProvider.tryGetWork<WorkCuckatoo31>().value_or(nullptr);
        if (!work) {
            continue;
        }

        std::vector<CuckatooSolver::Cycle> cycles = solver.solve(calculateKeys(*work), [&work]() {
            return work->expired();
        });
        LOG(INFO)<< "Found " << cycles.size() << " cycles of target length.";
        // TODO sha pow and compare to difficulty
        for (auto& cycle : cycles) {
            auto pow = work->makeWorkSolution<WorkSolutionCuckatoo31>();
            pow->height = work->height;
            pow->nonce = work->nonce;
            pow->pow = std::move(cycle.edges);
            args_.workProvider.submitWork(std::move(pow));
        }
    }
}

} /* namespace miner */
