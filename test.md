# step1
## 1.1 Usage
### compile files
```shell
cd step1
make 
```
### Command line:
```shell
a) Server:
./BDS <DiskFileName> <#cylinders> <#sector per cylinder> <track-to-track delay> <port=10356>

b) Client:
./BDC <DiskServerAddress> <port=10356>
```
## 1.2 Commands
### Information Request
```shell
I
```
The disk returns two integers representing the disk geometry: the number of cylinders and the number of sectors per cylinder.
### Read Request
```shell
R <cylinder> <sector>
```
The disk returns "Yes" followed by a whitespace and the 256 bytes of information, or "No" if the block does not exist.
### Write Request
```shell
W <cylinder> <sector> <length> <data>
```
The disk returns "Yes" to the client if it is a valid write request (legal values of cylinder, sector, and length), or returns "No" otherwise.
## 1.3 Testing
### Start Server
```shell
./BDS disk.dat 10 20 5 10356
```

### Start Client
```shell
./BDC localhost 10356
```

### Test Cases
```shell
# On Client
I
# Expected Output
Yes: 10 20
# Valid Read Request
R 0 0
# Expected Output
Yes: (256 bytes of data)

# Invalid Read Request (out of bounds)
R 10 20
# Expected Output
No
# Valid Write Request
W 0 0 10 HelloWorld
# Expected Output
Yes

# Verify the Write
R 0 0
# Expected Output
Yes: HelloWorld (rest of the 256 bytes zero-padded)

# Invalid Write Request (out of bounds)
W 10 20 10 InvalidData
# Expected Output
No

# Invalid Write Request (length > 256)
W 0 0 300 TooLongData
# Expected Output
No

# Incorrect Command Syntax
InvalidCommand
# Expected Output
No

# Missing Arguments for Read
R 0
# Expected Output
No

# Missing Arguments for Write
W 0 0 10
# Expected Output
No

```

# step2
## 2.1 Usage
### compile files
```shell
cd step2
make 
```
### Command line:
```shell
Command line:
a) Disk Server:
./BDS <DiskFileName> <#cylinders> <#sectors per cylinder> <track-to-track delay> <port=10356>
b) File-system Server:
./FS <DiskServerAddress> <BDSPort=10356> <FSPort=12356>
c) File-system Client:
./FC <ServerAddr> <FSPort=12356>
```
## 2.2 Testing
```shell
cradle@ubuntu-PD:~/Desktop/prj3/Prj3_new/step2$ ./FC 10.211.55.10 10347
f
Done
ls

pwd
/
mkdir a1
Yes
mkdir d1
Yes
cd a1
Yes: /a1/
mkdir a2
Yes
cd a2
Yes: /a1/a2/
mk f.c  
Yes
w f. 15 Hello,world!
No: The file does not exist or is not a file
w f.c 15 Hello,World!
Yes
cat f.c
Yes: Hello,World!
i f.c 3 4 9999
Yes
cat f.c
Yes: Hel9999lo,World!
d f.c 4 7
Yes
cat f.c
Yes: Hel9orld!
pwd
/a1/a2/
cd ..
Yes: /a1/
cd ..
Yes: /
ls
/a1/a2/f.c, Size:0, Last Modified Time:Fri May 31 01:21:13 2024
&
/a1 /a1/a2 /d1
rmdir a1
Yes
Yes
ls

&
/d1
f
Done
ls

e
Client closed
```

# step3
## 3.1 Usage
### compile files
```shell
cd step3
make 
```
### Command line:
```shell
Command line:
a) Disk Server:
./BDS <DiskFileName> <#cylinders> <#sectors per cylinder> <track-to-track delay> <port=10356>
b) File-system Server:
./FS <DiskServerAddress> <BDSPort=10356> <FSPort=12356>
c) File-system Client:
./FC <ServerAddr> <FSPort=12356>
```
## 3.2 Commands
### list users
```shell
list_user
```
### create user
```shell
create_user <username> <password>
```
### delete user
```shell
delete_user <username> <password>
```

### log in
```shell
login <username> <password>
```
### log out 
```shell
logout <username> <password>
```

## 3.3 Testing
```shell
cradle@ubuntu-PD:~/Desktop/prj3/Prj3_new/step3$ ./FC 10.211.55.10 10365
ls

f
Done
pwd
/
create_user yjh 123
Yes: User created
create_user ljj 123
Yes: User created
login yjh 123
Yes: Logged in as yjh
pwd
/yjh/
mkdir a1
Yes
cd a1
Yes: /yjh/a1/
mkdir a2 
Yes
cd a2
Yes: /yjh/a1/a2/
mk yjh_f.c        
Yes
logout
Yes: Logged out
pwd
/
ls
/user/ljj.txt, Size:0, Last Modified Time:Thu May 30 23:44:41 2024 /user/yjh.txt, Size:0, Last Modified Time:Thu May 30 23:44:41 2024 /yjh/a1/a2/yjh_f.c, Size:13, Last Modified Time:Thu May 30 23:44:41 2024
&
/ljj /user /yjh /yjh/a1 /yjh/a1/a2
login ljj 123
Yes: Logged in as ljj
delete_user yjh 
Yes
Yes: User deleted
ls
/user/ljj.txt, Size:0, Last Modified Time:Thu May 30 23:44:41 2024
cd ..
Yes: /
ls
/user/ljj.txt, Size:0, Last Modified Time:Thu May 30 23:44:41 2024 /yjh/a1/a2/yjh_f.c, Size:0, Last Modified Time:Thu May 30 23:44:41 2024
&
/ljj /user /yjh /yjh/a1 /yjh/a1/a2
f
Done
ls

pwd
/
create_user yjh 123
Yes: User created
create_user ljj 123
No: User already exists
ls
/user/yjh.txt, Size:0, Last Modified Time:Thu May 30 23:47:05 2024
ls
/user/yjh.txt, Size:0, Last Modified Time:Thu May 30 23:47:05 2024
e
Client closed
```


















