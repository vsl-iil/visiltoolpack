#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <list>
#include <iostream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <psapi.h>
#include <tlhelp32.h>

#define quit(...) \
        vaprint(__VA_ARGS__); \
        return -1;
#define BUILDTIME __DATE__

const std::string version = std::string("0.9.0");
const int checkstep_ms = 250;

typedef std::pair<DWORD, float> ProcCPU;

typedef struct ProcessTiming {
    DWORD  pid;
    UINT64 kernel;
    UINT64 user;

    ProcessTiming(DWORD pid, UINT64 kernel, UINT64 user)
        : pid(pid)
        , kernel(kernel)
        , user(user)
    {}
} ProcessTiming;

// Список используется только в одной функции + программа однопоточная, так что
// глобальное состояние - не грех
std::list<ProcessTiming> proclist = std::list<ProcessTiming>();

void displayHelp();
float checkSystemTiming(FILETIME* idlet0, FILETIME* kernelt0, FILETIME* usert0, float* kernuser);
UINT64 filetimeToUint(FILETIME* ft);
void gotTheTime(char*, size_t);
int listProcesses(std::list<ProcCPU> &processes, int amount, float total_kernuser);

template <typename... Ts>
void vaprint(Ts... args);

int main(int argc, char** argv) {
    float duration  = 1.0;
    float threshold = 0.5; 
    float wait = 30'000.0;

    for (int i = 1; i < argc; i++) {
        char* argument = argv[i];
        if (!strcmp(argument, "-h") || !strcmp(argument, "--help")) {
            displayHelp();
            return 0;
        } else if (!strcmp(argument, "-d") || !strcmp(argument, "--duration")) {
            if (i+1 >= argc) {
                displayHelp();
                return -1;
            } else {
                i++;
                try {
                    duration = std::stof(argv[i]);
                    if (duration < checkstep_ms/1000.0) {
                        quit("Duration cannot be less than ", checkstep_ms, " ms");
                    }
                } 
                catch (std::invalid_argument const& ex) {
                    quit("Invalid value for `-d`");
                }
            }
        } else if (!strcmp(argument, "-t") || !strcmp(argument, "--threshold")) {
            if (i+1 >= argc) {
                displayHelp();
                return -1;
            } else {
                i++;
                try {
                    threshold = std::stof(argv[i]) / 100.0;
                    if (threshold > 1.0) {
                        quit("Threshold cannot be greater than 100%");
                    }
                } 
                catch (std::invalid_argument const& ex) {
                    quit("Invalid value for `-t`");
                }
            }
        } else if (!strcmp(argument, "-i") || !strcmp(argument, "--idle")) {
            if (i+1 >= argc) {
                displayHelp();
                return -1;
            } else {
                i++;
                try {
                    wait = std::stof(argv[i]) * 1000.0;
                    if (wait < 0.0) {
                        quit("Wait time cannot be negative");
                    }
                } 
                catch (std::invalid_argument const& ex) {
                    quit("Invalid value for `-i`");
                }
            }
        } else {
            displayHelp();
            quit("Invalid argument: ", argument);
        }
    }

    std::cout << "Duration:  " << duration << " s" << "\n"
              << "Threshold: " << threshold * 100 << "%" << "\n"
              << "Wait time: " << wait / 1000    << " s" << std::endl;

    Sleep(wait);
    
    float spike_duration = 0.0;
    float prev = 0.0;
    size_t stime_size = 64;
    char* stime = new char[stime_size];

    FILETIME idle = {}, kernel = {}, user = {};
    GetSystemTimes(&idle, &kernel, &user);

    std::list<ProcCPU> cpu_per_proc = std::list<ProcCPU>();
    listProcesses(cpu_per_proc, 3, 0.0f);
    while (true) {
        Sleep((duration < 1.0 ? duration*1000 : 1000));

        float kernuser;
        float percentage = checkSystemTiming(&idle, &kernel, &user, &kernuser);
        if (percentage >= threshold) {
            // происходит скачок
            spike_duration += 1.0;
        } else if (prev < threshold) {
            spike_duration = 0.0;
        } else if (spike_duration >= 0.5 && duration > 2) {
            spike_duration -= 0.25;
        } 
        std::cout << "[DEBUG] " << percentage * 100 << "%" << std::endl;

        if (spike_duration >= static_cast<int>(duration)) {
            spike_duration = 0.0;
            // log stuff
            gotTheTime(stime, stime_size);
            std::cout << "[ " << stime << " ] " 
                      << "Spike detected! "
                      << "| CPU: " << percentage * 100 << "%"
                      << std::endl;
            
            int status;
            if ((status = listProcesses(cpu_per_proc, 3, kernuser)) != ERROR_SUCCESS) {
                std::cout << "listProcesses error: " << status << std::endl;
            };

            std::vector<char> filename(MAX_PATH, 0);
            for (auto proc : cpu_per_proc) {
            
                std::fill(filename.begin(), filename.end(), 0);
                GetProcessImageFileNameA(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, std::get<0>(proc)), &filename[0], MAX_PATH);

                std::cout   << "PID:  " << std::get<0>(proc)
                           << " CPU%: " << std::get<1>(proc) * 100.0 << "%"
                           << " Name: " << std::string(std::find(filename.rbegin(), filename.rend(), '\\').base(), filename.end())
                                        // ^^^ куда проще было бы malloc(char, MAX_PATH) и strrchr(filename, '\\'),
                                        //     но - best practices, best practices...
                          << "\n========================" << std::endl;
            }
            std::cout << std::endl;

            cpu_per_proc.clear();
        }
        prev = duration;
    }

    free(stime);
}


