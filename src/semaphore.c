#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/sem.h>


#ifdef _SEM_SEMUN_UNDEFINED
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};
#endif


char *argv0;


int main(int argc, char *argv[])
{
	key_t key;
	int id = -1;

	(void) argc;
	argv0 = *argv;

	srand(time(NULL));
	for (;;) {
		double r = (double)rand();
		r /= (double)RAND_MAX + 1;
		r *= (1 << (8 * sizeof(key_t) - 2)) - 1;
		key = (key_t)r + 1;
		if (key == IPC_PRIVATE)
			continue;
		id = semget(key, 3, IPC_CREAT | IPC_EXCL | 0600);
		if (id != -1)
			break;
		if ((errno != EEXIST) && (errno != EINTR))
			goto fail;
	}

	printf("key:%zx, id:%i\n", (size_t)key, id);

	if (semctl(id, 0, IPC_RMID) == -1)
		goto fail;

	return 0;

fail:
	perror(argv0);
	if ((id != -1) && (semctl(id, 0, IPC_RMID) == -1))
		perror(argv0);
	return 1;
}

