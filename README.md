# mysh

**Note:** Designed for POSIX systems (Linux, macOS). To use this project on Windows, run under WSL with gcc and make both installed.

mysh is a unix-style shell written in C that supports pipelines, background jobs, POSIX exec, plus a handful of builtins, and a socket-based chat server/client to showcase networking and multiplexed I/O.

This project supports the following commands as builtins:

- echo
- ls
- cd
- cat
- wc
- pipes (|)
- background processes (&)
- kill
- ps
- exit
- start-server
- close-server
- send
- start-client
- All Bash commands (if not replaced by an already supported builtin)

## Getting Started

### Prerequisites

- POSIX-compatible OS
  - Linux
  - macOS
  - Windows **only** via WSL (Windows Subsytem for Linux)
    - Ubuntu recommended
- GNU C Compiler (gcc)
- GNU Make

### Installation

#### For Linux/macOS

1. Install gcc and make.

```
sudo apt update
sudo apt install build-essential
```

2. Clone the repo.

```
git clone
```

3. Build, then run the project.

_In the project repo:_

```
make
./mysh
```

#### For Windows

1. Open Powershell in **administrator mode**, then install WSL.

```
wsl --install Ubuntu-24.04 --web-download
```

2. Set up Linux user info by following the instructions in the shell.

3. Install gcc and make.

_In Powershell:_

```
wsl
sudo apt update
sudo apt install build-essential
```

4. Clone and navigate to the repo.

```
git clone
```

5. Build, then run the project.

_In the project repo:_

```
make
./mysh
```
