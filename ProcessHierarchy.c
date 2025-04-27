#include <stdio.h>        // For standard input/output operations like printf
#include <stdlib.h>       // For atoi() to convert strings to integers
#include <string.h>       // For strcmp() to compare option strings
#include <unistd.h>       // For getuid() and other POSIX functions
#include <sys/types.h>    // Defines pid_t for process IDs
#include <dirent.h>       // For directory operations with /proc
#include <signal.h>       // For signal handling like SIGKILL, SIGSTOP
#include <ctype.h>        // For isdigit() to filter /proc entries
#include <errno.h>        // For errno and error handling

// Custom structure to hold process details fetched from /proc
typedef struct {
    pid_t pid;    // Process ID
    pid_t ppid;   // Parent Process ID
    char state;   // Process state (e.g., 'Z' for zombie)
} ProcessInfo;

// Function declarations for process tree operations
int is_in_tree(pid_t root_pid, pid_t target_pid);              // Checks if a PID is in a tree
ProcessInfo get_process_info(pid_t pid);                       // Fetches process info from /proc
void print_basic_info(pid_t pid);                              // Prints PID and PPID
int count_defunct_descendants(pid_t pid);                      // Counts zombie descendants
void list_non_direct_descendants(pid_t pid, pid_t parent);     // Lists non-direct descendants
void list_immediate_descendants(pid_t pid);                    // Lists direct children
void list_siblings(pid_t pid);                                 // Lists sibling processes
void list_defunct_siblings(pid_t pid);                         // Lists zombie siblings
void list_defunct_descendants(pid_t pid);                      // Lists zombie descendants
void list_grandchildren(pid_t pid);                            // Lists grandchildren
void print_status(pid_t pid);                                  // Prints process status
void kill_zombie_parents(pid_t pid);                           // Kills parents of zombies
void kill_descendants(pid_t pid);                              // Kills all descendants
void stop_descendants(pid_t pid);                              // Stops all descendants
void continue_descendants(pid_t pid);                          // Continues stopped descendants
void print_error(const char *msg, int errnum);                 // Custom error printer

int main(int argc, char *argv[]) {
    // Ensure correct argument count (3 or 4) for proper execution
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Error: Incorrect number of arguments\n");      // Alert user to argument issue
        fprintf(stderr, "Usage: %s [root_process] [process_id] [Option]\n", argv[0]); // Show correct format
        fprintf(stderr, "Example: %s 1234 5678 -id\n", argv[0]);       // Provide a practical example
        return 1;                                                      // Exit with failure code
    }

    // Convert command-line args to PIDs
    pid_t root_pid = atoi(argv[1]);   // Root of the process tree
    pid_t target_pid = atoi(argv[2]); // Target process to analyze
    char *option = (argc == 4) ? argv[3] : NULL; // Optional operation flag

    // Validate that PIDs are positive numbers
    if (root_pid <= 0 || target_pid <= 0) {
        fprintf(stderr, "Error: Process IDs must be positive integers\n"); // Reject invalid PIDs
        fprintf(stderr, "Got root_pid=%d, target_pid=%d\n", root_pid, target_pid); // Show bad values
        return 1;                                                         // Exit on invalid input
    }

    // Verify root process exists before proceeding
    ProcessInfo root_info = get_process_info(root_pid);
    if (root_info.pid == 0) {
        fprintf(stderr, "Error: Root process %d does not exist or is inaccessible\n", root_pid); // Root not found
        return 1;                                                                       // Abort if root invalid
    }

    // Check if target is in the tree rooted at root_pid
    if (!is_in_tree(root_pid, target_pid)) {
        if (option) {
            printf("Notice: Process %d does not belong to the tree rooted at %d\n", target_pid, root_pid); // Inform user
        }
        return 0; // Exit silently if no option, or with notice if option provided
    }

    // Handle no-option case: just print basic info
    if (!option) {
        print_basic_info(target_pid); // Display PID and PPID of target
        return 0;                    // Done, exit cleanly
    }

    // Process the provided option with a series of checks
    if (strcmp(option, "-dc") == 0) {
        int count = count_defunct_descendants(target_pid); // Count zombies
        if (count >= 0) {
            printf("Number of defunct descendants: %d\n", count); // Output result if successful
        }
    } else if (strcmp(option, "-ds") == 0) {
        list_non_direct_descendants(target_pid, target_pid); // List deeper descendants
    } else if (strcmp(option, "-id") == 0) {
        list_immediate_descendants(target_pid); // List direct children
    } else if (strcmp(option, "-lg") == 0) {
        list_siblings(target_pid); // List processes at same level
    } else if (strcmp(option, "-lz") == 0) {
        list_defunct_siblings(target_pid); // List zombie siblings only
    } else if (strcmp(option, "-df") == 0) {
        list_defunct_descendants(target_pid); // Show all zombie descendants
    } else if (strcmp(option, "-gc") == 0) {
        list_grandchildren(target_pid); // Display second-level descendants
    } else if (strcmp(option, "-do") == 0) {
        print_status(target_pid); // Show if process is defunct
    } else if (strcmp(option, "--pz") == 0) {
        kill_zombie_parents(target_pid); // Terminate parents of zombies
    } else if (strcmp(option, "-sk") == 0) {
        kill_descendants(target_pid); // Kill all descendants
    } else if (strcmp(option, "-st") == 0) {
        stop_descendants(target_pid); // Pause descendants
    } else if (strcmp(option, "-dt") == 0) {
        continue_descendants(target_pid); // Resume stopped descendants
    } else if (strcmp(option, "-rp") == 0) {
        if (kill(root_pid, SIGKILL) == -1) { // Attempt to kill root process
            print_error("Failed to kill root process", errno); // Report failure
        } else {
            printf("Root process %d terminated successfully\n", root_pid); // Confirm success
        }
    } else {
        fprintf(stderr, "Error: Invalid option '%s'\n", option); // Unknown option error
        fprintf(stderr, "Valid options: -dc, -ds, -id, -lg, -lz, -df, -gc, -do, --pz, -sk, -st, -dt, -rp\n"); // List all options
        return 1; // Exit with error
    }

    return 0; // Successful execution
}

