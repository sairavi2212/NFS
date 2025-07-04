# Network File System (NFS)

This project implements a distributed Network File System with three main components:

1. **Client (CL)**: Interface for users to interact with the system
2. **Naming Server (NS)**: Central coordinator managing file metadata and storage server information
3. **Storage Server (SS)**: Responsible for storing and retrieving actual file data

## System Architecture

The system follows a distributed architecture where:

- **Naming Server** acts as a coordinator maintaining file paths, storage locations, and facilitating client-storage server communication
- **Storage Servers** store the actual file data and respond to read/write requests
- **Clients** interact with the system through a command-line interface

## Features

- File operations: create, read, write, append, delete, list, copy
- Directory operations: create, list, delete
- File metadata retrieval
- Fault tolerance through data replication
- Streaming capabilities for media files
- Synchronous and asynchronous write operations
- Load balancing across multiple storage servers

## Components

### Client
The client provides a command-line interface for users to interact with the system. Key operations include:
- READ: Read the contents of a file
- WRITE: Write/overwrite a file (synchronous or asynchronous)
- APPEND: Add content to an existing file
- CREATE_FILE: Create a new file
- CREATE_FOLDER: Create a new directory
- LIST: List all accessible files and directories
- DELETE: Delete a file or directory
- COPY: Copy a file or directory to a new location
- RETREIVE: Get metadata about a file
- STREAM: Stream media files

### Naming Server
The naming server manages:
- File path trie structure for efficient path lookup
- Storage server registration and health monitoring
- Assignment of files to storage servers
- Backup management for fault tolerance
- Request routing from clients to appropriate storage servers

### Storage Server
Storage servers handle:
- Physical storage and retrieval of file data
- Creation and deletion of files and directories
- Backup copies of files from other storage servers
- Health status reporting to the naming server

## Communication Protocol

The system uses a custom communication protocol with various message types for different operations. TCP sockets are used for reliable communication between components.

## Fault Tolerance

The system implements fault tolerance through:
- Data replication across multiple storage servers
- Backup paths for critical files
- Periodic health checks (pings) between the naming server and storage servers

## Concurrency Control

The implementation uses various mutex locks to handle concurrent access:
- File access locks to prevent conflicts during read/write operations
- Storage server management locks
- Path trie structure locks

## Steps to Run

### Compilation

1. Compile all the components:

   ```bash
   # Compile the Naming Server
   cd NFS/NS
   gcc -o ns ns.c init.c trie.c cache.c -lpthread
   
   # Compile the Storage Server
   cd ../SS
   gcc -o ss main.c -lpthread
   
   # Compile the Client (using the provided makefile)
   cd ../client
   make
   ```

   Note: If you encounter any compilation errors related to missing header files, you may need to adjust include paths using the `-I` flag to include the common directory.

### Execution

1. **Start the Naming Server (NS) first**:
   ```bash
   cd NFS/NS
   ./ns
   ```
   The naming server will start and wait for connections from storage servers and clients.

2. **Start one or more Storage Servers (SS)**:
   ```bash
   cd NFS/SS
   ./ss [directory_paths]
   ```
   Where `directory_paths` are the directories you want to expose through the file system.
   
   Example:
   ```bash
   ./ss folder1 folder2
   ```

3. **Start the Client and connect to the NFS**:
   ```bash
   cd NFS/client
   ./client
   ```
   This will open the client interface where you can enter commands.

### Using the Client

Once the client is running, you can use the following commands:

- `READ` - Read a file
- `WRITE` - Write to a file
- `APPEND` - Append to a file
- `CREATE_FILE` - Create a new file
- `CREATE_FOLDER` - Create a new directory
- `LIST` - List all accessible files and directories
- `DELETE` - Delete a file or directory
- `COPY` - Copy a file or directory
- `RETREIVE` - Get metadata about a file
- `STREAM` - Stream a media file
- `EXIT` - Exit the client

Example session:
```
Enter operation: LIST

Accessible Paths
folder1/file1.txt
folder1/file2.txt
folder2/file3.txt
End of paths

Enter operation: READ
...Enter file name: folder1/file1.txt
[Contents of the file will be displayed]

Enter operation: EXIT
```

### Network Configuration

By default, the system uses the following ports:
- Naming Server: 2000 (for clients), 2500 (for storage servers)
- Storage Servers: Configurable, default 4000
- The IP address of the Naming Server needs to be set in the client configuration

## Implementation Details

Written in C, the system utilizes various system calls and libraries including:
- Socket programming for network communication
- POSIX threads for concurrency
- File I/O operations for data storage