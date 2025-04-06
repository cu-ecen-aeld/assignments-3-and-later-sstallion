#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#define DATA_PATH	"/var/tmp/aesdsocketdata"
#define DATA_SERVICE	"9000"

#define fatal(...) \
	dofatal(__FILE__, __LINE__, __VA_ARGS__)

struct thread_entry {
	pthread_t thread;
	SLIST_ENTRY(thread_entry) entries;
};

SLIST_HEAD(thread_head, thread_entry);

char *program;

volatile bool stop;
struct thread_head threads;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct sockaddr_storage addr;
socklen_t addrlen = sizeof(addr);
struct sockaddr_in *addrin = (void *)&addr;
int datafd;
int sockfd;

__attribute__ ((noreturn))
void dofatal(const char *file, const int line, const char *format, ...)
{
	char buf[1024];
	va_list args;

	if (format == NULL) {
		dofatal(file, line, strerror(errno));
	} else {
		va_start(args, format);
		vsnprintf(buf, sizeof(buf), format, args);
		fprintf(stderr, "%s:%d: %s\n", file, line, buf);
		va_end(args);
	}
	exit(-1);
}

void append(int fd, void *buf, ssize_t n)
{
	pthread_mutex_lock(&lock);
	if (lseek(datafd, 0, SEEK_END) < 0)
		fatal(NULL);

	n = write(datafd, buf, n);
	if (n < 0)
		fatal(NULL);

	pthread_mutex_unlock(&lock);
}

int bindto(const char *service)
{
	int err, sockfd;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(NULL, service, &hints, &res);
	if (err != 0)
		fatal(NULL);

	for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd < 0)
			fatal(NULL);

		const int *optval = &(int){1};
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, optval, sizeof(*optval)))
			fatal(NULL);

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0)
			fatal(NULL);

		freeaddrinfo(res);
		return sockfd;
	}
	return -1;
}

void *timer(void *arg)
{
	time_t t;
	struct tm *tmp;
	char buf[1024];
	ssize_t n;

	while (!stop) {
		sleep(10);
		if (stop)
			break;
		t = time(NULL);
		tmp = localtime(&t);
		n = strftime(buf, sizeof(buf), "timestamp:%a, %d %b %Y %T %z\n", tmp);
		append(datafd, buf, n);
	}
	return NULL;
}

void *client(void *arg)
{
	int acceptfd = (uintptr_t)arg;
	char buf[1024];
	ssize_t n;

	syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(addrin->sin_addr));

	// Accumulate packets until a newline is received:
	while (!stop) {
		n = recv(acceptfd, buf, sizeof(buf), 0);
		if (n < 0)
			fatal(NULL);
		append(datafd, buf, n);
		if (memchr(buf, '\n', sizeof(buf)) != NULL)
			break;
	}

	// Send accumulated packets to client:
	pthread_mutex_lock(&lock);
	if (lseek(datafd, 0, SEEK_SET) < 0)
		fatal(NULL);
	while (!stop) {
		n = read(datafd, buf, sizeof(buf));
		if (n < 0)
			fatal(NULL);
		if (n == 0)
			break; // EOF

		if (send(acceptfd, buf, n, 0) < 0)
			fatal(NULL);
	}
	pthread_mutex_unlock(&lock);

	close(acceptfd);
	syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(addrin->sin_addr));
	return NULL;
}

void term(int signum)
{
	struct thread_entry *entry;

	syslog(LOG_INFO, "Caught signal, exiting");
	closelog();

	close(sockfd);
	close(datafd);

	unlink(DATA_PATH);

	stop = true;
	SLIST_FOREACH(entry, &threads, entries)
	    pthread_join(entry->thread, NULL);

	exit(0);
}

void usage(void)
{
	fprintf(stderr, "Usage: %s [-d]\n", program);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	bool daemonize = false;
	struct thread_entry *entry;
	int err, opt;

	program = argv[0];
	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch (opt) {
		case 'd':
			daemonize = true;
			break;
		default:
			usage();
		}
	}

	SLIST_INIT(&threads);
	openlog(NULL, 0, LOG_USER);

	datafd = open(DATA_PATH, O_CREAT | O_RDWR, 0644);
	if (datafd < 0)
		fatal(NULL);

	sockfd = bindto(DATA_SERVICE);
	if (sockfd < 0)
		fatal("Unable to bind to %s", DATA_SERVICE);

	if (daemonize)
	if (daemon(0, 0) < 0)
		fatal(NULL);

	entry = malloc(sizeof(*entry));
	if (!entry)
		fatal(NULL);

	err = pthread_create(&entry->thread, NULL, timer, NULL);
	if (err != 0)
		fatal("Unable to create timer thread: %s", strerror(err));

	SLIST_INSERT_HEAD(&threads, entry, entries);

	signal(SIGINT, term);
	signal(SIGTERM, term);

	err = listen(sockfd, 8);
	if (err < 0)
		fatal(NULL);

	for (;;) {
		int acceptfd;

		acceptfd = accept(sockfd, (void *)&addr, &addrlen);
		if (acceptfd < 0)
			fatal(NULL);

		entry = malloc(sizeof(*entry));
		if (!entry)
			fatal(NULL);

		err = pthread_create(&entry->thread, NULL, client, (void *)(uintptr_t)acceptfd);
		if (err != 0)
			fatal("Unable to create client thread: %s", strerror(err));

		SLIST_INSERT_HEAD(&threads, entry, entries);
	}
}
