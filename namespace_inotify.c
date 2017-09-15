#define _GNU_SOURCE
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>
#include <sys/inotify.h>

#define STACK_SIZE	1024 * 1024
static char container_stack[STACK_SIZE];

typedef struct {
	int fd;
	int nstype;
	char nsname[16];
} namespace_t;

#define MAX_CONTAINER_INFO	1024
typedef struct {
	int pid;
	char id[128];
} container_info_t;
container_info_t container_infos[MAX_CONTAINER_INFO];
int container_info_num = 0;

void displayInotifyEvent(struct inotify_event *i) {
	printf("    wd =%2d; ", i->wd);
	if (i->cookie > 0)
		printf("cookie =%4d; ", i->cookie);

	printf("mask = ");
	if (i->mask & IN_ACCESS)        printf("IN_ACCESS ");
	if (i->mask & IN_ATTRIB)        printf("IN_ATTRIB ");
	if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
	if (i->mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
	if (i->mask & IN_CREATE)        printf("IN_CREATE ");
	if (i->mask & IN_DELETE)        printf("IN_DELETE ");
	if (i->mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
	if (i->mask & IN_IGNORED)       printf("IN_IGNORED ");
	if (i->mask & IN_ISDIR)         printf("IN_ISDIR ");
	if (i->mask & IN_MODIFY)        printf("IN_MODIFY ");
	if (i->mask & IN_MOVE_SELF)     printf("IN_MOVE_SELF ");
	if (i->mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
	if (i->mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
	if (i->mask & IN_OPEN)          printf("IN_OPEN ");
	if (i->mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
	if (i->mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");
	printf("\n");

	if (i->len > 0)
		printf("        name = %s\n", i->name);
}

#ifndef NAME_MAX
#define NAME_MAX 128
#endif
#define INOTIFY_BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

int container_main(void *args) {
	int rc = 0, i=0;
	int inotifyFD = 0, watchFD = 0, numRead = 0;
	char buf[INOTIFY_BUF_LEN] __attribute__ ((aligned(8)));
	struct inotify_event *event;
	char *p = NULL;
	int inotify_flags = IN_CREATE | IN_DELETE | IN_OPEN | IN_CLOSE | IN_ACCESS | IN_CLOSE_WRITE;

	int container_fd = 0;
	char container_buffer[128] = {"ggghhhhhh"};

	printf("[CONTAINER] Entering Container\n");

	container_fd  = open("/tmp/qoo2.txt", O_WRONLY | O_CREAT, 0644);
	if (container_fd == -1) printf("rrrororooroerror, %s(%d)\n", strerror(errno), errno);
	write(container_fd, container_buffer, 128);
	close(container_fd);

	system("/bin/sh");
	printf("[CONTAINER] Leaving Container, rc=%d\n", rc);
	return 0;

	inotifyFD = inotify_init();
	if (inotifyFD == -1) {
		printf("[CONTAINER] inotify_init() failed: %s(%d)\n", strerror(errno), errno);
		return -1;
	}

	watchFD = inotify_add_watch(inotifyFD, "/tmp", inotify_flags);
	if (watchFD == -1) {
		printf("[CONTAINER] inotify_add_watch() failed: %s(%d)\n", strerror(errno), errno);
		return -1;
	}

	for (;;) {
		numRead = read(inotifyFD, buf, INOTIFY_BUF_LEN);

		switch (numRead) {
			case -1:
				printf("[CONTAINER] read failed: %s(%d)\n", strerror(errno), errno);
				return -1;
			case 0:
				printf("[CONTAINER] no events\n");
				break;
			default:
				printf("[CONTAINER] read %d events\n", numRead);
				for (p=buf; p<buf+numRead;) {
					event = (struct inotify_event *) p;
					displayInotifyEvent(event);
					p += sizeof(struct inotify_event) + event->len;
				}
				break;
		}
	}

	printf("[CONTAINER] Leaving Container, rc=%d\n", rc);

	return 0;
}

void list_container_pids(void) {
	char container_name[64]={0};
	int container_pid = 0;
	char buffer[64];
	FILE *fp = NULL;

	fp = popen("./containerPID.sh", "r");
	while (fgets(buffer, 64, fp)) {
		if ((strlen(buffer) > 0) && (container_info_num < MAX_CONTAINER_INFO)) {
			sscanf(buffer, "%s %d", container_infos[container_info_num].id, &container_infos[container_info_num].pid);
			container_info_num ++;
			buffer[0] = '\0';
		}
	}
	fclose(fp);
}

void dump_container_infos(void) {
	int i=0;

	for (i=0; i<container_info_num; i++) {
		printf("container: %s, pid=%d\n", container_infos[i].id, container_infos[i].pid);
	}
}

int container_id_to_pid(char *container_id) {
	int i = 0;
	int pid = 0;
	int len = 0;

	for (i=0; i<container_info_num; i++) {
		if (strncmp(container_infos[i].id, container_id, strlen(container_infos[i].id)) == 0) {
			pid = container_infos[i].pid;
			break;
		}
	}

	return pid;
}

int open_process_namespace(int pid, namespace_t *namespaces) {
	int fd=0, i=0;
	char buffer[64] = {0};
	char *proc_string_pid = "/proc/%d/ns";
	char *proc_string_self = "/proc/self/ns";
	char *proc_string = (pid > 0) ? proc_string_pid : proc_string_self;
	namespace_t *ns = NULL;

	for (ns=namespaces; ns->nsname[0] != '\0'; ns++) {
		if (pid) {
			snprintf(buffer, 64, "/proc/%d/ns/%s", pid, ns->nsname);
		} else {
			snprintf(buffer, 64, "/proc/self/ns/%s", ns->nsname);
		}

		if ((fd = open(buffer, O_RDONLY)) == -1) {
			printf("open %s failed: %s(%d)", buffer, strerror(errno), errno);
			break;
		}
		ns->fd = fd;
		//printf("open namespace %s, type=%d, fd=%d\n", ns->nsname, ns->nstype, ns->fd);
	}

	return 0;
}

int set_process_namespace(namespace_t *namespaces) {
	int rc=0, i=0;
	namespace_t *ns = NULL;

	for (ns=namespaces; ns->nsname[0] != '\0'; ns++) {
		if ((rc = setns(ns->fd, ns->nstype)) == -1) {
			printf("setns(%s) failed: %s(%d)", ns->nsname, strerror(errno), errno);
			break;
		}
		//printf("set namespace %s, type=%d, fd=%d\n", ns->nsname, ns->nstype, ns->fd);
	}

	return rc;
}

void close_process_namespace(namespace_t *namespaces) {
	namespace_t *ns = NULL;

	for (ns=namespaces; ns->nsname[0] != '\0'; ns++) {
		if (ns->fd > 0) {
			//printf("close namespace %s, type=%d, fd=%d\n", ns->nsname, ns->nstype, ns->fd);
			close(ns->fd);
			ns->fd = -1;
		}
	}
}

void dump_process_namespace(namespace_t *namespaces) {
	namespace_t *ns = NULL;

	for (ns=namespaces; ns->nsname[0] != '\0'; ns++) {
		printf("dump namespace %s, type=%d, fd=%d\n", ns->nsname, ns->nstype, ns->fd);
	}
}

int main(int argc, char **argv) {
	int fd = 0, pid = 0, i=0, j=0;
	int container_pid = 0;
	char filename[128] = {0};
	namespace_t agent_ns[] = {
		{-1, 0, "cgroup"},
		{-1, CLONE_NEWIPC, "ipc"},
		{-1, CLONE_NEWPID, "pid"},
		//{-1, CLONE_NEWNET, "net"},
		{-1, CLONE_NEWUTS, "uts"},
		{-1, CLONE_NEWNS, "mnt"},
		{-1, 0, ""}
	};

	list_container_pids();
	dump_container_infos();

	open_process_namespace(0, agent_ns);
	//dump_process_namespace(agent_ns);

	for (i=1; i<argc; i++) {
		namespace_t container_ns[] = {
			{-1, 0, "cgroup"},
			{-1, CLONE_NEWIPC, "ipc"},
			{-1, CLONE_NEWPID, "pid"},
			//{-1, CLONE_NEWNET, "net"},
			{-1, CLONE_NEWUTS, "uts"},
			{-1, CLONE_NEWNS, "mnt"},
			{-1, 0, ""}
		};

		pid = container_id_to_pid(argv[i]);
		if (pid == 0) continue;

		open_process_namespace(pid, container_ns);
		//dump_process_namespace(container_ns);
		set_process_namespace(container_ns);

		container_pid = clone(container_main, container_stack+STACK_SIZE,
				CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD, NULL);

		waitpid(container_pid, NULL, 0);
		close_process_namespace(container_ns);

		set_process_namespace(agent_ns);
	}
	close_process_namespace(agent_ns);

	return 0;
}
