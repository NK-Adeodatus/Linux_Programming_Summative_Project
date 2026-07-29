#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

/*
 * shared.h - Server-side shared state: registered users, connected users,
 * and the equipment reservation table. All access to mutable shared state
 * (equipment reservations, connected users list) must go through the
 * accompanying mutexes. Locking is wired up in a later step; for now this
 * file only declares the data model.
 */

#define MAX_USERS 16          /* max entries in the registered-user list */
#define MAX_CONNECTED 16      /* max simultaneously connected clients */
#define MAX_EQUIPMENT 8       /* max equipment items */
#define USER_ID_LEN 32        /* max length of a user id string */
#define EQUIP_NAME_LEN 32     /* max length of an equipment name */

/* One piece of lab equipment and its reservation state. */
struct equipment {
	int id;
	char name[EQUIP_NAME_LEN];
	int reserved;                  /* 0 = free, 1 = reserved */
	char reserved_by[USER_ID_LEN]; /* empty string if not reserved */
};

/* Registered (valid) user ids - who is allowed to log in at all. */
extern const char *registered_users[MAX_USERS];
extern const int registered_user_count;

/* Currently connected user ids (for server-side display/tracking). */
extern char connected_users[MAX_CONNECTED][USER_ID_LEN];
extern int connected_user_count;
extern pthread_mutex_t connected_users_lock;

/* The shared equipment table and its guard mutex. */
extern struct equipment equipment_table[MAX_EQUIPMENT];
extern const int equipment_count;
extern pthread_mutex_t equipment_lock;

/* Populate equipment_table with its initial (all-free) inventory. */
void shared_init(void);

#endif /* SHARED_H */