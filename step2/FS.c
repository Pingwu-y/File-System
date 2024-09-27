// file system server
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_DATA_LENGTH 1000
#define MAX_INPUT_SIZE 1024
#define BLOCK_SIZE 256
#define MAX_FILENAME_LENGTH 100
#define MAX_FILE_ENTRIES 1000
#define MAX_PATH_LENGTH 100

//
int sockfd, newsockfd, portno;
socklen_t clilen;
char buffer[100000];
char *line = NULL;
char buf[1024];
struct sockaddr_in serv_addr, cli_addr;
int n, nk;
int* com_num;
FILE *fd_log;
struct hostent *server;

int disk_sockfd, disk_portno, disk_n;
struct sockaddr_in disk_serv_addr;
struct hostent *disk_server;
char disk_buffer[1024];
int file_count;

// 磁盘变量
int CYLINDERS;
int SECTORS;
#define NUM_BLOCKS 256

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

// 得到buffer中的有效string
char* getValidString(const char* buffer) {
    int len = strlen(buffer);
    int start = 0;
    int end = len - 1;

    // 分别移动start和end到字符串首尾
    while (start < len && isspace(buffer[start])) {
        start++;
    }
    while (end >= start && isspace(buffer[end])) {
        end--;
    }

    // 把有效内容拷贝到validStr内
    int strLen = end - start + 1;
    if (strLen <= 0) {
        return NULL;
    }
    char* validStr = (char*)malloc(sizeof(char) * (strLen + 1));
    if (validStr == NULL) {
        return NULL;
    }
    memcpy(validStr, buffer + start, strLen);
    validStr[strLen] = '\0';
    return validStr;
}



typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    char type; // “F”表示文件，“D”表示目录
    int start_block;
    int file_size;
    char last_modified[32]; 
} DirectoryEntry;

DirectoryEntry root_directory[MAX_FILE_ENTRIES];
char disk[NUM_BLOCKS][BLOCK_SIZE];
char current_directory[MAX_PATH_LENGTH] = "/";

// 将 Directory 序列化成字符串
char* serialize_directory_entry(DirectoryEntry dir_entry) {
    char* serialized_string = (char*)malloc(BLOCK_SIZE * sizeof(char));

    sprintf(serialized_string, "%s %c %d %d %s|",
            dir_entry.filename, 
            dir_entry.type, 
            dir_entry.start_block, 
            dir_entry.file_size, 
            dir_entry.last_modified);
    return serialized_string;
}

// 将字符串反序列化为 Directory
void deserialize_directory_entries(const char* serialized_string, DirectoryEntry* dir_entry) {
    if(strcmp(serialized_string, "\0") == 0){
        file_count = 0;
        return;
    }
    sscanf(serialized_string, "%d", &file_count);
    const char* current_position = serialized_string + 1;

    for (int i = 0; i < file_count; i++) {
        sscanf(current_position, "%s %c %d %d %[^|]",
            dir_entry[i].filename, 
            &dir_entry[i].type, 
            &dir_entry[i].start_block,
            &dir_entry[i].file_size, 
            dir_entry[i].last_modified);
        
        // 切换到下一个序列化字符串首
        current_position = strchr(current_position, '|');
        if (current_position == NULL) {
            printf("Invalid serialized_string format\n");
            return;
        }
        current_position++;
    }
}

// 查找第一个空闲的目录条目
int find_free_entry() {
    for (int i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (root_directory[i].filename[0] == '\0')
            return i;
    }
    return -1;
}

// 在磁盘中查找第一个空闲的数据块
int find_free_block() {
    for (int i = 1; i < NUM_BLOCKS; i++) {
        if (disk[i][0] == '\0')
            return i;
    }
    return -1;
}

// 合并数组str1和str2，用以连接目录路径和文件名
char* concat_strings(const char* str1, const char* str2) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    char* result = (char*)malloc((len1 + len2 + 1) * sizeof(char));

    strcpy(result, str1);
    strcat(result, str2);

    return result;
}

int find_file(const char* name) {
    char* filename= concat_strings(current_directory, name);
    for (int entry_index = 0; entry_index < MAX_FILE_ENTRIES; entry_index++) {
        if (strcmp(root_directory[entry_index].filename, filename) == 0) {
            free(filename);
            return entry_index;
        }
    }
    free(filename);
    return -1;
}

