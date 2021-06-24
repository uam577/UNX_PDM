// System includes
#include <cmath>
#include <cstring>

// Local includes
#include "palanteer.h"
#include "testPart.h"

// Selection of the logging or not of the "RANDOM" group
#ifndef PL_GROUP_RANDOM
#define PL_GROUP_RANDOM 1
#endif



// =============
// Crash helpers
// =============

// Inline just to stress & test the stacktrace display
template<class T>
inline
void
crashSubContractor(uint16_t crashKind, T zero)
{
    switch(crashKind) {
    case 0: printf("%d", 1/zero); break;
    case 1: *((volatile char*)0) = 0; break;
    case 2:
        plAssert(0==1, "This is an assertion-based crash", zero, crashKind);
        (void) zero; // Fallthrough, in case NDEBUG is defined
        // FALLTHRU
    case 3: abort();
    }
}


void
doCrash_Please(int crashKind, int zero)
{
    crashSubContractor(crashKind, zero);
}


// =======================
// Random number generator
// =======================

uint32_t
RandomLCM::get(uint64_t min, uint64_t max)
{
    plScope("RandomLCM::get");
    plgLockWait(RANDOM, "Random generator");
    std::lock_guard<std::mutex> lk(_mx);
    plgLockScopeState(RANDOM, "Random generator", true);

    uint32_t generatedNumber = (uint32_t)(min+(getNext()&0xFFFFFFFF)*(max-min)/(1LL<<32));
    plgVar(RANDOM, generatedNumber);

    return generatedNumber;
}

uint64_t
RandomLCM::getNext(void)
{
    // It is a really dummy random generator
    uint64_t x = _lastValue;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    _lastValue = (_lastValue^x)*1099511628211ULL;
    return x;
}


// ===================================
// Functions of the "associated" task
// ===================================

float
busyWait(int kRoundQty)
{
    float cosSum = 14.;
    for(int i=0; i<1000*kRoundQty; ++i) cosSum += cos(0.1*i);
    return cosSum;
}


// Thread entry point
void
associatedTask(int groupNbr, const char* groupName, int crashKind)
{
    // Declare the thread name with a dynamic string
    char fullThreadName[64];
    snprintf(fullThreadName, sizeof(fullThreadName), "%s%sAssociate", groupName, strlen(groupName)? "/":"");
    plDeclareThreadDyn(fullThreadName);

    float dummyValue   = 0.;
    int   iterationNbr = 0;
    Synchro& synchro   = groupSynchro[groupNbr];
    std::string synchroLockName = strlen(groupName)? (std::string(groupName) + " synchro") : "synchro";

    while(1) {
        // Local scope to contain the RAII lock
        {
            // Wait for an order to do something
            plLockWaitDyn(synchroLockName.c_str());
            std::unique_lock<std::mutex> lk(synchro.mx);
            synchro.cv.wait(lk, [&synchro] { return synchro.command.load()!=0; });

            // Thread was awakened
            plLockScopeStateDyn(synchroLockName.c_str(), true);

            // Get the command from the "control" thread of the group
            int controlCount = synchro.command.exchange(0);
            plLockStateDyn(synchroLockName.c_str(), false);
            if(controlCount==2) break; // End of thread, command "2" from the control thread means quit the while loop
        }

        // Marker of a great event
        if(iterationNbr==4) plMarker("important", "5th iteration reached!");

        // Do something
        plScope("SomeWork");
        dummyValue += busyWait(globalRandomGenerator.get(1500, 4000));

        // Crash if required
        if(crashKind>=0 && iterationNbr==3) doCrash_Please(crashKind, 0); // Crash at 3rd iteration if crash required
        ++iterationNbr;
    }

    plBegin("Final result");
    plVar(dummyValue);
    plEnd("Final result");
}
