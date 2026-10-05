/* spinlock.c — = cavOS spinlockAcquire/spinlockRelease (utilities/spinlock.c). */
#include "spinlock.h"
#include "task.h"

void spinlockAcquire(Spinlock *lock) {
    while (__atomic_test_and_set((void *)lock, __ATOMIC_ACQUIRE))
        handControl();
}

void spinlockRelease(Spinlock *lock) {
    __atomic_clear((void *)lock, __ATOMIC_RELEASE);
}
