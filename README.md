# File-System Project

## Overview
This project is a basic file system with disk storage and multi-user support implemented in C. It was developed as part of the **CS2303: Project Workshop of Operating Systems** course at **Shanghai Jiao Tong University**. The system includes a server-side disk management module and a client-side interface, enabling users to perform common file operations over a network using socket communication.
<img width="402" alt="image" src="https://github.com/user-attachments/assets/ebe715dc-f6d3-4233-83b1-495c1be20504">


## Features
- **Disk Storage System**: Supports disk initialization, reading, and writing using sector-based access.
- **File System**: Basic file operations including file creation, deletion, reading, and writing.
- **Multi-user Support**: Allows multiple users with individual home directories and file permissions.
- **Socket Communication**: Communication between the client and server is handled via TCP sockets.
- **Directory Management**: Provides functions for managing directories, including creation, listing, and deletion.

## Project Structure
### Disk Storage System
- **BDS.c**: The server that handles disk operations such as reading and writing data to disk blocks.
- **BDC.c**: The client that interacts with the disk server and sends commands for disk I/O operations.

### File System
- **File System Server**: Manages the file system, including the root directory, INode structure, and file data storage.
- **File System Client**: Allows users to interact with the file system, perform operations, and communicate with the server.

### Multi-user Support
- Supports user creation, deletion, and login.
- Users have their own file permissions and home directories.
- Each file is associated with an owner and permission settings (read-only or writable).

## File Operations
The following commands are implemented in the file system:
- **Initialize**: Prepares the disk and root directory for file operations.
- **Create**: Creates a file or directory.
- **Read**: Reads the content of a file.
- **Write**: Writes data to a file.
- **Delete**: Deletes a file or directory.
- **List**: Lists all files in the current directory.
- **Change Directory**: Navigates between directories.

## Multi-User Functionality
- **Login**: Users can log in and access their own files and directories.
- **Permissions**: Each file has specific read/write permissions associated with its owner.
- **File Ownership**: Files created by users are tagged with the user as the owner, and permissions control who can modify the file.

## How to Run
1. **Compile the code**:

```bash
gcc -o server BDS.c
gcc -o client BDC.c
gcc -o file_system_server FileSystemServer.c
gcc -o file_system_client FileSystemClient.c
```
3. **Start the disk server:**
```bash
./server disk_file cylinders sectors_per_cylinder track_to_track_delay port_number
```
3. **Start the file system server:**

```bash
./file_system_server port_number
```
4. Run the client:
```bash
./client port_number
```