void displayHelp() {
    std::cerr << "cpuspike v."
                << version
                << "  Copyright (C) 2025  Egor Lvov\n"
                << "Usage:\n"
                << "  cpuspike [OPTION...]\n"
                << "\t-h | --help                 -  display this help\n"
                << "\t-d | --duration <SECONDS>   -  how long should the spike be to be detected (default: 1)\n"
                << "\t-t | --threshold <PERCENT>  -  how high should the spike be to be detected (default: 50%)\n"
                << "\t-i | --idle <SECONDS>       -  wait SECONDS of user idling before starting (default: 30)\n"
                << std::endl;
}

float checkSystemTiming(FILETIME* idlet0, FILETIME* kernelt0, FILETIME* usert0, float* kernuser) {
    FILETIME idlet1 = {}, kernelt1 = {}, usert1 = {};
    if (!GetSystemTimes(&idlet1, &kernelt1, &usert1)) {
        std::cerr << "GetSystemTimes error: " << GetLastError() << std::endl;
        exit(-1);
    }

    float d_idle = filetimeToUint(&idlet1)   - filetimeToUint(idlet0);
    float d_user = filetimeToUint(&usert1)   - filetimeToUint(usert0);
    float d_kern = filetimeToUint(&kernelt1) - filetimeToUint(kernelt0);

    *idlet0   = idlet1;
    *kernelt0 = kernelt1;
    *usert0   = usert1;

    *kernuser = d_kern + d_user;

    return (d_kern - d_idle + d_user) / (d_kern + d_user); 
}

UINT64 filetimeToUint(FILETIME* ft) {
    ULARGE_INTEGER uli = {};
    uli.LowPart  = ft->dwLowDateTime;
    uli.HighPart = ft->dwHighDateTime;
    return uli.QuadPart;
}

void gotTheTime(char* outbuf, size_t szbuf) {
    auto sysclock = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ctime_s(outbuf, szbuf, &sysclock);
    outbuf[strlen(outbuf)-1] = '\0';
}

int listProcesses(std::list<ProcCPU> &processes, int amount, float total) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }

    std::list<ProcessTiming> currentlist = std::list<ProcessTiming>();
    PROCESSENTRY32 entry = {};
    entry.dwSize = sizeof(PROCESSENTRY32);
    BOOL success = Process32First(snapshot, &entry);

    if (!success) {
        return GetLastError();
    } 

    FILETIME _tmp = {}, kernel = {}, user = {};
    // Заполняем список текущих процессов
    do {
        if (entry.th32ProcessID == 0) continue;
        HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, entry.th32ProcessID);
        if (proc == NULL) {
            int lasterr = GetLastError();
            if (lasterr == ERROR_ACCESS_DENIED) {
                continue;
            } else {
                return GetLastError();
            }
        }
        if (GetProcessTimes(proc, &_tmp, &_tmp, &kernel, &user) == 0) {
            return GetLastError();
        }
        currentlist.emplace_back(entry.th32ProcessID, filetimeToUint(&kernel), filetimeToUint(&user));
    } while(Process32Next(snapshot, &entry));

    CloseHandle(snapshot);

    currentlist.sort([](const ProcessTiming &a, const ProcessTiming &b) {
        return a.pid < b.pid;
    });
    
    auto itold  = proclist.begin();
    auto itnew  = currentlist.begin();

    std::vector<ProcCPU> perf = std::vector<ProcCPU>();
    perf.reserve(1024);
    while (itold != proclist.end() && itnew != currentlist.end()) {
        if (itnew->pid < itold->pid)
            // новый процесс
            itnew++;
        else if (itnew->pid > itold->pid)
            // процесс завершился
            itold++;
        else {
            // Находим производительность процесса
            UINT64 d_kern = itnew->kernel - itold->kernel;
            UINT64 d_user = itnew->user - itold->user;
            // UINT64 d_time = itnew->time - itold->time;
            float cpu_usage = static_cast<float>(d_kern + d_user) / total;
            // std::cout << cpu_usage << std::endl;
            perf.push_back(ProcCPU(itnew->pid, cpu_usage));
        }

        itnew++;
        itold++;
    }

    proclist.swap(currentlist);

    std::sort(perf.begin(), perf.end(), [](const auto &a, const auto &b) {
        return std::get<1>(a) > std::get<1>(b);
    });

    auto itperf = perf.begin();
    for (int i = 0; i < amount && itperf < perf.end(); i++) {
        processes.push_back(*itperf);
        itperf++;
    }

    return ERROR_SUCCESS;
}

// float getLogicaltoPhysicalRatio() {

// }

template <typename... Ts>
void vaprint(Ts... args) {
    (std::cerr << ... << args) << std::endl;
}