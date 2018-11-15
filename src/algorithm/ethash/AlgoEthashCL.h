
#pragma once

#include <future>
#include <atomic>
#include <vector>
#include <src/algorithm/Algorithm.h>
#include <src/pool/Work.h>
#include <src/algorithm/ethash/DagCache.h>
#include <src/algorithm/ethash/DagFile.h>
#include <src/util/LockUtils.h>

#include <mutex>

namespace miner {

    class AlgoEthashCL : AlgoBase {

        LockGuarded<DagCacheContainer> dagCaches;
        WorkProvider &pool;

        std::atomic<bool> shutdown {false};

        //submit tasks are potentially pushed from several threads, therefore LockGuarded
        LockGuarded<std::vector<std::future<void>>> submitTasks;

        std::vector<std::future<void>> gpuTasks; //one task per gpu

        //gets called once for each gpu
        void gpuTask(cl::Device clDevice);

        //gets called numGpuSubTasks times from each gpuTask
        void gpuSubTask(const cl::Device &clDevice, DagFile &dag);

        //gets called by gpuSubTask for each result nonce
        void submitShareTask(std::shared_ptr<const Work<kEthash>> work, std::vector<uint64_t> resultNonces);

        //returns possible solution nonces
        std::vector<uint64_t> runKernel(const cl::Device &, const Work<kEthash> &,
                uint64_t nonceBegin, uint64_t nonceEnd);

    public:
        explicit AlgoEthashCL(AlgoConstructionArgs);

        //destructor shuts down all working threads and joins them
        ~AlgoEthashCL();

    };

}