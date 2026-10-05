/* spinlock.h — theo cavOS utilities/spinlock.c (Bài 12).
 * cavOS: Spinlock = atomic_flag; acquire = test_and_set, bận thì handControl() (nhường CPU
 * cho task khác thay vì quay tại chỗ — 1 CPU thì quay tại chỗ là vô ích). */
#pragma once

typedef volatile unsigned char Spinlock;

void spinlockAcquire(Spinlock *lock);
void spinlockRelease(Spinlock *lock);
