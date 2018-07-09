/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "os/mynewt.h"
#include "rwlock/rwlock.h"

#if MYNEWT_VAL(RWLOCK_DEBUG)
#define RWLOCK_DBG_ASSERT(expr) (assert(expr))
#else
#define RWLOCK_DBG_ASSERT(expr)
#endif

/**
 * Unblocks the next pending user.  The caller must lock the mutex prior to
 * calling this.
 */
static void
rwlock_unblock(struct rwlock *lock)
{
    RWLOCK_DBG_ASSERT(lock->mtx.mu_owner == g_current_task);
    RWLOCK_DBG_ASSERT(lock->handoffs == 0);

    /* Give priority to pending writers. */
    if (lock->pending_writers > 0) {
        /* Indicate that ownership is being transfered to a single writer. */
        lock->handoffs = 1;

        os_sem_release(&lock->wsem);
        lock->pending_writers--;
    } else {
        /* Indicate that ownership is being transfered to a collection of
         * readers.
         */
        lock->handoffs = lock->pending_readers;

        while (lock->pending_readers > 0) {
            os_sem_release(&lock->rsem);
            lock->pending_readers--;
        }
    }
}

static void
rwlock_complete_handoff(struct rwlock *lock)
{
    RWLOCK_DBG_ASSERT(lock->mtx.mu_owner == g_current_task);
    RWLOCK_DBG_ASSERT(lock->handoffs > 0);
    lock->handoffs--;
}

/**
 * Indicates whether a prospective reader must wait for the lock to become
 * available.
 */
static bool
rwlock_read_must_block(const struct rwlock *lock)
{
    RWLOCK_DBG_ASSERT(lock->mtx.mu_owner == g_current_task);

    return lock->active_writer ||
           lock->pending_writers > 0 ||
           lock->handoffs > 0;
}

/**
 * Indicates whether a prospective writer must wait for the lock to become
 * available.
 */
static bool
rwlock_write_must_block(const struct rwlock *lock)
{
    RWLOCK_DBG_ASSERT(lock->mtx.mu_owner == g_current_task);

    return lock->active_writer ||
           lock->num_readers > 0 ||
           lock->handoffs > 0;
}

void
rwlock_acquire_read(struct rwlock *lock)
{
    bool acquired;

    os_mutex_pend(&lock->mtx, OS_TIMEOUT_NEVER);

    if (rwlock_read_must_block(lock)) {
        lock->pending_readers++;
        acquired = false;
    } else {
        lock->num_readers++;
        acquired = true;
    }

    os_mutex_release(&lock->mtx);

    if (acquired) {
        /* No contention; lock acquired. */
        return;
    }

    /* Wait for the lock to become available. */
    os_sem_pend(&lock->rsem, OS_TIMEOUT_NEVER);

    /* Record reader ownership. */
    os_mutex_pend(&lock->mtx, OS_TIMEOUT_NEVER);
    lock->num_readers++;
    rwlock_complete_handoff(lock);
    os_mutex_release(&lock->mtx);
}

void
rwlock_release_read(struct rwlock *lock)
{
    os_mutex_pend(&lock->mtx, OS_TIMEOUT_NEVER);

    RWLOCK_DBG_ASSERT(lock->num_readers > 0);
    lock->num_readers--;

    /* If this is the last active reader, unblock a pending writer if there is
     * one.
     */
    if (lock->num_readers == 0) {
        rwlock_unblock(lock);
    }

    os_mutex_release(&lock->mtx);
}

void
rwlock_acquire_write(struct rwlock *lock)
{
    bool acquired;

    os_mutex_pend(&lock->mtx, OS_TIMEOUT_NEVER);

    if (rwlock_write_must_block(lock)) {
        lock->pending_writers++;
        acquired = false;
    } else {
        lock->active_writer = true;
        acquired = true;
    }

    os_mutex_release(&lock->mtx);

    if (acquired) {
        /* No contention; lock acquired. */
        return;
    }

    /* Wait for the lock to become available. */
    os_sem_pend(&lock->wsem, OS_TIMEOUT_NEVER);

    /* Record writer ownership. */
    os_mutex_pend(&lock->mtx, OS_TIMEOUT_NEVER);
    lock->active_writer = true;
    rwlock_complete_handoff(lock);
    os_mutex_release(&lock->mtx);
}

void
rwlock_release_write(struct rwlock *lock)
{
    os_mutex_pend(&lock->mtx, OS_TIMEOUT_NEVER);

    RWLOCK_DBG_ASSERT(lock->active_writer);
    lock->active_writer = false;

    rwlock_unblock(lock);

    os_mutex_release(&lock->mtx);
}

int
rwlock_init(struct rwlock *lock)
{
    int rc;

    *lock = (struct rwlock) { 0 };

    rc = os_mutex_init(&lock->mtx);
    if (rc != 0) {
        return rc;
    }

    rc = os_sem_init(&lock->rsem, 0);
    if (rc != 0) {
        return rc;
    }

    rc = os_sem_init(&lock->wsem, 0);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
