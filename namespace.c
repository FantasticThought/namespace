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
#include "fifo.h"

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

/* for IPC: named pipe */
char ipcbuf[BUFSIZ];
int fifo_to_server_fd = FIFO_DEFAULT_FD;
int fifo_to_client_fd = FIFO_DEFAULT_FD;
fifo_client_data_t *client_data = (fifo_client_data_t *) ipcbuf;
fifo_server_data_t *server_data = (fifo_server_data_t *) ipcbuf;

int container_main(void *args) {
	fifo_file_info_t file_info = {"/etc/hostname", {0}};

	printf("[CONTAINER] Entering Container %s\n", (char *)args);
	client_data->cmd = FIFO_SEND_HOSTNAME;
	sprintf(client_data->data, "%s", (char *)args);
	write(fifo_to_server_fd, client_data, FIFO_BUF_LEN);
	printf("[IPC] Send command FIFO_SEND_HOSTNAME, data=%s\n", client_data->data);
	read(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
	printf("[IPC] Server respone=%d\n", server_data->rsp);

	stat(file_info.filename, &file_info.stat);
	client_data->cmd = FIFO_SEND_FILE_INFO;
	memcpy(client_data->data, (char *)&file_info, sizeof(fifo_file_info_t));
	write(fifo_to_server_fd, client_data, FIFO_BUF_LEN);
	printf("[IPC] Send command FIFO_SEND_FILE_INFO\n");
	read(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
	printf("[IPC] Server respone=%d\n", server_data->rsp);

	system("/bin/sh");
	printf("[CONTAINER] Leave Container\n");

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

void change_ns_with_pid(int pid, char *container_id) {
	int container_pid = 0;
	namespace_t container_ns[] = {
		{-1, 0, "cgroup"},
		{-1, CLONE_NEWIPC, "ipc"},
		{-1, CLONE_NEWPID, "pid"},
		//{-1, CLONE_NEWNET, "net"},
		{-1, CLONE_NEWUTS, "uts"},
		{-1, CLONE_NEWNS, "mnt"},
		{-1, 0, ""}
	};

	open_process_namespace(pid, container_ns);
	//dump_process_namespace(container_ns);
	set_process_namespace(container_ns);

	container_pid = clone(container_main, container_stack+STACK_SIZE,
			CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD, container_id);
	waitpid(container_pid, NULL, 0);

	close_process_namespace(container_ns);
}

int main(int argc, char **argv) {
	int fd = 0, pid = 0, i=0;
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

	fifo_to_server_fd = open(FIFO_TO_SERVER, O_WRONLY);
	fifo_to_client_fd = open(FIFO_TO_CLIENT, O_RDONLY);

	client_data->cmd = FIFO_SEND_HELLO;
	sprintf(client_data->data, "Hello from client");
	write(fifo_to_server_fd, client_data, FIFO_BUF_LEN);
	read(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
	printf("[IPC] %s\n", server_data->data);

	open_process_namespace(0, agent_ns);
	//dump_process_namespace(agent_ns);

	for (i=1; i<argc; i++) {
		pid = container_id_to_pid(argv[i]);
		if (pid == 0) continue;

		change_ns_with_pid(pid, argv[i]);

		set_process_namespace(agent_ns);
	}
	close_process_namespace(agent_ns);

	client_data->cmd = FIFO_SEND_EXIT;
	write(fifo_to_server_fd, client_data, FIFO_BUF_LEN);
	close(fifo_to_server_fd);
	close(fifo_to_client_fd);

	return 0;
}
