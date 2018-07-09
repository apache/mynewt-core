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

#ifndef H_RWLOCK_
#define H_RWLOCK_

#include "os/mynewt.h"

/**
 * @brief Readers–writer lock - lock for multiple readers, single writer.
 *
 * This lock is write-preferring.  That is:
 *     o If there is no active writer and no pending writers, read-acquisitions
 *       do not block.
 *     o If there is an active writer or a pending writer, read-acquisitions
 *       block.
 *     o When the last active reader or the active writer releases the lock, it
 *       is acquired by a pending writer if there is one.  If there are no
 *       pending writers, the lock is acquired by all pending readers.
 *
 * All struct fields should be considered private.
 */
struct rwlock {
    /** Protects access to rwlock's internal state. */
    struct os_mutex mtx;

    /** Blocks and wakes up pending readers. */
    struct os_sem rsem;

    /** Blocks and wakes up pending writers. */
    struct os_sem wsem;

    /** The number of active readers. */
    uint8_t num_readers;

    /** Whether there is an active writer. */
    bool active_writer;

    /** The number of blocked readers. */
    uint8_t pending_readers;

    /** The number of blocked writers. */
    uint8_t pending_writers;

    /**
     * The number of ownership transfers currently in progress.  No new
     * acquisitions are allowed until all handoffs are complete.
     */
    uint8_t handoffs;
};

/**
 * @brief Acquires the lock for use by a reader.
 *
 * @param lock                  The lock to acquire.
 */
void rwlock_acquire_read(struct rwlock *lock);

/**
 * Releases the lock from a reader.
 *
 * @param lock                  The lock to release.
 */
void rwlock_release_read(struct rwlock *lock);

/**
 * @brief Acquires the lock for use by a writer.
 *
 * @param lock                  The lock to acquire.
 */
void rwlock_acquire_write(struct rwlock *lock);

/**
 * Releases the lock from a writer.
 *
 * @param lock                  The lock to release.
 */
void rwlock_release_write(struct rwlock *lock);

/**
 * Initializes a readers-writer lock.
 *
 * @param lock                  The lock to initialize.
 *
 * @return                      0 on success; nonzero on failure.
 */
int rwlock_init(struct rwlock *lock);

#endif
