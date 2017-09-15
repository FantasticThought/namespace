#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "fifo.h"

int main(int argc, char **argv) {
	int loop = 1;
	int fifo_to_server_fd = FIFO_DEFAULT_FD;
	int fifo_to_client_fd = FIFO_DEFAULT_FD;
	char buf[FIFO_BUF_LEN];
	fifo_client_data_t *client_data = (fifo_client_data_t *) buf;
	fifo_server_data_t *server_data = (fifo_server_data_t *) buf;


	mkfifo(FIFO_TO_SERVER, 0666);
	mkfifo(FIFO_TO_CLIENT, 0666);

	fifo_to_server_fd = open(FIFO_TO_SERVER, O_RDONLY);
	fifo_to_client_fd = open(FIFO_TO_CLIENT, O_WRONLY);

	printf("Server is ready...\n");

	while (loop)
	{
		read(fifo_to_server_fd, client_data, FIFO_BUF_LEN);

		switch (client_data->cmd) {
			case FIFO_SEND_HELLO:
				printf("[IPC] %s\n", client_data->data);
				server_data->rsp = FIFO_RSP_OK;
				sprintf(server_data->data, "Hello from Server");
				write(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
				break;
			case FIFO_SEND_HOSTNAME:
				printf("[IPC] Client hostname is %s\n", client_data->data);
				server_data->rsp = FIFO_RSP_OK;
				write(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
				break;
			case FIFO_SEND_PROCESS:
				printf("[IPC] Client process is: \n%s\n", client_data->data);
				server_data->rsp = FIFO_RSP_OK;
				write(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
				break;
			case FIFO_SEND_LOG:
				printf("[IPC] Client log: %s\n", client_data->data);
				server_data->rsp = FIFO_RSP_OK;
				write(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
				break;
			case FIFO_SEND_FILE_INFO:
			{
				fifo_file_info_t *file_info = (fifo_file_info_t *) client_data->data;
				printf("[IPC] Client send file info: %s\n", file_info->filename);
				printf("[IPC]\tfile size = %ld\n", file_info->stat.st_size);
				if (S_ISREG(file_info->stat.st_mode)) {
					printf("[IPC]\tA regular file\n");
				} else if (S_ISDIR(file_info->stat.st_mode)) {
					printf("[IPC]\tA directory\n");
				} else {
					printf("[IPC]\tUnknown type for name\n");
				}
				printf("[IPC]\tLast status change: %s", ctime(&file_info->stat.st_ctime));
				printf("[IPC]\tLast file access: %s", ctime(&file_info->stat.st_atime));
				printf("[IPC]\tLast file modification: %s", ctime(&file_info->stat.st_mtime));
				server_data->rsp = FIFO_RSP_OK;
				write(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
			}
				break;
			case FIFO_SEND_EXIT:
				printf("[IPC] Client exited\n");
				loop = 0;
				break;
			default:
				printf("[IPC] Client send unknown command 0x%.08X\n", client_data->cmd);
				server_data->rsp = FIFO_RSP_ERROR;
				sprintf(server_data->data, "Unknown command");
				write(fifo_to_client_fd, server_data, FIFO_BUF_LEN);
				break;
		}
	}

	close(fifo_to_server_fd);
	close(fifo_to_client_fd);

	unlink(FIFO_TO_SERVER);
	unlink(FIFO_TO_CLIENT);
	return 0;
}
