# ProcessHierarchy

A powerful Linux command-line utility for analyzing and manipulating process trees.

## Overview

ProcessHierarchy helps system administrators and developers explore process relationships, detect zombie processes, and manage process hierarchies. It provides various options to list, analyze, and control processes within a specific process subtree.

## Features

- **Process Tree Analysis**: Verify if a process belongs to a specific process tree
- **Relationship Inspection**: List immediate descendants, grandchildren, siblings, and non-direct descendants
- **Zombie Process Management**: Find and count defunct (zombie) processes, identify parents of zombies
- **Process Control**: Kill, stop, or continue entire process subtrees

## Usage

```
./processhierarchy [root_process] [process_id] [Option]
```

### Parameters

- `root_process`: PID of the root process defining the process tree
- `process_id`: Target process to analyze or manipulate
- `Option`: Optional command to execute (see below)

### Options

| Option | Description |
|--------|-------------|
| `-dc` | Count defunct (zombie) descendants |
| `-ds` | List non-direct descendants |
| `-id` | List immediate descendants (direct children) |
| `-lg` | List sibling processes |
| `-lz` | List defunct (zombie) siblings |
| `-df` | List defunct (zombie) descendants |
| `-gc` | List grandchildren |
| `-do` | Print process status (defunct or not) |
| `--pz` | Kill parents of zombie processes |
| `-sk` | Kill all descendants |
| `-st` | Stop (pause) all descendants |
| `-dt` | Continue (resume) stopped descendants |
| `-rp` | Terminate the root process |

## Examples

1. Check basic information about a process:
   ```
   ./processhierarchy 1 1234
   ```

2. List all immediate children of process 1234 in the tree rooted at process 1:
   ```
   ./processhierarchy 1 1234 -id
   ```

3. Count all zombie processes in the subtree:
   ```
   ./processhierarchy 1 1234 -dc
   ```

4. Kill all descendants of process 1234:
   ```
   ./processhierarchy 1 1234 -sk
   ```

## Technical Details

This utility works by analyzing the `/proc` filesystem to get process relationships and states. It constructs process trees by examining the parent-child relationships between processes.

The program uses several core system calls and libraries:
- Signal handling for process control (SIGKILL, SIGSTOP, SIGCONT)
- Directory operations for traversing the `/proc` filesystem
- Process information gathering from `/proc/[pid]/stat`

## Security Considerations

- This utility requires appropriate permissions to access process information and perform signal operations
- Some operations like killing processes may require elevated privileges
- Use caution when running destructive operations like `-sk` (kill all descendants)

## Compilation

Compile the program with:

```
gcc -o processhierarchy processhierarchy.c
```

## Dependencies

- Standard C libraries (stdio.h, stdlib.h, string.h)
- POSIX system libraries (unistd.h, sys/types.h, dirent.h, signal.h)

## Limitations

- Process relationships may change during execution
- Some operations may fail due to permission issues
- Maximum of 1024 descendants can be handled in a single operation
