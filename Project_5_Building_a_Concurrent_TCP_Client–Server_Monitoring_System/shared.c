#include <string.h>
#include <pthread.h>
#include "shared.h"

/*
 * shared.c - definitions and initialization for the shared server state
 * declared in shared.h.
 */

/* Registered users allowed to authenticate. Hardcoded for this project;
 * a real system would load these from a database or config file.
 */
const char *registered_users[MAX_USERS] = {
	"alice123",
	"bob456",
	"carol789",
	"dave000"
};
const int registered_user_count = 4;

/* Currently connected users - grows/shrinks as clients connect/disconnect. */
char connected_users[MAX_CONNECTED][USER_ID_LEN];
int connected_user_count = 0;
pthread_mutex_t connected_users_lock = PTHREAD_MUTEX_INITIALIZER;

/* The equipment inventory and its guard mutex. */
struct equipment equipment_table[MAX_EQUIPMENT];
const int equipment_count = 3;
pthread_mutex_t equipment_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * shared_init - initialize the equipment table to its starting,
 * all-available state. Must be called once at server startup, before
 * any client threads are created.
 */
void shared_init(void)
{
	strcpy(equipment_table[0].name, "Oscilloscope");
	strcpy(equipment_table[1].name, "SolderStation");
	strcpy(equipment_table[2].name, "LogicAnalyzer");

	for (int i = 0; i < equipment_count; i++)
	{
		equipment_table[i].id = i + 1;
		equipment_table[i].reserved = 0;
		equipment_table[i].reserved_by[0] = '\0';
	}
}