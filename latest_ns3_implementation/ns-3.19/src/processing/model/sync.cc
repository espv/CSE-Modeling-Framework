#include "ns3/sync.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Sync");

Semaphore::Semaphore(int initialValue) {
    NS_LOG_INFO("Created semaphore with initial value: " << initialValue);
    value = initialValue;
}

// Up() increments the semaphore and returns true if there's at least one process
// waiting to be unblocked. The task scheduler is then
// responsible for enqueueing the unblocked process.
bool Semaphore::Up() {
    NS_ASSERT(value >= 0);

    value += 1;
    return !m_blocked.empty();
}

// Down() decrements the semaphore if possible, or returns false if the calling
// process should be blocked on the semaphore.
bool Semaphore::Down() {
    NS_ASSERT(value >= 0);

    if (value == 0) {
        return false;
    } else {
        value -= 1;
    }
    return true;
}

// Block(pid) adds pid to the blocking queue.
void Semaphore::Block(int pid) {
    NS_ASSERT(value == 0);

    m_blocked.push_back(pid);
}

// Unblock() removes the first unblocked process in the waiting queue.
int Semaphore::Unblock() {
    NS_ASSERT(!m_blocked.empty());

    const int unblocked = m_blocked.front();
    m_blocked.pop_front();
    return unblocked;
}

Completion::Completion() {
    m_waiter = -1;
    m_completed = false;
}

void Completion::SetWaiter(int pid) {
    if (m_waiter != -1) {
        NS_LOG_ERROR("" << pid << " is waiting on a completion already waited on by " << m_waiter);
    }
    m_waiter = pid;
}

bool Completion::IsCompleted() {
    return m_completed;
}

int Completion::Complete() {
    m_completed = true;
    return m_waiter;
}

}