void create_file(const char* name, char type) {
    // printf("Creating file: name = %s, type = %c\n", name, type);

    // 连接当前目录和文件名
    char* filename = concat_strings(current_directory, name);
    // printf("Concatenated filename: %s\n", filename);

    // 检查文件名是否重复
    if (find_file(name) != -1) {
        printf("No: same name\n");
        free(filename);
        return;
    }
    
    // 查找空闲目录条目
    int entry_index = find_free_entry();
    if (entry_index == -1) {
        printf("No: full\n");
        free(filename);
        return;
    }
    // 将文件名和类型写入目录条目
    strcpy(root_directory[entry_index].filename, filename);
    root_directory[entry_index].type = type;
    // printf("New directory entry: index = %d, filename = %s, type = %c\n", entry_index, filename, type);

    // 如果是目录
    if (type == 'D') {
        root_directory[entry_index].start_block = 0;
        root_directory[entry_index].file_size = 0;
        printf("Yes\n");
    } 
    // 如果是文件
    else {
        root_directory[entry_index].start_block = find_free_block();
        strcpy(disk[root_directory[entry_index].start_block], "0");
        root_directory[entry_index].file_size = 1;
        // printf("New file: start_block = %d\n", root_directory[entry_index].start_block);

        sprintf(disk_buffer, "W %d %d 0\n",
                root_directory[entry_index].start_block / CYLINDERS,
                root_directory[entry_index].start_block % CYLINDERS);
        disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
        if (disk_n < 0) error("Error: Writing to disk");
        bzero(disk_buffer, 1024);
        disk_n = read(disk_sockfd, disk_buffer, 1023); // 预留1字节存字符串终止符\0
        if (disk_n < 0) error("Error: Reading from disk");
        printf("%s", disk_buffer);
        bzero(disk_buffer, 1024);
    }

    file_count++;
    // printf("File count updated: %d\n", file_count);

    // 更新最后修改时间
    time_t current_time;
    time(&current_time);
    strcpy(root_directory[entry_index].last_modified, ctime(&current_time));
    // 把换行符替换为字符串终止符\0
    root_directory[entry_index].last_modified[strlen(root_directory[entry_index].last_modified) - 1] = '\0';
    // printf("Last modified time updated: %s\n", root_directory[entry_index].last_modified);
    
    free(filename);
    // printf("File creation completed.\n");

}

void delete_file(const char* name, int isPrint) {
    char* filename = concat_strings(current_directory, name);
    int found = 0;

    for (int i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (strcmp(root_directory[i].filename, filename) == 0) {
            found = 1;

            // 如果是文件
            if (root_directory[i].type == 'F') {
                root_directory[i].filename[0] = '\0';
                root_directory[i].file_size = 0;
                free(filename);
                strcpy(disk[root_directory[i].start_block], "\0");

                sprintf(disk_buffer, "D %d %d \n", 
                        root_directory[i].start_block / CYLINDERS, 
                        root_directory[i].start_block % CYLINDERS);
                
                disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
                if (disk_n < 0) error("Error: Writing to disk");
                bzero(disk_buffer, 1024);
                disk_n = read(disk_sockfd, disk_buffer, 1023);
                if (disk_n < 0) error("Error: Reading from disk");
                printf("%s", disk_buffer);
                bzero(disk_buffer, 1024);
            } 
            // 如果是目录
            else if (root_directory[i].type == 'D') {
                // 检查目录是否为空
                int is_empty = 1;
                char* dir_prefix = concat_strings(filename, "/");
                for (int j = 0; j < MAX_FILE_ENTRIES; j++) {
                    if (strncmp(root_directory[j].filename, dir_prefix, strlen(dir_prefix)) == 0) {
                        is_empty = 0;
                        break;
                    }
                }

                if (is_empty) {
                    root_directory[i].filename[0] = '\0';
                    root_directory[i].file_size = 0;
                    free(filename);
                    if(isPrint){
                        printf("Yes\n");
                    }
                } else {
                    // 递归删除目录内容
                    for (int j = 0; j < MAX_FILE_ENTRIES; j++) {
                        if (strncmp(root_directory[j].filename, dir_prefix, strlen(dir_prefix)) == 0) {
                            char* sub_name = strdup(root_directory[j].filename + strlen(current_directory));
                            delete_file(sub_name, 0);
                            free(sub_name);
                        }
                    }
                    // 删除空目录
                    root_directory[i].filename[0] = '\0';
                    root_directory[i].file_size = 0;
                    free(filename);
                    if(isPrint){
                        printf("Yes\n");
                    }
                }
                free(dir_prefix);
            }
            
            file_count--;
            return;
        }
    }

    if (!found) {
        printf("No: File/directory does not exist\n");
        free(filename);
    }
}


