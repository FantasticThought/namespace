#ifndef __FIFO_H__
#define __FIFO_H__

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>

#define FIFO_TO_CLIENT	"./to_client.fifo"
#define FIFO_TO_SERVER	"./to_server.fifo"
#define FIFO_BUF_LEN	BUFSIZ
#define FIFO_DEFAULT_FD	-1

typedef enum {
	FIFO_SEND_HELLO,
	FIFO_SEND_HOSTNAME,
	FIFO_SEND_PROCESS,
	FIFO_SEND_FILE_INFO,
	FIFO_SEND_LOG,
	FIFO_SEND_EXIT
} fifo_client_cmd_t;

typedef struct {
	fifo_client_cmd_t cmd;
	char data[0];
} fifo_client_data_t;

typedef enum {
	FIFO_RSP_OK,
	FIFO_RSP_ERROR,
} fifo_server_rsp_t;

typedef struct {
	fifo_server_rsp_t rsp;
	char data[0];
} fifo_server_data_t;

typedef struct {
	char filename[PATH_MAX];
	struct stat stat;
} fifo_file_info_t;

extern int fifo_to_client_fd;
extern int fifo_to_server_fd;

#endif