// Custom error reporting function with detailed message
void print_error(const char *msg, int errnum) {
    fprintf(stderr, "Error: %s: %s\n", msg, strerror(errnum)); // Combine custom message with system error
}

// Retrieve process details from /proc filesystem
ProcessInfo get_process_info(pid_t pid) {
    ProcessInfo info = {0, 0, ' '}; // Initialize with zeroes and blank state
    char path[32];                  // Buffer for /proc path
    snprintf(path, sizeof(path), "/proc/%d/stat", pid); // Construct path to stat file

    FILE *fp = fopen(path, "r"); // Open process stat file
    if (!fp) {
        return info; // Return empty info if file inaccessible (e.g., no perms)
    }

    // Read PID, skip command, get state and PPID
    if (fscanf(fp, "%d %*s %c %d", &info.pid, &info.state, &info.ppid) != 3) {
        fclose(fp);  // Close file on read failure
        return info; // Return empty if parsing fails
    }
    fclose(fp);      // Clean up file handle
    return info;     // Return populated info
}

// Determine if target_pid is in the tree rooted at root_pid
int is_in_tree(pid_t root_pid, pid_t target_pid) {
    if (root_pid == target_pid) return 1; // Base case: same process

    ProcessInfo info = get_process_info(target_pid); // Get target’s info
    if (info.pid == 0) return 0;                    // Target doesn’t exist

    pid_t current = info.ppid; // Start at parent
    int iterations = 0;        // Counter to avoid infinite loops
    while (current != 0 && iterations++ < 1000) { // Traverse up to root or limit
        if (current == root_pid) return 1;        // Found root in chain
        info = get_process_info(current);         // Move to next parent
        current = info.ppid;                      // Update current PID
    }
    return 0; // Root not found in chain
}

// Display basic process info for no-option case
void print_basic_info(pid_t pid) {
    ProcessInfo info = get_process_info(pid); // Fetch process details
    if (info.pid == 0) {
        fprintf(stderr, "Error: Cannot get information for process %d\n", pid); // Alert if inaccessible
        return;
    }
    printf("PID: %d, PPID: %d\n", info.pid, info.ppid); // Print requested info
}

