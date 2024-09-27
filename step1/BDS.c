// a basic disk server
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "tcp_utils.h"

#define BLOCKSIZE 256
int ncyl, nsec, ttd;
char *disk_data;
int last_c = 0;

// return a negative value to exit
int cmd_i(tcp_buffer *write_buf, char *args, int len) {
    static char buf[64];
    sprintf(buf, "%d %d", ncyl, nsec);

    // send to buffer, including the null terminator
    send_to_buffer(write_buf, buf, strlen(buf) + 1);
    return 0;
}

int cmd_r(tcp_buffer *write_buf, char *args, int len) {
    // send_to_buffer(write_buf, args, len);
    // return 0;
    int c, s;
    if (sscanf(args, "%d %d", &c, &s) != 2) {
        send_to_buffer(write_buf, "No", 3);
        return 0;
    }
    if (c < 0 || c >= ncyl || s < 0 || s >= nsec) {
        send_to_buffer(write_buf, "No", 3);
        return 0;
    }
    char response[BLOCKSIZE + 4];
    memcpy(response, "Yes ", 4);
    memcpy(response + 4, disk_data + (c * nsec + s) * BLOCKSIZE, BLOCKSIZE);
    send_to_buffer(write_buf, response, BLOCKSIZE + 4);
    usleep(ttd * abs(c - (last_c != -1 ? last_c : c)));
    last_c = c;
    return 0;
}

int cmd_w(tcp_buffer *write_buf, char *args, int len) {
    // send_to_buffer(write_buf, "Yes", 4);
    // return 0;
    int c, s, l;
    if (sscanf(args, "%d %d %d", &c, &s, &l) != 3 || l > BLOCKSIZE) {
        send_to_buffer(write_buf, "No", 3);
        return 0;
    }
    char *data = strchr(args, ' ') + 1;
    data = strchr(data, ' ') + 1;
    data = strchr(data, ' ') + 1;
    if (c < 0 || c >= ncyl || s < 0 || s >= nsec || l < 0 || l > BLOCKSIZE || len - (data - args) < l) {
        send_to_buffer(write_buf, "No", 3);
        return 0;
    }
    memcpy(disk_data + (c * nsec + s) * BLOCKSIZE, data, l);
    if (l < BLOCKSIZE) {
        memset(disk_data + (c * nsec + s) * BLOCKSIZE + l, 0, BLOCKSIZE - l);
    }
    send_to_buffer(write_buf, "Yes", 4);
    usleep(ttd * abs(c - (last_c != -1 ? last_c : c)));
    last_c = c;
    return 0;
}

int cmd_e(tcp_buffer *write_buf, char *args, int len) {
    send_to_buffer(write_buf, "Bye!", 5);
    return -1;
}

static struct {
    const char *name;
    int (*handler)(tcp_buffer *write_buf, char *, int);
} cmd_table[] = {
    {"I", cmd_i},
    {"R", cmd_r},
    {"W", cmd_w},
    {"E", cmd_e},
};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

void add_client(int id) {
    // some code that are executed when a new client is connected
    // you don't need this in step1
}

int handle_client(int id, tcp_buffer *write_buf, char *msg, int len) {
    char *p = strtok(msg, " \r\n");
    int ret = 1;
    for (int i = 0; i < NCMD; i++)
        if (strcmp(p, cmd_table[i].name) == 0) {
            ret = cmd_table[i].handler(write_buf, p + strlen(p) + 1, len - strlen(p) - 1);
            break;
        }
    if (ret == 1) {
        static char unk[] = "Unknown command";
        send_to_buffer(write_buf, unk, sizeof(unk));
    }
    if (ret < 0) {
        return -1;
    }
}

void clear_client(int id) {
    // some code that are executed when a client is disconnected
    // you don't need this in step2
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
                "Usage: %s <disk file name> <cylinders> <sector per cylinder> "
                "<track-to-track delay> <port>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    // args
    char *diskfname = argv[1];
    ncyl = atoi(argv[2]);
    nsec = atoi(argv[3]);
    ttd = atoi(argv[4]);  // ms
    int port = atoi(argv[5]);

    // open file
    int fd = open(diskfname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("Error opening disk file");
        exit(EXIT_FAILURE);
    }
    // stretch the file
    long FILESIZE = ncyl * nsec * BLOCKSIZE;
    if (lseek(fd, FILESIZE - 1, SEEK_SET) == -1) {
        perror("Error calling lseek() to 'stretch' the file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    if (write(fd, "", 1) == -1) {
        perror("Error writing last byte of the file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    // mmap
    disk_data = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk_data == MAP_FAILED) {
        perror("Error mapping the file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // command
    tcp_server server = server_init(port, 1, add_client, handle_client, clear_client);
    server_loop(server);

    // Clean up
    if (munmap(disk_data, FILESIZE) == -1) {
        perror("Error un-mapping the file");
    }
    close(fd);
    return 0;
}