void list_file() {
    int flag = 1; //
    char tmp[256] = "";
    char file_tmp[1024] = "";
    char dir_tmp[256] = "";

    char sorted_files[MAX_FILE_ENTRIES][MAX_FILENAME_LENGTH];
    char sorted_dirs[MAX_FILE_ENTRIES][MAX_FILENAME_LENGTH];
    int num_files = 0;
    int num_dirs = 0;
    
    //遍历根目录，分类文件和目录
    for (int i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (strncmp(root_directory[i].filename, current_directory, strlen(current_directory)) == 0) {
            flag = 0;
            if (root_directory[i].type == 'F') {
                strcpy(sorted_files[num_files], root_directory[i].filename);
                num_files++;
            } else {
                strcpy(sorted_dirs[num_dirs], root_directory[i].filename);
                num_dirs++;
            }
        }
    }

    //对文件名和目录名按照字母序排序
    for (int i = 0; i < num_files - 1; i++) {
        for (int j = i + 1; j < num_files; j++) {
            if (strcmp(sorted_files[i], sorted_files[j]) > 0) {
                char temp[MAX_FILENAME_LENGTH];
                strcpy(temp, sorted_files[i]);
                strcpy(sorted_files[i], sorted_files[j]);
                strcpy(sorted_files[j], temp);
            }
        }
    }
    for (int i = 0; i < num_dirs - 1; i++) {
        for (int j = i + 1; j < num_dirs; j++) {
            if (strcmp(sorted_dirs[i], sorted_dirs[j]) > 0) {
                char temp[MAX_FILENAME_LENGTH];
                strcpy(temp, sorted_dirs[i]);
                strcpy(sorted_dirs[i], sorted_dirs[j]);
                strcpy(sorted_dirs[j], temp);
            }
        }
    }

    // 构建目录和文件的打印字符串
    for (int i = 0; i < num_files; i++) {
        sprintf(tmp, "%s, Size:%d, Last Modified Time:%s", sorted_files[i], root_directory[i].file_size, root_directory[i].last_modified);
        strcat(file_tmp, tmp);
        if (i < num_files - 1) {
            strcat(file_tmp, " ");
        }
    }
    for (int i = 0; i < num_dirs; i++) {
        sprintf(tmp, "%s", sorted_dirs[i]);
        strcat(dir_tmp, tmp);
        if (i < num_dirs - 1) {
            strcat(dir_tmp, " ");
        }
    }

    if (flag) printf("\n"); 
    else {
        if (dir_tmp[0] != '\0') {
            strcat(file_tmp, "\n&\n");
        }
        strcat(file_tmp, dir_tmp);
        printf("%s\n", file_tmp);
    }
}

void change_dir(char* path) {
    // "cd ..", 更换到父目录
    // if (strcmp(path, "..") == 0) {
    //     strcpy(current_directory, strrchr(current_directory, '/'));
    //     printf("Yes: %s\n", current_directory);
    // } 
    // "cd ..", 更换到父目录
    if (strcmp(path, "..") == 0) {
        // 找到最后一个斜杠的位置
        char* last_slash = strrchr(current_directory, '/');
        if (last_slash != NULL && last_slash != current_directory) {
            // 如果不是根目录，将最后一个斜杠替换为字符串结束符
            *last_slash = '\0';
            last_slash = strrchr(current_directory, '/');
            if (last_slash != NULL && last_slash != current_directory) {
                *(last_slash + 1) = '\0'; // 保持末尾的斜杠
            } else {
                strcpy(current_directory, "/"); // 根目录处理
            }
        } else {
            // 如果是根目录，将其保持为根目录
            strcpy(current_directory, "/");
        }
        printf("Yes: %s\n", current_directory);
    }
    // "cd .", 保持当前目录
    else if (strcmp(path, ".") == 0) {
        printf("Yes: %s\n", current_directory);
    }
    // "cd /", 切换到根目录
    else if (strcmp(path, "/") == 0) {
        strcpy(current_directory, "/");
        printf("Yes: %s\n", current_directory);
    }
    // 其他路径
    else {
        if (root_directory[find_file(path)].type == 'D' && path[0] != '\0') {
            strcat(current_directory, path);
            strcat(current_directory, "/");
            printf("Yes: %s\n", current_directory);
        }
        else {
            printf("No: No such Directory\n");
            return;
        }
    }
}

