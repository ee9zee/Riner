#pragma once

#include <list>
#include <string>
#include <src/common/StringSpan.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/JsonForward.h>

namespace miner {

    class Config {
    public:

        Config() {}; //invalid config
        explicit Config(const std::string &configStr);
        explicit Config(const nl::json &configJson);

        struct GlobalSettings {
            optional<int>
                    temp_cutoff,
                    temp_overheat,
                    temp_target;
            uint32_t temp_hysteresis;

            uint16_t api_port;

            std::string opencl_kernel_dir;
            std::string start_profile;
        };

        struct Pool {
            std::string powType; //proof of work type: e.g. "ethash"
            std::string protocolType; //e.g. "EthashStratum3", can also be an alias as defined in Pool::Entry

            std::string host;
            uint16_t port;
            std::string username, password;
        };

        struct DeviceProfile {
            std::string name;

            struct GpuSettings {
                optional<uint32_t>
                        core_clock_MHz_min, //engine_min
                        core_clock_MHz_max, //engine_max
                        core_clock_MHz,
                        memory_clock_MHz,
                        power_limit_W,
                        core_voltage_mV,
                        core_voltage_offset_mV;
            };

            struct AlgoSettings {
                std::string algoImplName; //e.g. "AlgoEthashCL"

                GpuSettings gpuSettings;

                uint32_t
                        num_threads,
                        work_size,
                        raw_intensity;
            };

            std::list<AlgoSettings> algoSettings;

            optional_ref<const AlgoSettings> getAlgoSettings(const std::string &algoImplName) const;
        };

        struct Profile {

            struct Mapping {
                std::string deviceProfileName;
                std::string algoImplName;
            };

            std::string name;
            std::map<size_t, Mapping> devices_by_index;
            Mapping device_default;
        };

        optional_ref<const DeviceProfile> getDeviceProfile(const std::string &name) const;
        optional_ref<Profile> getProfile(const std::string &name);
        optional_ref<Profile> getStartProfile();

        operator bool() {return valid;}

        const std::list<Pool> &getPools() const;

    private:
        void tryParse(const nl::json &j); //calls parse(j) within a try block
        void parse(const nl::json &j);

        GlobalSettings globalSettings;
    public:
        const GlobalSettings &getGlobalSettings() const;

    private:

        bool valid = false;

        std::list<DeviceProfile> deviceProfiles;
        std::list<Profile> profiles;
        std::list<Pool> pools;
    };

}
