
#include "PoolEthash.h"
#include <src/pool/WorkEthash.h>
#include <src/pool/AutoRefillQueue.h>
#include <src/util/Logging.h>
#include <src/util/HexString.h>
#include <src/common/Json.h>
#include <src/common/Endian.h>
#include <chrono>
#include <random>
#include <functional>

#include <asio.hpp>
#include <src/common/Chrono.h>

namespace miner {

    const uint32_t WorkEthash::uniqueNonce{static_cast<uint32_t>(std::random_device()())};

    void PoolEthashStratum::onConnected(CxnHandle cxn) {
        LOG(DEBUG) << "onConnected";
        acceptMiningNotify = false;

        jrpc::Message subscribe = jrpc::RequestBuilder{}
            .id(io.nextId++)
            .method("mining.subscribe")
            .param("sgminer")
            .param("5.5.17-gm")
            .done();

        io.callAsync(cxn, subscribe, [this] (CxnHandle cxn, jrpc::Message response) {
            //this handler gets invoked when a jrpc response with the same id as 'subscribe' is received

            //return if it's not a {"result": true} message
            if (!response.isResultTrue()) {
                LOG(INFO) << "mining subscribe is not result true, instead it is " << response.str();
                return;
            }

            jrpc::Message authorize = jrpc::RequestBuilder{}
                .id(io.nextId++)
                .method("mining.authorize")
                .param(args.username)
                .param(args.password)
                .done();

            io.callAsync(cxn, authorize, [this] (CxnHandle cxn, jrpc::Message response) {
                acceptMiningNotify = true;
                _cxn = cxn; //store connection for submit
            });
        });

        io.addMethod("mining.notify", [this] (nl::json params) {
            if (acceptMiningNotify) {
                if (params.is_array() && params.size() >= 4)
                    onMiningNotify(params);
                else
                    throw jrpc::Error{jrpc::invalid_params, "expected at least 4 params"};
            }
        });

        io.setIncomingModifier([&] (jrpc::Message &msg) {
            //called everytime a jrpc::Message is incoming, so that it can be modified
            onStillAlive(); //update still alive timer

            //incoming mining.notify should be a notification (which means id = {}) but some pools
            //send it with a regular id. The io object will treat it like a notification once we
            //remove the id.
            if (msg.hasMethodName("mining.notify"))
                msg.id = {};
        });

        io.setReadAsyncLoopEnabled(true);
        io.readAsync(cxn); //start listening for incoming responses and rpcs from this cxn
    }

    void PoolEthashStratum::onMiningNotify(const nl::json &jparams) {
        bool cleanFlag = jparams.at(4);
        cleanFlag = true;
        if (cleanFlag)
            protocolDatas.clear(); //invalidates all gpu work that links to them

        //create work package
        protocolDatas.emplace_back(std::make_shared<EthashStratumProtocolData>(getPoolUid()));
        auto &sharedProtoData = protocolDatas.back();
        auto weakProtoData = make_weak(sharedProtoData);

        auto work = std::make_unique<WorkEthash>(weakProtoData);

        sharedProtoData->jobId = jparams.at(0);
        HexString(jparams[1]).getBytes(work->header);
        HexString(jparams[2]).getBytes(work->seedHash);
        HexString(jparams[3]).swapByteOrder().getBytes(work->target);

        //work->epoch is calculated in the refill thread

        workQueue->setMaster(std::move(work), cleanFlag);
    }

    void PoolEthashStratum::submitWorkImpl(unique_ptr<WorkSolution> resultBase) {

        auto result = static_unique_ptr_cast<WorkSolutionEthash>(std::move(resultBase));

        //build and send submitMessage on the tcp thread

        io.postAsync([this, result = std::move(result)] {

            auto protoData = result->tryGetProtocolDataAs<EthashStratumProtocolData>();
            if (!protoData) {
                LOG(INFO) << "work result cannot be submitted because it has expired";
                return; //work has expired
            }

            uint32_t shareId = io.nextId++;

            jrpc::Message submit = jrpc::RequestBuilder{}
                .id(shareId)
                .method("mining.submit")
                .param(args.username)
                .param(protoData->jobId)
                .param("0x" + HexString(toBytesWithBigEndian(result->nonce)).str()) //nonce must be big endian
                .param("0x" + HexString(result->header).str())
                .param("0x" + HexString(result->mixHash).str())
                .done();

            auto onResponse = [] (CxnHandle cxn, jrpc::Message response) {
                std::string acceptedStr = response.isResultTrue() ? "accepted" : "rejected";
                LOG(INFO) << "share with id '" << response.id << "' got " << acceptedStr;
            };

            //this handler gets called if there was no response after the last try
            auto onNeverResponded = [shareId] () {
                LOG(INFO) << "share with id " << shareId << " got discarded after pool did not respond multiple times";
            };

            io.callAsyncRetryNTimes(_cxn, submit, 5, seconds(5), onResponse, onNeverResponded);

        });
    }

    PoolEthashStratum::PoolEthashStratum(PoolConstructionArgs args)
    : args(args) {
        //initialize workQueue
        auto refillThreshold = 8; //once the queue size goes below this, lambda gets called

        auto refillFunc = [] (auto &out, auto &workMaster, size_t currentSize) {

            workMaster->setEpoch();

            for (auto i = currentSize; i < 16; ++i) {
                ++workMaster->extraNonce;
                auto newWork = std::make_unique<WorkEthash>(*workMaster);
                out.push_back(std::move(newWork));
            }
            LOG(INFO) << "workQueue got refilled from " << currentSize << " to " << currentSize + out.size() << " items";
        };

        workQueue = std::make_unique<WorkQueueType>(refillThreshold, refillFunc);


        io.launchClientAutoReconnect(args.host, args.port, [this] (auto cxn) {
            onConnected(cxn);
        });

    }

    PoolEthashStratum::~PoolEthashStratum() {
        shutdown = true;
    }

    optional<unique_ptr<Work>> PoolEthashStratum::tryGetWork() {
        if (auto work = workQueue->popWithTimeout(std::chrono::milliseconds(100)))
            return std::move(work.value()); //implicit unique_ptr upcast
        return nullopt; //timeout
    }

    uint64_t PoolEthashStratum::getPoolUid() const {
        return uid;
    }

    cstring_span PoolEthashStratum::getName() const {
        return args.host;
    }

}