// Count zombie processes in the tree
int count_defunct_descendants(pid_t pid) {
    int count = 0; // Initialize zombie counter
    DIR *dir = opendir("/proc"); // Open process directory
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Report dir access failure
        return -1;                                           // Indicate error
    }

    struct dirent *entry; // Directory entry pointer
    while ((entry = readdir(dir)) != NULL) { // Iterate through /proc
        if (!isdigit(*entry->d_name)) continue; // Skip non-numeric entries
        pid_t curr_pid = atoi(entry->d_name);  // Convert name to PID
        ProcessInfo info = get_process_info(curr_pid); // Get process info
        if (info.pid == 0) continue;                  // Skip if process vanished
        if (is_in_tree(pid, curr_pid) && info.state == 'Z') { // Check tree and zombie state
            count++;                                          // Increment for each zombie
        }
    }
    closedir(dir); // Release directory handle
    return count;  // Return total zombies found
}

// List processes deeper than direct children
void list_non_direct_descendants(pid_t pid, pid_t parent) {
    DIR *dir = opendir("/proc"); // Access process list
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Handle dir open error
        return;
    }

    struct dirent *entry; // Entry for iteration
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Ignore non-PID entries
        pid_t curr_pid = atoi(entry->d_name);  // Parse PID
        ProcessInfo info = get_process_info(curr_pid); // Fetch details
        if (info.pid == 0) continue;                  // Skip missing processes
        // Print if in tree, not direct child, and not the root
        if (is_in_tree(pid, curr_pid) && info.ppid != parent && curr_pid != pid) {
            printf("%d\n", curr_pid);
        }
    }
    closedir(dir); // Clean up
}

// Show immediate children of the process
void list_immediate_descendants(pid_t pid) {
    DIR *dir = opendir("/proc"); // Open /proc for scanning
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Dir access failed
        return;
    }

    struct dirent *entry; // Directory iterator
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Filter out non-PIDs
        pid_t curr_pid = atoi(entry->d_name);  // Extract PID
        ProcessInfo info = get_process_info(curr_pid); // Get process data
        if (info.pid == 0) continue;                  // Skip if not found
        if (info.ppid == pid) {                       // Direct child check
            printf("%d\n", curr_pid);                 // Output child PID
        }
    }
    closedir(dir); // Close directory
}

// List processes sharing the same parent
void list_siblings(pid_t pid) {
    ProcessInfo info = get_process_info(pid); // Get target’s parent info
    if (info.pid == 0) {
        fprintf(stderr, "Error: Cannot get information for process %d\n", pid); // Target inaccessible
        return;
    }

    DIR *dir = opendir("/proc"); // Open process directory
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Report access issue
        return;
    }

    struct dirent *entry; // Iterate through processes
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Skip non-numeric
        pid_t curr_pid = atoi(entry->d_name);  // Parse PID
        if (curr_pid == pid) continue;         // Exclude self
        ProcessInfo curr_info = get_process_info(curr_pid); // Get sibling info
        if (curr_info.ppid == info.ppid) {                  // Same parent check
            printf("%d\n", curr_pid);                       // Print sibling PID
        }
    }
    closedir(dir); // Free dir resource
}

// List only zombie siblings
void list_defunct_siblings(pid_t pid) {
    ProcessInfo info = get_process_info(pid); // Fetch target process details
    if (info.pid == 0) {
        fprintf(stderr, "Error: Cannot get information for process %d\n", pid); // Error if not found
        return;
    }

    DIR *dir = opendir("/proc"); // Access /proc filesystem
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Dir error
        return;
    }

    struct dirent *entry; // Directory entry iterator
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Ignore non-PIDs
        pid_t curr_pid = atoi(entry->d_name);  // Convert to PID
        if (curr_pid == pid) continue;         // Skip target itself
        ProcessInfo curr_info = get_process_info(curr_pid); // Get sibling data
        if (curr_info.ppid == info.ppid && curr_info.state == 'Z') { // Check sibling and zombie
            printf("%d\n", curr_pid);                                // Print zombie sibling
        }
    }
    closedir(dir); // Clean up directory
}

