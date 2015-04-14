#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/sem.h>


#ifdef _SEM_SEMUN_UNDEFINED
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};
#endif


#define P 0
#define Q 1
#define N 2

#define SEMAPHORES 3


char *argv0;


static int
create_semaphores(void)
{
	key_t key;
	int id = -1, saved_errno;
	union semun values;

	values.array = NULL;

	/* Create semaphore array. */
	srand(time(NULL));
	for (;;) {
		double r = (double)rand();
		r /= (double)RAND_MAX + 1;
		r *= (1 << (8 * sizeof(key_t) - 2)) - 1;
		key = (key_t)r + 1;
		if (key == IPC_PRIVATE)
			continue;
		id = semget(key, SEMAPHORES, IPC_CREAT | IPC_EXCL | 0600);
		if (id != -1)
			break;
		if ((errno != EEXIST) && (errno != EINTR))
			goto fail;
	}

	/* Initialise the array. */
	values.array = calloc(SEMAPHORES, sizeof(unsigned short));
	if (values.array == NULL)
		goto fail;
	if (semctl(id, 0, SETALL, values.array) == -1)
		goto fail;
	free(values.array);
	values.array = NULL;

	printf("key:%zi(x%zx), id:%i\n", (ssize_t)key, (size_t)key, id);
	return 0;

fail:
	saved_errno = errno;
	if ((id != -1) && (semctl(id, 0, IPC_RMID) == -1))
		perror(argv0);
	free(values.array);
	errno = saved_errno;
	return -1;
}


static int
remove_semaphores(key_t key)
{
	int id = semget(key, SEMAPHORES, 0600);

	if (id == -1)
		return -1;

	if (semctl(id, 0, IPC_RMID) == -1)
		return -1;

	return 0;
}


static int
acquire_semaphore(key_t key, int semaphore, int delta)
{
	struct sembuf op;
	int id;

	id = semget(key, SEMAPHORES, 0600);
	if (id == -1)
		return -1;

	op.sem_op = -delta;
	op.sem_num = semaphore;
	op.sem_flg = 0;

	return semop(id, &op, 1);
}


static int
release_semaphore(key_t key, int semaphore, int delta)
{
	struct sembuf op;
	int id;

	id = semget(key, SEMAPHORES, 0600);
	if (id == -1)
		return -1;

	op.sem_op = delta;
	op.sem_num = semaphore;
	op.sem_flg = 0;

	return semop(id, &op, 1);
}


static int
zero_semaphore(key_t key, int semaphore)
{
	struct sembuf op;
	int id;

	id = semget(key, SEMAPHORES, 0600);
	if (id == -1)
		return -1;

	op.sem_op = 0;
	op.sem_num = semaphore;
	op.sem_flg = 0;

	return semop(id, &op, 1);
}


static int
read_semaphore(key_t key, int semaphore, int *z, int *n, int *v)
{
	struct sembuf op;
	int id, r;

	id = semget(key, SEMAPHORES, 0600);
	if (id == -1)
		return -1;

	if (r = semctl(id, semaphore, GETZCNT), r == -1)
		return -1;
	if (z)
		*z = r;

	if (r = semctl(id, semaphore, GETNCNT), r == -1)
		return -1;
	if (n)
		*n = r;

	if (r = semctl(id, semaphore, GETVAL), r == -1)
		return -1;
	if (v)
		*v = r;

	return 0;
}


int
main(int argc, char *argv[])
{
	int r = -1, saved_errno, value;
	key_t key = argc > 2 ? (key_t)atoll(argv[2]) : 0;

	argv0 = *argv;

	if (!strcmp(argv[1], "create")) {
		r = create_semaphores();
	} else if (!strcmp(argv[1], "remove")) {
		r = remove_semaphores(key);
	} else if (!strcmp(argv[1], "pick-up")) {
		r = release_semaphore(key, N, 1);
	} else if (!strcmp(argv[1], "hang-up")) {
		r = acquire_semaphore(key, N, 1);
	} else if (!strcmp(argv[1], "listen")) {
		r = acquire_semaphore(key, Q, 1);
		if (r) {
			saved_errno = errno;
			r = release_semaphore(key, P, 1);
			if (r)
				perror(argv0);
			errno = saved_errno;
			goto fail;
		}
		printf("read here\n");
		r = release_semaphore(key, P, 1);
	} else if (!strcmp(argv[1], "speak")) {
		printf("write here\n");
		r = read_semaphore(key, N, NULL, NULL, &value);
		if (r)
			goto fail;
		r = release_semaphore(key, Q, value);
		if (r)
			goto fail;
		r = acquire_semaphore(key, P, value);
	} else
		return 2;

	if (r)
		goto fail;
	return 0;

fail:
	perror(argv0);
	return 1;
}

