#define _XOPEN_SOURCE 700
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


#define    open_semaphore()                        ((sem_id = semget(key_sem, SEMAPHORES, 0600)) == -1 ? -1 : 0)
#define acquire_semaphore(semaphore, delta, undo)  semaphore_op(semaphore, -delta, undo)
#define release_semaphore(semaphore, delta, undo)  semaphore_op(semaphore, +delta, undo)
#define    zero_semaphore(semaphore)               semaphore_op(semaphore, 0, 0)


#define t(inst)  if ((inst) == -1)  goto fail



char *argv0;

static int sem_id = -1;
static key_t key_sem = -1;
static key_t key_shm = -1;



static int
get_keys(void)
{
	int saved_errno;
	char *line;
	size_t len;

	line = NULL, len = 0;
	t(getline(&line, &len, stdin));
	t(key_sem = (key_t)atoll(line));
	free(line);

	line = NULL, len = 0;
	t(getline(&line, &len, stdin));
	t(key_sem = (key_t)atoll(line));
	free(line);

	return 0;

fail:
	saved_errno = errno;
	free(line);
	errno = saved_errno;
	return -1;
}


static int
create_semaphores(void)
{
	int id = -1, saved_errno;
	union semun values;

	values.array = NULL;

	/* Create semaphore array. */
	for (;;) {
		double r = (double)rand();
		r /= (double)RAND_MAX + 1;
		r *= (1 << (8 * sizeof(key_t) - 2)) - 1;
		key_sem = (key_t)r + 1;
		if (key_sem == IPC_PRIVATE)
			continue;
		id = semget(key_sem, SEMAPHORES, IPC_CREAT | IPC_EXCL | 0600);
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

	printf("%zi\n", (ssize_t)key_sem);
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
	int id = -1, saved_errno;
	struct shmid_ds _info;

	/* Create shared memory. */
	for (;;) {
		double r = (double)rand();
		r /= (double)RAND_MAX + 1;
		r *= (1 << (8 * sizeof(key_t) - 2)) - 1;
		key_shm = (key_t)r + 1;
		if (key_shm == IPC_PRIVATE)
			continue;
		id = shmget(key_shm, MEMORY_SIZE, IPC_CREAT | IPC_EXCL | 0600);
		if (id != -1)
			break;
		if ((errno != EEXIST) && (errno != EINTR))
			goto fail;
	}

	printf("%zi\n", (ssize_t)key_shm);
	return 0;

fail:
	saved_errno = errno;
	if ((id != -1) && (shmctl(id, IPC_RMID, &_info) == -1))
		perror(argv0);
	errno = saved_errno;
	return -1;
}


static int
remove_semaphores(void)
{
	int id = semget(key_shm, SEMAPHORES, 0600);
	return ((id == -1) || (semctl(id, 0, IPC_RMID) == -1)) ? -1 : 0;
}


static int
remove_shared_memory(void)
{
	struct shmid_ds _info;
	int id = shmget(key_shm, MEMORY_SIZE, 0600);
	return ((id == -1) || (shmctl(sem_id, IPC_RMID, &_info) == -1)) ? -1 : 0;
}


static int
semaphore_op(int semaphore, int delta, int undo)
{
	struct sembuf op;
	op.sem_op = delta;
	op.sem_num = semaphore;
	op.sem_flg = undo * SEM_UNDO;
	return semop(sem_id, &op, 1);
}


static int
write_semaphore(int semaphore, int value)
{
	union semun semval;
	semval.val = value;
	return semctl(sem_id, semaphore, SETVAL, semval);
}



static int
read_shared_memory(char *message)
{
	int id, saved_errno;
	void *address = NULL;

	t(shmget(key_shm, MEMORY_SIZE, 0600));
	address = shmat(id, NULL, SHM_RDONLY);
	if ((address == (void *)-1) || !address)
		goto fail;
	strncpy(message, address, MEMORY_SIZE);
	t(shmdt(address));
	return 0;

fail:
	saved_errno = errno;
	if (address && (shmdt(address) == -1))
		perror(argv0);
	errno = saved_errno;
	return -1;
}


static int
write_shared_memory(const char *message)
{
	int id, saved_errno;
	void *address = NULL;

	t(id = shmget(key_shm, MEMORY_SIZE, 0600));
	address = shmat(id, NULL, 0);
	if ((address == (void *)-1) || !address)
		goto fail;
	memcpy(address, message, (strlen(message) + 1) * sizeof(char));
	t(shmdt(address));
	return 0;

fail:
	saved_errno = errno;
	if (address && (shmdt(address) == -1))
		perror(argv0);
	errno = saved_errno;
	return -1;
}


int
main(int argc, char *argv[])
{
	const char *message = argc > 2 ? argv[2] : "default message";
	char read_message[MEMORY_SIZE];

	argv0 = *argv;

	if (!strcmp(argv[1], "create")) {
		srand(time(NULL));
		t(create_semaphores());
		t(create_shared_memory());

	} else if (!strcmp(argv[1], "remove")) {
		t(get_keys());
		t(remove_semaphores());
		t(remove_shared_memory());

	} else if (!strcmp(argv[1], "listen")) {
		t(get_keys());
		t(open_semaphore());
		t(release_semaphore(S, 1, 1));
		for (;;) {
			t(release_semaphore(Q, 1, 0));
			t(zero_semaphore(Q));
			t(read_shared_memory(read_message));
			printf("%s\n", read_message);
			t(release_semaphore(W, 1, 1));
			t(acquire_semaphore(S, 1, 1));
			t(zero_semaphore(S));
			t(release_semaphore(S, 1, 1));
			t(acquire_semaphore(W, 1, 1));
		}

	} else if (!strcmp(argv[1], "broadcast")) {
		t(get_keys());
		t(open_semaphore());
		t(acquire_semaphore(X, 1, 1));
		t(zero_semaphore(W));
		t(write_shared_memory(message));
		t(write_semaphore(Q, 0));
		t(zero_semaphore(S));
		t(release_semaphore(X, 1, 1));

	} else
		return 2;

	return 0;

fail:
	perror(argv0);
	return 1;
}


#undef t