// List all zombie descendants except the target
void list_defunct_descendants(pid_t pid) {
    DIR *dir = opendir("/proc"); // Open process info directory
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Handle open failure
        return;
    }

    struct dirent *entry; // Iterator for /proc entries
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Skip non-numeric entries
        pid_t curr_pid = atoi(entry->d_name);  // Parse PID
        ProcessInfo info = get_process_info(curr_pid); // Fetch process info
        if (info.pid == 0) continue;                  // Skip if not found
        // Check if descendant, zombie, and not target
        if (is_in_tree(pid, curr_pid) && info.state == 'Z' && curr_pid != pid) {
            printf("%d\n", curr_pid);                 // Output zombie PID
        }
    }
    closedir(dir); // Release directory handle
}

// List all grandchildren of the target process
void list_grandchildren(pid_t pid) {
    DIR *dir = opendir("/proc"); // Start scanning processes
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Report dir failure
        return;
    }
    struct dirent *entry; // Outer loop for children
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Skip non-PIDs
        pid_t child_pid = atoi(entry->d_name); // Get child PID
        ProcessInfo child_info = get_process_info(child_pid); // Child process info
        if (child_info.ppid != pid) continue;                // Not a direct child

        DIR *dir2 = opendir("/proc"); // Inner loop for grandchildren
        if (!dir2) {
            print_error("Cannot access /proc directory for grandchildren", errno); // Inner dir error
            closedir(dir);                                                 // Clean up outer dir
            return;
        }

        struct dirent *entry2; // Iterate grandchildren
        while ((entry2 = readdir(dir2)) != NULL) {
            if (!isdigit(*entry2->d_name)) continue; // Filter out non-PIDs
            pid_t grand_pid = atoi(entry2->d_name); // Parse grandchild PID
            if (get_process_info(grand_pid).ppid == child_pid) { // Check if child’s child
                printf("%d\n", grand_pid);                       // Print grandchild
            }
        }
        closedir(dir2); // Close inner directory
    }
    closedir(dir); // Close outer directory
}

// Display whether the process is defunct or active
void print_status(pid_t pid) {
    ProcessInfo info = get_process_info(pid); // Get process state
    if (info.pid == 0) {
        fprintf(stderr, "Error: Cannot get status for process %d\n", pid); // Status fetch failed
        return;
    }
    printf("Process %d is %s\n", pid, (info.state == 'Z') ? "Defunct" : "Not Defunct"); // Report status
}

