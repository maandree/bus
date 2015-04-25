#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define t(stmt)  if (stmt) goto fail



static size_t pauser_count = 0;
static size_t pausers_size = 0;
static char* pausers = NULL;



static int is_moc_playing(void)
{
	return !WEXITSTATUS(system("env LANG=C mocp -i 2>/dev/null | grep 'State: PLAY' >/dev/null"));
}



/* In a proper implementation, message whould be copyied, and then
 * a new thread would be created that parsed the copy. But that is
 * too much for an example, especially since it would also require
 * a mutex to make sure two threads do not modify data at the same
 * time, causing chaos. */
static int callback(const char *message, void *user_data)
{
 	char *msg;
	size_t len = 0;
	while ((len < 2047) && message[len])
		len++;
	msg = malloc((len + 1) * sizeof(char));
	t(msg == NULL);
	memcpy(msg, message, len * sizeof(char));
	msg[len] = 0;
	/* BEGIN run as in a separate thread */
	if (pauser_count || is_moc_playing()) {
		char *begin = strchr(msg, ' ');
		ssize_t pid;
		int requests_pause;
		if (begin == NULL)
			return 1;
		*begin++ = 0;
		pid = (ssize_t)atoll(msg);
		if (pid < 1) /* We need a real PID, too bad there is
				no convient way to detect if it dies. */
			return 1;
		if ((strstr(begin, "force-pause ") == begin) || !strcmp(begin, "force-pause"))
			requests_pause = 1;
		else if ((strstr(begin, "unforce-pause ") == begin) || !strcmp(begin, "unforce-pause"))
			requests_pause = 0;
		else
			return 1;
		if ((size_t)pid >= pausers_size) {
			pausers = realloc(pausers, (size_t)(pid + 1) * sizeof(char));
			t(pausers == NULL); /* Let's ignore the memory leak. */
			memset(pausers + pausers_size, 0, ((size_t)(pid + 1) - pausers_size) * sizeof(char));
			pausers_size = (size_t)(pid + 1);
		}
		if (pausers[pid] ^ requests_pause) {
			pauser_count += requests_pause ? 1 : -1;
			pausers[pid] = requests_pause;
			if (pauser_count == (size_t)requests_pause)
				system(requests_pause ? "mocp -P" : "mocp -U");
		}
	}
	/* END run as in a separate thread */
	return 1;
	(void) user_data;

fail:
	perror("monitor");
	return -1;
}



int main()
{
	bus_t bus;
	t(bus_open(&bus, "/tmp/example-bus", BUS_RDONLY));
	t(bus_read(&bus, callback, NULL));
	bus_close(&bus);
	free(pausers);
	return 0;

fail:
	perror("monitor");
	bus_close(&bus);
	free(pausers);
	return 1;
}