void write_file(const char* name, int len, const char* data) {
    char* filename= concat_strings(current_directory, name);

    for (int i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (strcmp(root_directory[i].filename, filename) == 0 && root_directory[i].type == 'F') {
            int start_block = root_directory[i].start_block;

            strncpy(disk[start_block], data, len);
            root_directory[i].file_size = len;
            
            sprintf(disk_buffer, "W %d %d %s\n", 
                    root_directory[i].start_block / CYLINDERS,
                    root_directory[i].start_block % CYLINDERS,
                    data);
            disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
            if (disk_n < 0) error("Error: Writing to disk");
            bzero(disk_buffer, 1024);
            disk_n = read(disk_sockfd, disk_buffer, 1023);
            if (disk_n < 0) error("Error: Reading from disk");
            printf("%s", disk_buffer);
            bzero(disk_buffer, 1024);

            // 更新最后修改时间    
            time_t current_time;
            time(&current_time);
            strcpy(root_directory[i].last_modified, ctime(&current_time));
            root_directory[i].last_modified[strlen(root_directory[i].last_modified) - 1] = '\0';

            free(filename);
            return;
        }
    }

    free(filename);
    printf("No: The file does not exist or is not a file\n");
}

void read_file(const char* name) {
    char* filename= concat_strings(current_directory, name);

    for (int i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (strcmp(root_directory[i].filename, filename) == 0 && root_directory[i].type == 'F') {
            int start_block = root_directory[i].start_block;
            int file_size = root_directory[i].file_size;

            printf("Yes: ");
            for (int block = start_block; block < start_block + (file_size / BLOCK_SIZE) + 1; block++) {
                printf("%.*s", file_size - (block - start_block) * BLOCK_SIZE, disk[block]);
            }
            printf("\n");

            free(filename);
            return;
        }
    }

    free(filename);
    printf("No: The file does not exist or is not a file\n");
}

void insert_file(const char* name, int position, int length, const char* data) {
    char* filename= concat_strings(current_directory, name);
    for (int i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (strcmp(root_directory[i].filename, filename) == 0 && root_directory[i].type == 'F') {
            int start_block = root_directory[i].start_block;  
            // 如果pos大于文件长度，插到文件末尾
            if (position > root_directory[i].file_size)
                position = root_directory[i].file_size;

            if (root_directory[i].file_size + length > MAX_DATA_LENGTH) {
                printf("Error: Inserted data exceeds maximum file size.\n");
                return;
            }
                
            memmove(disk[start_block] + position + length, disk[start_block] + position, root_directory[i].file_size - position);
            strncpy(disk[start_block] + position, data, length);
            root_directory[i].file_size += length;

            sprintf(disk_buffer, "W %d %d %s\n", 
                    root_directory[i].start_block / CYLINDERS,
                    root_directory[i].start_block % CYLINDERS, 
                    disk[start_block]);
            disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
            if (disk_n < 0) error("Error: Writing to disk");
            bzero(disk_buffer, 1024);
            disk_n = read(disk_sockfd, disk_buffer, 1023);
            if (disk_n < 0) error("Error: Reading from disk");
            printf("%s", disk_buffer);
            bzero(disk_buffer, 1024);
            
            // 更新最后修改时间
            time_t current_time;
            time(&current_time);
            strcpy(root_directory[i].last_modified, ctime(&current_time));
            root_directory[i].last_modified[strlen(root_directory[i].last_modified) - 1] = '\0';

            free(filename);
            return;
        }
    }
    free(filename);
    printf("No: The file does not exist or is not a file\n");
}

