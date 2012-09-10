/*-
 * Copyright (c) 2008-2012 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * Spin locks:
 *
 * These used for cases where fast mutual exclusion is needed (where operations
 * done while holding the spin lock are expected to complete in a small number
 * of instructions.
 */

#if SPINLOCK_TYPE == SPINLOCK_GCC

static inline void
__wt_spin_init(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);

	*(t) = 0;
}

static inline void
__wt_spin_destroy(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);
	WT_UNUSED(t);
}

static inline void
__wt_spin_lock(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);

	while (__sync_lock_test_and_set(t, 1))
		while (*t)
			WT_PAUSE();
}

static inline int
__wt_spin_trylock(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);

	return (__sync_lock_test_and_set(t, 1);
}

static inline void
__wt_spin_unlock(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);

	__sync_lock_release(t);
}

#elif SPINLOCK_TYPE == SPINLOCK_PTHREAD_MUTEX

static inline void
__wt_spin_init(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);

	(void)pthread_mutex_init(t, NULL);
}

static inline void
__wt_spin_destroy(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);

	(void)pthread_mutex_destroy(t);
}

static inline void
__wt_spin_lock(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);
	pthread_mutex_lock(t);
}

static inline int
__wt_spin_trylock(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);
	return (pthread_mutex_trylock(t));
}

static inline void
__wt_spin_unlock(WT_SESSION_IMPL *session, WT_SPINLOCK *t)
{
	WT_UNUSED(session);
	pthread_mutex_unlock(t);
}

#else

#error Unknown spinlock type

#endif
