// a basic disk server
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define BLOCKSIZE 256

//define valuable
int sockfd, newsockfd, portno;
socklen_t clilen;
char buffer[4096];
char *line = NULL;
char buf[4096];
char instr[BLOCKSIZE];
struct sockaddr_in serv_addr, cli_addr;
int n;
int* com_num;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int CYLINDERS, SECTORS, track_to_track_time, fd;
    char *disk_data;
    char *filename;

    if (argc != 6) {
        fprintf(stderr, "Usage: %s <DiskFileName> <#cylinders> <#sectors per cylinder> <track-to-track delay> <port>\n", argv[0]);
        exit(-1);
    }
    filename = argv[1];
    CYLINDERS = atoi(argv[2]);
    SECTORS = atoi(argv[3]);
    track_to_track_time = atoi(argv[4]);
    portno = atoi(argv[5]);
    long FILESIZE = BLOCKSIZE * SECTORS * CYLINDERS;

    FILE * fd_log = fopen("disk.log", "w");
    if (fd_log == NULL) {
        perror("Error: Could not open file 'fs.log'.\n");
    }
    
    // Open disk file and map it into memory
    fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        printf("Error: Could not open file '%s'.\n", filename);
        exit(-1);
    }
    if (lseek(fd, FILESIZE - 1, SEEK_SET) == -1) {
        printf("Error calling lseek() to 'stretch' the file\n");
        exit(-1);
    }
    if (write(fd, "", 1) == -1) {
        printf("Error writing last byte of the file\n");
        exit(-1);
    }
    disk_data = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk_data == MAP_FAILED) {
        close(fd);
        printf("Error:Could not map file.\n");
        exit(-1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    error("Error opening socket");
    printf("Accepting connections ...\n");
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("Error on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
        error("Error on accept");
    printf("New client(%d) is added\n", cli_addr.sin_port);

    do{
        n = read(newsockfd, buffer, 4095);
        if (n < 0) error("Error reading from socket");
        buffer[n] = '\0';

        //redirect to client
        dup2(newsockfd, STDOUT_FILENO);

        // Process requests
        char cmd;
        int c, s;
        int last_c = 0;
        char data[BLOCKSIZE] = "";
        if(sscanf(buffer, "%c", &cmd) < 1)continue;
        strcpy(buf, buffer+2);
        if (cmd == 'E') {
            // Exit
            // printf("Goodbye!\n");
            close(newsockfd);
            return 1;
        }
        else if (cmd == 'I') {
            // Information request
            fprintf(fd_log, "command: I\n");
            printf("%d %d\n", CYLINDERS, SECTORS);
            fprintf(fd_log, "%d %d\n", CYLINDERS, SECTORS);
        } 
        else if (cmd == 'R') {
            // Read request
            sscanf(buf, "%d %d", &c, &s);
            fprintf(fd_log, "command: R %d %d\n", c, s);
            if (c < 0 || c >= CYLINDERS || s < 0 || s >= SECTORS) {
                printf("No\n");
            } 
            else {
                memcpy(data, disk_data + (c * SECTORS + s) * BLOCKSIZE, BLOCKSIZE);
                printf("%s\n", data);
                fprintf(fd_log, "Yes %s\n", data);
                usleep(track_to_track_time * abs(c - last_c));
            }
        }
        else if (cmd == 'W') {
            // Write request
            sscanf(buf, "%d %d %[^\n]", &c, &s, instr);
            fprintf(fd_log, "command: W %d %d %s\n", c, s, instr);
            if (c < 0 || c >= CYLINDERS || s < 0 || s >= SECTORS) {
                printf("No\n");
            }
            else {
                strcpy(data, instr);
                memcpy(disk_data + (c * SECTORS + s) * BLOCKSIZE, data, strlen(data));
                printf("Yes\n");
                fprintf(fd_log, "Yes\n");
                usleep(track_to_track_time * abs(c - last_c));
            }
        }
        else if (cmd == 'D') {
            // Delete block
            sscanf(buf, "%d %d", &c, &s);
            fprintf(fd_log, "command: D %d %d\n", c, s);
            if (c < 0 || c >= CYLINDERS || s < 0 || s >= SECTORS) {
                printf("No\n");
            } 
            else {
                strcpy(data, "\0");
                memcpy(disk_data + (c * SECTORS + s) * BLOCKSIZE, data, 1);
                printf("Yes\n");
                fprintf(fd_log, "Yes\n");
                usleep(track_to_track_time * abs(c - last_c));
            }
        }
        else if (cmd == 'C') {
            fprintf(fd_log, "command: C\n");
            // Clear all
            for(int i = 0; i < CYLINDERS; i ++){
                for(int j = 0; j < SECTORS; j ++){
                    strcpy(data, "");
                    memcpy(disk_data + (i * SECTORS + j) * BLOCKSIZE, data, 1);
                }
            }
            printf("Yes\n");
            fprintf(fd_log, "Yes\n");
        }
        else if (cmd == '\n') continue;
        else {
            printf("Instructions Error!\n");
            fprintf(fd_log, "Instructions Error!\n");
        }
        last_c = c;
        bzero(buffer, 4096);
        bzero(buf, 4096);
    }while(1);
    close(newsockfd);
    close(sockfd);
    
    // Unmap and close
    if (disk_data != NULL && munmap(disk_data, FILESIZE) == -1) {
        perror("munmap");
        return -1;
    }
    if (fd != -1 && close(fd) == -1) {
        perror("close");
        return -1;
    }
    fclose(fd_log);
    return 0;
}