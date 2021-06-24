
#pragma once

#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>


// Synchronization
// ===============
struct Synchro {
    Synchro(void) { command.store(0); }
    Synchro(const Synchro& s) { command.store(s.command.load()); }
    std::atomic<int> command;
    std::mutex       mx;
    std::condition_variable cv;
};

extern std::vector<Synchro> groupSynchro;


// Random generator
// ================
class RandomLCM
{
public:
    RandomLCM(void) { }
    uint32_t get(uint64_t min, uint64_t max);

private:
    uint64_t   getNext(void);
    uint64_t   _lastValue = 14695981039346656037ULL;
    std::mutex _mx;
};

extern RandomLCM globalRandomGenerator;


// Functions
// =========
void  associatedTask(int groupNbr, const char* groupName, int crashKind);

float busyWait(int kRoundQty);
