# Define compiler and compiler flags
CC = gcc
CFLAGS = -Wall

# Define object files for each executable
DISK_OBJ = BDS.o
FS_OBJ = FS.o
CLIENT_OBJ = FC.o

# Define executable names
DISK_EXEC = BDS
FS_EXEC = FS
CLIENT_EXEC = FC

# Define default target to build all executables
all: $(DISK_EXEC) $(FS_EXEC) $(CLIENT_EXEC)

# Define dependencies for each executable
$(DISK_EXEC): $(DISK_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(FS_EXEC): $(FS_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(CLIENT_EXEC): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Define how each object file should be built
BDS.o: BDS.c
	$(CC) $(CFLAGS) -c -o $@ $<

FS.o: FS.c
	$(CC) $(CFLAGS) -c -o $@ $<

FC.o: FC.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Define how to clean up object and executable files
clean:
	rm -f $(DISK_OBJ) $(FS_OBJ) $(CLIENT_OBJ) $(DISK_EXEC) $(FS_EXEC) $(CLIENT_EXEC) disk.log fs.log disk_file