// Terminate parents of zombie descendants
void kill_zombie_parents(pid_t pid) {
    DIR *dir = opendir("/proc"); // Open /proc for process scan
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Dir access error
        return;
    }

    struct dirent *entry; // Directory entry iterator
    int killed_any = 0;   // Flag for any kills performed

    while ((entry = readdir(dir)) != NULL) { // Scan all processes
        if (!isdigit(*entry->d_name)) continue; // Skip non-numeric
        pid_t curr_pid = atoi(entry->d_name);  // Convert to PID
        ProcessInfo info = get_process_info(curr_pid); // Get process details
        if (info.pid == 0 || !is_in_tree(pid, curr_pid)) continue; // Skip if not in tree

        if (info.state == 'Z' && info.ppid != 0) { // Found a zombie with a parent
            pid_t parent_to_kill = info.ppid;     // Target its parent
            if (kill(parent_to_kill, SIGKILL) == -1) { // Attempt kill
                char msg[64];                         // Buffer for custom error
                snprintf(msg, sizeof(msg), "Failed to kill parent %d of zombie %d", parent_to_kill, curr_pid);
                print_error(msg, errno);              // Report kill failure
            } else {
                printf("Killed parent %d of zombie process %d\n", parent_to_kill, curr_pid); // Success message
                killed_any = 1;                       // Mark that we killed something
            }
        }
    }
    closedir(dir); // Clean up

    if (!killed_any) { // If no zombies were found to act on
        printf("No zombie processes found among descendants of %d\n", pid); // Inform user
    }
}
// Terminate all descendants with SIGKILL, ensuring grandchildren are included
void kill_descendants(pid_t pid) {
    DIR *dir = opendir("/proc"); // Open /proc to scan processes
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Report if /proc unavailable
        return;
    }

    // Array to store descendants (arbitrary max size; adjust as needed)
    pid_t descendants[1024]; // Buffer for PIDs
    int descendant_count = 0; // Track number of descendants found
    struct dirent *entry;     // Directory entry iterator

    // First pass: Collect all descendants into an array
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Skip non-numeric entries
        pid_t curr_pid = atoi(entry->d_name);  // Parse PID from dir name
        ProcessInfo info = get_process_info(curr_pid); // Fetch process details
        if (info.pid == 0) continue;                  // Skip if process doesn’t exist
        if (is_in_tree(pid, curr_pid) && curr_pid != pid) { // In tree, not target
            if (descendant_count < 1024) {                    // Check buffer limit
                descendants[descendant_count++] = curr_pid;   // Add to list
            } else {
                fprintf(stderr, "Warning: Too many descendants; some may be missed\n"); // Warn on overflow
                break;
            }
        }
    }
    closedir(dir); // Close initial scan

    // Second pass: Kill from deepest to shallowest by iterating backwards
    for (int i = descendant_count - 1; i >= 0; i--) {
        pid_t curr_pid = descendants[i]; // Get PID to kill
        if (kill(curr_pid, SIGKILL) == -1) { // Send SIGKILL
            char msg[64];                    // Buffer for error message
            snprintf(msg, sizeof(msg), "Failed to kill descendant %d", curr_pid);
            print_error(msg, errno);         // Report kill failure
        } else {
            printf("Killed descendant %d\n", curr_pid); // Confirm successful kill
        }
    }

    // Optional: Re-scan to catch any missed descendants (e.g., new forks)
    dir = opendir("/proc"); // Re-open /proc for verification
    if (dir) {
        int missed = 0; // Count of missed descendants
        while ((entry = readdir(dir)) != NULL) {
            if (!isdigit(*entry->d_name)) continue;
            pid_t curr_pid = atoi(entry->d_name);
            ProcessInfo info = get_process_info(curr_pid);
            if (info.pid == 0) continue;
            // Check if still alive and in tree
            if (is_in_tree(pid, curr_pid) && curr_pid != pid) {
                missed++;
                if (kill(curr_pid, SIGKILL) == -1) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Failed to kill missed descendant %d", curr_pid);
                    print_error(msg, errno);
                } else {
                    printf("Killed missed descendant %d\n", curr_pid);
                }
            }
        }
        if (missed > 0) {
            fprintf(stderr, "Note: %d descendants were missed in first pass and killed in second\n", missed); // Inform user
        }
        closedir(dir); // Clean up
    }
}

// Pause all descendants with SIGSTOP
void stop_descendants(pid_t pid) {
    DIR *dir = opendir("/proc"); // Open /proc directory
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Access error
        return;
    }

    struct dirent *entry; // Process iterator
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Skip non-numeric
        pid_t curr_pid = atoi(entry->d_name);  // Extract PID
        ProcessInfo info = get_process_info(curr_pid); // Fetch data
        if (info.pid == 0) continue;                  // Skip missing
        if (is_in_tree(pid, curr_pid) && curr_pid != pid) { // Check descendant
            if (kill(curr_pid, SIGSTOP) == -1) {            // Attempt stop
                print_error("Failed to stop descendant process", errno); // Stop failed
            } else {
                printf("Stopped descendant %d\n", curr_pid); // Confirm stop
            }
        }
    }
    closedir(dir); // Clean up
}

// Resume stopped descendants with SIGCONT
void continue_descendants(pid_t pid) {
    DIR *dir = opendir("/proc"); // Open process directory
    if (!dir) {
        print_error("Cannot access /proc directory", errno); // Dir error
        return;
    }

    struct dirent *entry; // Iterator for /proc entries
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue; // Ignore non-PIDs
        pid_t curr_pid = atoi(entry->d_name);  // Parse PID
        ProcessInfo info = get_process_info(curr_pid); // Get process info
        if (info.pid == 0) continue;                  // Skip if gone
        // Check if descendant, not self, and stopped
        if (is_in_tree(pid, curr_pid) && curr_pid != pid && info.state == 'T') {
            if (kill(curr_pid, SIGCONT) == -1) {      // Try to continue
                print_error("Failed to continue descendant process", errno); // Report failure
            } else {
                printf("Continued descendant %d\n", curr_pid); // Confirm continue
            }
        }
    }
    closedir(dir); // Close directory
}