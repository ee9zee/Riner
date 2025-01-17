
#include "Pool.h"
#include "PoolEthash.h"
#include "PoolGrin.h"

namespace miner {

    /**
     * Register all Pool implementations here.
     * This is necessary so that the compiler includes the necessary code into a library
     * because only Pool is referenced by another compilation unit.
     */
    static const Pool::Registry<PoolEthashStratum> registryPoolEthashStratum {"EthashStratum2", HasPowTypeEthash::getPowType(), "stratum2"};
    static const Pool::Registry<PoolGrinStratum>   registryPoolGrinStratum   {"Cuckatoo31Stratum"  , HasPowTypeCuckatoo31::getPowType(), "GrinStratum", "stratum"};


    uint64_t Pool::createNewPoolUid() {
        static std::atomic<uint64_t> uid = {1};
        return uid.fetch_add(1);
    }

    void StillAliveTrackable::onStillAlive() {
        lastKnownAliveTime = clock::now();
    };

    StillAliveTrackable::clock::time_point
    StillAliveTrackable::getLastKnownAliveTime() {
        return lastKnownAliveTime;
    }

    void StillAliveTrackable::setLastKnownAliveTime(clock::time_point time) {
        lastKnownAliveTime = time;
    }

    unique_ptr<Pool> Pool::makePool(PoolConstructionArgs args, const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry.value().makeFunc(std::move(args));
        return nullptr;
    }

    unique_ptr<Pool> Pool::makePool(PoolConstructionArgs args, const std::string &powType, const std::string &protocolType) {
        for (const auto &entry : getEntries()) {
            bool hasMatchingName =
                    protocolType == entry.protocolType ||
                            (!entry.protocolType.empty() && protocolType == entry.protocolTypeAlias);

            if (hasMatchingName && entry.powType == powType)
                return entry.makeFunc(std::move(args));
        }
        return nullptr;
    }

    std::string Pool::getPoolImplNameForPowAndProtocolType(const std::string &powType, const std::string &protocolType) {
        for (const auto &entry : getEntries())
            if (entry.powType == powType && entry.protocolType == protocolType)
                return entry.poolImplName;
        return ""; //no matching poolImpl found
    }

    std::string Pool::getPowTypeForPoolImplName(const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry.value().powType;
        return ""; //no matching powType found
    }

    std::string Pool::getProtocolTypeForPoolImplName(const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry.value().protocolType;
        return ""; //no matching protocol type found
    }

    bool Pool::hasPowType(const std::string &powType) {
        for (const auto &entry : getEntries()) {
            if (entry.powType == powType) {
                return true;
            }
        }
        return false;
    }

    bool Pool::hasProtocolType(const std::string &protocolType) {
        for (const auto &entry : getEntries()) {
            if (entry.protocolType == protocolType ||
                (!entry.protocolType.empty() && protocolType == entry.protocolTypeAlias)) {
                return true;
            }
        }
        return false;
    }

    auto Pool::entryWithName(const std::string &poolImplName) -> optional_ref<Entry> {
        for (auto &entry : getEntries())
            if (entry.poolImplName == poolImplName)
                return type_safe::opt_ref(entry);
        return nullopt;
    }

}