#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void usage(void)
{
	fprintf(stderr, "Usage: writer writefile writestr\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	if (argc < 3)
		usage();

	char *writefile = argv[1];
	char *writestr = argv[2];

	openlog(NULL, 0, LOG_USER);

	int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		syslog(LOG_ERR, "Unable to open file: %s", strerror(errno));
		return EXIT_FAILURE;
	}

	syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	if (write(fd, writestr, strlen(writestr)) == -1) {
		syslog(LOG_ERR, "Unable to write string: %s", strerror(errno));
		return EXIT_FAILURE;
	}

	close(fd);
	closelog();
	return EXIT_SUCCESS;
}
