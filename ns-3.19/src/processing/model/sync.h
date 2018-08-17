#ifndef SYNC_INCLUDE_HEADER
#define SYNC_INCLUDE_HEADER

#include "ns3/object.h"
#include "ns3/empty.h"
#include "ns3/simple-ref-count.h"

#include <list>

namespace ns3 {

class Semaphore;
class Semaphore : public SimpleRefCount<Semaphore>
{
public:
    Semaphore(int initialValue);

    bool Up();
    bool Down();

    void Block(int pid);
    int Unblock();

private:
    int value;
    std::list<int> m_blocked;
};

class Completion : public SimpleRefCount<Completion>
{
public:
    Completion();

    void SetWaiter(int pid);
    int Complete();
    bool IsCompleted();
private:
    int m_waiter;
    bool m_completed;
};

}

#endif
