#ifndef SARD_NN_PAPER_PROFILER_H
#define SARD_NN_PAPER_PROFILER_H

#include <sys/resource.h>

#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

class Profiler {
   private:
    std::string blockName;
    std::string logFilename;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    struct rusage startUsage;

    unsigned int sampleIntervalUs;
    std::atomic<bool> stopSampling{false};
    std::atomic<long> blockBaseKb{0};
    std::atomic<long> blockMinKb{0};
    std::atomic<long> blockPeakKb{0};
    std::thread samplerThread;

    // Read the current VmRSS (in kB) of this process. VmRSS is a per-process
    // metric, so it includes the resident memory of all threads (OpenMP/HE workers).
    static long readVmRssKb() {
        std::ifstream f("/proc/self/status");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                std::istringstream iss(line.substr(6));
                long kb = 0;
                iss >> kb;
                return kb;
            }
        }
        return 0;
    }

   public:
    Profiler(const std::string& name, const std::string& filename, unsigned int intervalUs = 1000)
        : blockName(name), logFilename(filename), sampleIntervalUs(intervalUs) {
        startTime = std::chrono::high_resolution_clock::now();
        getrusage(RUSAGE_SELF, &startUsage);

        blockBaseKb.store(readVmRssKb());
        blockMinKb.store(blockBaseKb.load());
        blockPeakKb.store(blockBaseKb.load());

        samplerThread = std::thread([this] {
            while (!stopSampling.load()) {
                long kb = readVmRssKb();

                long curMin = blockMinKb.load();
                while (kb < curMin && !blockMinKb.compare_exchange_weak(curMin, kb)) {
                }

                long curPeak = blockPeakKb.load();
                while (kb > curPeak && !blockPeakKb.compare_exchange_weak(curPeak, kb)) {
                }

                std::this_thread::sleep_for(std::chrono::microseconds(sampleIntervalUs));
            }
        });
    }

    ~Profiler() {
        stopSampling = true;
        if (samplerThread.joinable()) {
            samplerThread.join();
        }

        struct rusage end_usage;
        getrusage(RUSAGE_SELF, &end_usage);

        auto end_time = std::chrono::high_resolution_clock::now();

        auto start = std::chrono::time_point_cast<std::chrono::microseconds>(startTime).time_since_epoch().count();
        auto end = std::chrono::time_point_cast<std::chrono::microseconds>(end_time).time_since_epoch().count();
        auto wall_duration = end - start;

        // rusage saves the time in seconds and microseconds -> convert to microseconds to allow for easier calculations
        int64_t cpu_time_start = startUsage.ru_utime.tv_sec * 1000000L + startUsage.ru_utime.tv_usec;
        int64_t cpu_time_end = end_usage.ru_utime.tv_sec * 1000000L + end_usage.ru_utime.tv_usec;
        int64_t cpu_duration = cpu_time_end - cpu_time_start;

        int64_t sys_time_start = startUsage.ru_stime.tv_sec * 1000000L +
                                 startUsage.ru_stime.tv_usec;  // System time to check if we see overhead patterns for ciphertext movements within RAM etc.
        int64_t sys_time_end = end_usage.ru_stime.tv_sec * 1000000L + end_usage.ru_stime.tv_usec;
        int64_t sys_duration = sys_time_end - sys_time_start;

        int64_t minflt_duration = end_usage.ru_minflt - startUsage.ru_minflt;  // Minor page faults in RAM indicate bad memory allocation for ciphertexts
        int64_t nvcsw_duration = end_usage.ru_nvcsw - startUsage.ru_nvcsw;     // Voluntary context switches; high -> high blocking
        int64_t nivcsw_duration = end_usage.ru_nivcsw - startUsage.ru_nivcsw;  // Involuntary context switches; high -> too many active threads

        int64_t base_ram = blockBaseKb.load();
        int64_t min_ram = blockMinKb.load();
        int64_t peak_ram = blockPeakKb.load();

        auto current_time = std::chrono::system_clock::now();
        std::time_t current_time_c = std::chrono::system_clock::to_time_t(current_time);
        std::tm* local_time = std::localtime(&current_time_c);

        const std::string log_dir = "results/profiler_results";
        std::filesystem::create_directories(log_dir);

        if (std::ofstream log_file(log_dir + "/" + logFilename, std::ios_base::app); log_file.is_open()) {
            // Move the put pointer to the end to check file size
            log_file.seekp(0, std::ios::end);

            // If the file is completely empty, insert the header row
            if (log_file.tellp() == 0) {
                log_file
                    << "Timestamp,BlockName,WallTime_us,CPUTime_us,BaseRAM_KB,MinRAM_KB,PeakRAM_KB,SysTime_us,MinPageFaults,VolCtxSwitches,InvolCtxSwitches\n";
            }

            // Write the comma-separated data row
            log_file << std::put_time(local_time, "%Y-%m-%d %H:%M:%S") << "," << blockName << "," << wall_duration << "," << cpu_duration << "," << base_ram
                     << "," << min_ram << "," << peak_ram << "," << sys_duration << "," << minflt_duration << "," << nvcsw_duration << "," << nivcsw_duration
                     << "\n";
        }
    }
};

#endif  // SARD_NN_PAPER_PROFILER_H