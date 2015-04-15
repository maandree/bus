#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>



#ifdef _SEM_SEMUN_UNDEFINED
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};
#endif


#define S 0
#define W 1
#define X 2
#define Q 3

#define SEMAPHORES 4
#define MEMORY_SIZE (2 * 1024)


char *argv0;


static int
create_semaphores(void)
{
	key_t key;
	int id = -1, saved_errno;
	union semun values;

	values.array = NULL;

	/* Create semaphore array. */
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
	values.array[X] = 1;
	if (!values.array)
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
create_shared_memory(void)
{
	key_t key;
	int id = -1, saved_errno;
	struct shmid_ds _info;

	for (;;) {
		double r = (double)rand();
		r /= (double)RAND_MAX + 1;
		r *= (1 << (8 * sizeof(key_t) - 2)) - 1;
		key = (key_t)r + 1;
		if (key == IPC_PRIVATE)
			continue;
		id = shmget(key, MEMORY_SIZE, IPC_CREAT | IPC_EXCL | 0600);
		if (id != -1)
			break;
		if ((errno != EEXIST) && (errno != EINTR))
			goto fail;
	}

	printf("key:%zi(x%zx), id:%i\n", (ssize_t)key, (size_t)key, id);
	return 0;

fail:
	saved_errno = errno;
	if ((id != -1) && (shmctl(id, IPC_RMID, &_info) == -1))
		perror(argv0);
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
remove_shared_memory(key_t key)
{
	int id = shmget(key, MEMORY_SIZE, 0600);
	struct shmid_ds _info;

	if (id == -1)
		return -1;

	if (shmctl(id, IPC_RMID, &_info) == -1)
		return -1;

	return 0;
}


static int
acquire_semaphore(key_t key, int semaphore, int delta, int undo)
{
	struct sembuf op;
	int id;

	id = semget(key, SEMAPHORES, 0600);
	if (id == -1)
		return -1;

	op.sem_op = -delta;
	op.sem_num = semaphore;
	op.sem_flg = undo * SEM_UNDO;

	return semop(id, &op, 1);
}


static int
release_semaphore(key_t key, int semaphore, int delta, int undo)
{
	struct sembuf op;
	int id;

	id = semget(key, SEMAPHORES, 0600);
	if (id == -1)
		return -1;

	op.sem_op = delta;
	op.sem_num = semaphore;
	op.sem_flg = undo * SEM_UNDO;

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


static int
write_semaphore(key_t key, int semaphore, int value)
{
	union semun semval;
	int id;

	id = semget(key, SEMAPHORES, 0600);
	if (id == -1)
		return -1;

	semval.val = value;
	return semctl(id, semaphore, SETVAL, semval);
}


static int
read_shared_memory(key_t key, char *message)
{
	int id, saved_errno;
	void *address = NULL;

	id = shmget(key, MEMORY_SIZE, 0600);
	if (id == -1)
		goto fail;

	address = shmat(id, NULL, SHM_RDONLY);
	if ((address == (void *)-1) || !address)
		goto fail;

	strncpy(message, address, MEMORY_SIZE);

	if (shmdt(address) == -1)
		goto fail;

	return 0;

fail:
	saved_errno = errno;
	if (address && (shmdt(address) == -1))
		perror(argv0);
	errno = saved_errno;
	return -1;
}


static int
write_shared_memory(key_t key, const char *message)
{
	int id, saved_errno;
	void *address = NULL;

	id = shmget(key, MEMORY_SIZE, 0600);
	if (id == -1)
		goto fail;

	address = shmat(id, NULL, 0);
	if ((address == (void *)-1) || !address)
		goto fail;

	memcpy(address, message, (strlen(message) + 1) * sizeof(char));

	if (shmdt(address) == -1)
		goto fail;

	return 0;

fail:
	saved_errno = errno;
	if (address && (shmdt(address) == -1))
		perror(argv0);
	errno = saved_errno;
	return -1;
}


#define t(inst)  if (r = inst, r)  goto fail


int
main(int argc, char *argv[])
{
	int r = -1, saved_errno, value;
	key_t key_sem = argc > 2 ? (key_t)atoll(argv[2]) : 0;
	key_t key_shm = argc > 3 ? (key_t)atoll(argv[3]) : 0;
	const char *message = argc > 4 ? argv[4] : "default message";
	char read_message[MEMORY_SIZE];

	argv0 = *argv;

	if (!strcmp(argv[1], "create")) {
		srand(time(NULL));
		t(create_semaphores());
		t(create_shared_memory());

	} else if (!strcmp(argv[1], "remove")) {
		t(remove_semaphores(key_sem));
		t(remove_shared_memory(key_sem));

	} else if (!strcmp(argv[1], "listen")) {
		t(release_semaphore(key_sem, S, 1, 1));
		for (;;) {
			t(release_semaphore(key_sem, Q, 1, 0));
			t(zero_semaphore(key_sem, Q));
			t(read_shared_memory(key_shm, read_message));
			printf("%s\n", read_message);
			t(release_semaphore(key_sem, W, 1, 1));
			t(acquire_semaphore(key_sem, S, 1, 1));
			t(zero_semaphore(key_sem, S));
			t(release_semaphore(key_sem, S, 1, 1));
			t(acquire_semaphore(key_sem, W, 1, 1));
		}

	} else if (!strcmp(argv[1], "broadcast")) {
		t(acquire_semaphore(key_sem, X, 1, 1));
		t(zero_semaphore(key_sem, W));
		t(write_shared_memory(key_shm, message));
		t(write_semaphore(key_sem, Q, 0));
		t(zero_semaphore(key_sem, S));
		t(release_semaphore(key_sem, X, 1, 1));

	} else
		return 2;

	return 0;

fail:
	perror(argv0);
	return 1;
}


#undef t