void delete_content(const char* name, int position, int length) {
    char* filename= concat_strings(current_directory, name);
    for (int i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (strcmp(root_directory[i].filename, filename) == 0 && root_directory[i].type == 'F') {
            int start_block = root_directory[i].start_block;  
            if (position >= root_directory[i].file_size) {
                printf("Error: Invalid position.\n");
                return;
            }

            if (position + length > root_directory[i].file_size)
                length = root_directory[i].file_size - position;

            memmove(disk[start_block] + position, disk[start_block] + position + length, root_directory[i].file_size - position - length);
            root_directory[i].file_size -= length;
            
            sprintf(disk_buffer, "W %d %d %s\n",
                    root_directory[i].start_block / CYLINDERS, 
                    root_directory[i].start_block % CYLINDERS, 
                    disk[start_block]);
            disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
            if (disk_n < 0) error("Error reading from socket");
            bzero(disk_buffer, 1024);
            disk_n = read(disk_sockfd, disk_buffer, 1023);
            if (disk_n < 0) error("Error reading from socket");
            printf("%s", disk_buffer);
            bzero(disk_buffer, 1024);
            
            // 更新最后修改时间
            time_t current_time;
            time(&current_time);
            strcpy(root_directory[i].last_modified, ctime(&current_time));
            root_directory[i].last_modified[strlen(root_directory[i].last_modified) - 1] = '\0';

            free(filename);
            return;
        }
    }
    free(filename);
    printf("No: The file does not exist or is not a file\n");
}

// 将当前目录表保存到磁盘中
void store_table() {
    char entry_table[BLOCK_SIZE] = "";
    if (file_count != 0){
        sprintf(entry_table, "%d", file_count);
        for(int i = 0; i < MAX_FILE_ENTRIES; i++){
            if(strcmp(root_directory[i].filename , "\0") == 0) continue;
            strncat(entry_table, serialize_directory_entry(root_directory[i]), strlen(serialize_directory_entry(root_directory[i])));
        }
        sprintf(disk_buffer, "W %d %d %s\n", 0, 0, entry_table);
        disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
        if (disk_n < 0) error("Error: Writing to disk");
    }
    //如果fs中没有文件，则清空磁盘
    else{
        sprintf(disk_buffer, "C\n");
        disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
        if (disk_n < 0) error("Error: Writing to disk");
    }
}

