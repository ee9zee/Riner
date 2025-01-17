
#pragma once

#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <string>

namespace miner {

    /** @brief base class for pool protocol implementation specific data, e.g. stratum job id for stratum
     *  WorkProtocolData is attached to every Work object. Pool protocol implementations can subclass it to store
     *  any additional data that is not necessary for the POW algorithm but necessary for identifying which pool/work-package/etc. the work belongs to.
     *  When WorkSolutions are submitted to a Pool, they carry a weak_ptr to that same WorkProtocolData, allowing the pool to immediately read
     *  the necessary information for submission of the share.
     */
    class WorkProtocolData {
        uint64_t poolUid = 0; //used to determine whether a WorkResult is
        // returned to the same pool that created the work to begin with.
    public:
        WorkProtocolData(uint64_t poolUid) : poolUid(poolUid) {
        };

        /**
         * get the pool uid used for routing the Work to the correct pool protocol impl in case multiple pool connections of the same POWtype are open simultaneously
         * @return the runtime uid of a pool protocol impl
         */
        uint64_t getPoolUid() {
            return poolUid;
        }
    };

    /** @brief Solution counterpart of the Work class.
     * Every Work subclass is expected to have a corresponding WorkSolution subclass for a given POWtype.
     * WorkSolutions should be created via a Work's makeWorkSolution<T>() method, filled with the actual proof of work data and then submitted to the Pool
     */
    class WorkSolution {
        std::weak_ptr<WorkProtocolData> protocolData;

    protected:
        WorkSolution(std::weak_ptr<WorkProtocolData>, const std::string &powType);

    public:
        const std::string powType;

        /**
         * access type erased protocol data (use tryGetProtocolDataAs<T>() instead if you want to upcast it anyways)
         * @return type erased pool protocol specific data (such as stratum job id for stratum)
         */
        std::weak_ptr<WorkProtocolData> getProtocolData() const;

        /**
         * convenience function for getting the upcasted protocol data in the pool protocol implementation code
         * @tparam T type that the protocolData is supposed to be upcasted to
         * @return protocol data as T or nullptr iff work has expired
         */
        template<class T>
        std::shared_ptr<T> tryGetProtocolDataAs() const {
            if (auto pdata = protocolData.lock()) {
                return std::static_pointer_cast<T>(pdata);
            }
            return nullptr;
        }

        virtual ~WorkSolution() = default;
    };

    /** @brief base class for all classes representing a unit of work of a POWtype as provided by the pool protocol (e.g. see WorkEthash)
     * this unit of work does not necessarily correspond to e.g. a stratum job, it may be a work representation of smaller granularity.
     * The pool protocol implementation is incentivized to make this work of appropriate size (read: amount of work, not byte size) so that minimal overhead is
     * required on the AlgoImpl side to make use of it.
     * Data that is specific to Pool protocol implementations may not be added to a Work subclass, but rather to a WorkProtocolData subclass
     */
    class Work {
        /**
         * type erased protocol data which also works for finding out whether work has expired via weak_ptr's expired() functionality
         */
        std::weak_ptr<WorkProtocolData> protocolData;

    protected:
        Work(std::weak_ptr<WorkProtocolData>, const std::string &powType);

    public:
        const std::string powType;
        virtual ~Work() = default;

        /** @brief checks whether a solution to this work will still be accepted by the pool protocol.
         * expired() may be used as a hint to the algorithm that calculating solutions to this work package is not
         * beneficial anymore. Upon expiration it is expected (but not necessary) that an algorithm stops working on this work and
         * acquires fresh work from the pool.
         *
         * @return whether the work has been marked expired by the pool protocol implementation
         */
        bool expired() const { //thread safe
            //checks whether associated shared_ptr is still alive
            return protocolData.expired();
        }


        /**
         * @brief creates a solution object for this work object.
         * creates a solution object which can be filled with the algorithm results to then be submitted to the Pool.
         * There can be many WorkSolution objects generated from a single Work instance.
         * A solution object does not necessarily need to be submitted.
         * WorkSolution objects must be created through this function in order to be properly tied to a Work object (via the private protocolData member)
         * That way the pool protocol implementation can track which solution belongs to which work package.
         * A pool can make work objects expire (which can be queried via Work::expired()). This may be used as a hint to abort any calculations
         * and acquire a new work object, however it is not required to do so.
         * There is no obligation to check for expiration before submitting a solution
         *
         * @tparam WorkSolutionT Type of the Solution object that corresponds to this work's dynamic type (e.g. WorkSolutionEthash for WorkEthash).
         *
         * @return a WorkSolutionT wrapped in a unique_ptr in order to allow passing it later in a type erased fashion (as unique_ptr<WorkSolution>) without slicing
         */
        template<class WorkSolutionT>
        unique_ptr<WorkSolutionT> makeWorkSolution() const {
            return std::make_unique<WorkSolutionT>(protocolData);
        }
    };
}