void clear_all() {
    for(int i = 0; i < MAX_FILE_ENTRIES; i++){
        file_count = 0;
        strcpy(root_directory[i].filename, "");
        if(root_directory[i].type == 'F')
            strcpy(disk[root_directory[i].start_block], "\0");
        root_directory[i].file_size = 0;
    }

    sprintf(disk_buffer, "C\n");
    disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
    if (disk_n < 0) error("Error: Writing to disk");
    bzero(disk_buffer, 1024);
    disk_n = read(disk_sockfd, disk_buffer, 1023);
    if (disk_n < 0) error("Error: Reading from disk");
    printf("Done\n");
    bzero(disk_buffer, 1024);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
       fprintf(stderr,"usage %s <DiskServerAddress> <BDSPort> <FSPort>\n", argv[0]);
       exit(0);
    }

    // 连接磁盘服务器
    disk_portno = atoi(argv[2]);
    disk_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (disk_sockfd < 0) 
        error("ERROR opening socket");
    disk_server = gethostbyname(argv[1]);
    if (disk_server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &disk_serv_addr, sizeof(disk_serv_addr));
    disk_serv_addr.sin_family = AF_INET;
    bcopy((char *)disk_server->h_addr, 
         (char *)&disk_serv_addr.sin_addr.s_addr,
         disk_server->h_length);
    disk_serv_addr.sin_port = htons(disk_portno);
    if (connect(disk_sockfd,(struct sockaddr *) &disk_serv_addr,sizeof(disk_serv_addr)) < 0) 
        error("ERROR connecting");
    
    // 监听客户端连接
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("Error opening socket");
    printf("Accepting connections ...\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[3]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("Error on binding");
    listen(sockfd,5);

    // set log
    FILE *fd_log = fopen("fs.log", "w");
    if (fd_log == NULL) {
        perror("Error: Could not open file 'fs.log'.\n");
        exit(-1);
    }

    // 接受客户端连接
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
        error("Error on accept");
    fprintf(fd_log, "New client(%d) is added\n", cli_addr.sin_port);

    // 获取磁盘的柱面和扇区数
    strcpy(disk_buffer, "I\n");
    disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
    if (disk_n < 0) error("Error: Writing to disk");
    disk_n = read(disk_sockfd, disk_buffer, 1024);
    if (disk_n < 0) error("Error: Reading from disk");
    sscanf(disk_buffer, "%d %d", &CYLINDERS, &SECTORS);
    bzero(disk_buffer, 1024);

    // 初始化文件分配表
    sprintf(disk_buffer, "R %d %d\n", 0, 0);
    disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
    if (disk_n < 0) error("Error writing to socket");
    disk_n = read(disk_sockfd, disk_buffer, 1024);
    if (disk_n < 0) error("Error reading from socket");
    deserialize_directory_entries(getValidString(disk_buffer), root_directory);
    bzero(disk_buffer, 1024);

    // 将磁盘数据加载到内存
    for(int i = 1; i < CYLINDERS * SECTORS; i++){
        sprintf(disk_buffer, "R %d %d\n", i/CYLINDERS, i%CYLINDERS);
        disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
        if (disk_n < 0) error("Error: Writing to socket");
        bzero(disk_buffer, 1024);
        disk_n = read(disk_sockfd, disk_buffer, 1023);
        if (disk_n < 0) error("Error: Reading from socket");
        if(disk_buffer[0] == '\n') disk_buffer[0] = '\0';
        strncpy(disk[i], disk_buffer, BLOCK_SIZE);
        bzero(disk_buffer, 1024);
    }
    
    // 处理客户端命令
    do{
        // printf("FS:%s$ ", current_directory);
        // fflush(stdout);

        char command[MAX_INPUT_SIZE] = "";
        char arg1[MAX_INPUT_SIZE] = "";
        char arg2[MAX_INPUT_SIZE] = "";
        char arg3[MAX_INPUT_SIZE] = "";
        char arg4[MAX_INPUT_SIZE] = "";

        n = read(newsockfd, buffer, 255);
        if (n < 0) error("Error reading from socket");

        //redirect to client
        dup2(newsockfd, STDOUT_FILENO);
        
        line = buffer;    
        // 空操作
        if(line == NULL){
            write(newsockfd, "No Command\n", 10);
            continue;
        }      
        
         if (fd_log != NULL) {
            fprintf(fd_log, "Receive from PORT(%d): %s", cli_addr.sin_port, buffer);
        }


        //execute command
        if (sscanf(line, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4) < 1) {
            continue;
        }

        if(strcmp(command, "e") == 0){
            store_table();
            sprintf(disk_buffer, "E\n");
            disk_n = write(disk_sockfd, disk_buffer, strlen(disk_buffer));
            if (disk_n < 0) error("Error reading from socket");
            fprintf(fd_log, "Client(%d) is closed\n", cli_addr.sin_port);
            close(newsockfd);
            exit(1);
            return 1;
        }
        if (strcmp(command, "f") == 0) {
            clear_all();
        } 
        else if (strcmp(command, "mk") == 0) {
            create_file(arg1, 'F');
        } 
        else if (strcmp(command, "mkdir") == 0) {
            create_file(arg1, 'D');
        } 
        else if (strcmp(command, "rm") == 0) {
            delete_file(arg1, 1);
        } 
        else if (strcmp(command, "rmdir") == 0) {
            delete_file(arg1, 1);
        } 
        else if (strcmp(command, "cd") == 0) {
            change_dir(arg1);
        }
        else if (strcmp(command, "ls") == 0) {
            list_file();
        }
        else if (strcmp(command, "pwd") == 0) {
            printf("%s\n", current_directory);
        }
        else if (strcmp(command, "cat") == 0) {
            read_file(arg1);
        }
        else if (strcmp(command, "w") == 0) {
            int len = atoi(arg2);
            sscanf(line, "%*s %*s %*s %[^\n]", arg3);
            write_file(arg1, len, arg3);
        }
        else if (strcmp(command, "i") == 0) {
            int pos = atoi(arg2);
            int len = atoi(arg3);
            sscanf(line, "%*s %*s %*s %*s %[^\n]", arg4);
            insert_file(arg1, pos, len, arg4);
        }
        else if (strcmp(command, "d") == 0) {
            int pos = atoi(arg2);
            int len = atoi(arg3);
            delete_content(arg1, pos, len);
        } 
        else {
            printf("Invalid command.\n");
        }

        // 每次循环清空buffer
        bzero(buffer,256);
        bzero(buf,1024);


    }while(1);
    free(line);
    close(sockfd);

    // log
    if (fd_log != NULL) {
        fclose(fd_log);
    }

    return 0;
}