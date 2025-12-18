# SysMonitor++ Project Architecture

## Project Overview
**SysMonitor++** is a command-line system monitoring tool for Linux that uses system calls and the `/proc` filesystem to display CPU usage, memory statistics, and active processes.

## Hardware & Software Requirements
- **OS**: Ubuntu Linux
- **Compiler**: GNU gcc
- **Libraries**: GNU glibc
- **Text Editor**: vi, nano, or equivalent
- **Version Control**: GitHub Desktop

## Compilation & Execution

### Compilation
```bash
gcc sysmonitor.c -o sysmonitor
```

### Execution Modes
```bash
./sysmonitor                  # Interactive menu mode
./sysmonitor -m cpu           # CPU usage only
./sysmonitor -m mem           # Memory usage only
./sysmonitor -m proc          # Top 5 processes
./sysmonitor -c 2             # Continuous monitoring (2-second refresh)
./sysmonitor -h               # Help message
```

---

## Code Architecture & Contributor Assignments

### **Shared Components** (All Contributors)

#### Core Header Files & Includes
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
```

#### Global Variables
```c
FILE *logFile = NULL;
volatile sig_atomic_t running = 1;
```

#### Shared Helper Functions
```c
void writeLog(const char *message);
void displayHelp();
void handleSignal(int sig);
char* getCurrentTimestamp();
```

**Implementation Notes:**
- `writeLog()`: Appends timestamped messages to `syslog.txt`
- `getCurrentTimestamp()`: Returns formatted timestamp string
- `handleSignal()`: Catches SIGINT (Ctrl+C) for graceful shutdown
- `displayHelp()`: Displays usage information

---

### **Contributor 1: CPU Usage Module**

**Responsibility**: Implement CPU usage monitoring functionality

#### Functions to Implement
```c
void getCPUUsage();
```

#### Implementation Details
1. **Read `/proc/stat`** using system calls:
   - Use `open()`, `read()`, `close()`
   - Parse the first line starting with "cpu"
   - Extract values: user, nice, system, idle, iowait, irq, softirq

2. **Calculate CPU Usage**:
   - Formula: `CPU% = 100 * (1 - idle_time / total_time)`
   - Store previous values for delta calculation
   - Handle first-run scenario (no previous data)

3. **Display Format**:
```
=== CPU Usage ===
CPU Usage: 45.2%
```

4. **Logging**:
   - Write to `syslog.txt`: `[TIMESTAMP] CPU Usage: 45.2%`

#### Files in `/proc` to Access
- `/proc/stat`

#### Error Handling
- Check if `/proc/stat` can be opened
- Use `perror()` for file access errors
- Validate data format from `/proc/stat`

---

### **Contributor 2: Memory Usage Module**

**Responsibility**: Implement memory monitoring functionality

#### Functions to Implement
```c
void getMemoryUsage();
```

#### Implementation Details
1. **Read `/proc/meminfo`** using system calls:
   - Use `open()`, `read()`, `close()`
   - Parse lines: `MemTotal`, `MemFree`

2. **Calculate Memory Statistics**:
   - Total Memory (KB)
   - Free Memory (KB)
   - Used Memory = Total - Free
   - Usage Percentage = (Used / Total) * 100

3. **Display Format**:
```
=== Memory Usage ===
Total Memory:  16384 MB
Used Memory:   8192 MB
Free Memory:   8192 MB
Usage:         50.0%
```

4. **Logging**:
   - Write to `syslog.txt`: `[TIMESTAMP] Memory - Total: 16384MB, Used: 8192MB, Free: 8192MB (50.0%)`

#### Files in `/proc` to Access
- `/proc/meminfo`

#### Error Handling
- Check if `/proc/meminfo` can be opened
- Use `perror()` for file access errors
- Handle missing fields in `/proc/meminfo`

---

### **Contributor 3: Top Processes Module**

**Responsibility**: Implement process listing functionality

#### Functions to Implement
```c
void listTopProcesses();
```
#### Helper Functions
```c
int isNumeric(const char *str);
int readProcessName(int pid, char *name, size_t name_size);
int readProcessStat(int pid, unsigned long *utime, unsigned long *stime);
int compareProcesses(const void *a, const void *b);
```
#### Data Structure
```c
struct ProcessInfo {
    int pid;
    char name[256];
    unsigned long utime;       // User mode CPU time
    unsigned long stime;       // Kernel mode CPU time
    unsigned long total_time;  // Total CPU time (utime + stime)
    double cpu_percent;        // Relative percentage
};
```

#### Implementation Details
1. **Traverse `/proc` Directory**:
   - Use opendir(), readdir(), closedir()
   - Identify numeric directories (PIDs) using helper function isNumeric()
   - Collect up to 1024 processes in struct ProcessInfo array

2. **Read Process Information**:
   - For each PID, read /proc/[PID]/stat using readProcessStat() helper
   - Read /proc/[PID]/comm for process name using readProcessName() helper
   - Extract CPU time: utime + stime (fields 14 and 15 in stat)
   - Store in struct ProcessInfo with pid, name, utime, stime, total_time

3. **Calculate CPU Usage per Process**:
   - Sort processes by total CPU time using qsort() with compareProcesses() helper
   - Calculate relative percentages (top process = 100%, others relative to it)
   - Formula: cpu_percent = (100.0 * process_total_time) / max_total_time
   - Select top 5 processes

4. **Display Format**:
```
=== Top 5 Active Processes ===
PID        Process Name                   CPU Time        Relative %
=======================================================================
1234       chrome                         1523456         100.00%
5678       firefox                        1245789         81.78%
9012       code                           987654          64.83%
3456       systemd                        856234          56.20%
7890       bash                           645123          42.35%
```

5. **Logging**:
   - Write to syslog.txt: [TIMESTAMP] Top 5 processes displayed: Top process PID=1234 (chrome) with 1523456 CPU time

#### Files in `/proc` to Access
- `/proc/[PID]/stat`
- `/proc/[PID]/comm`

#### Error Handling
- Skip PIDs that cannot be read (process may have ended)
- Handle permission errors gracefully
- Validate that PID directories are numeric
- Use "[unknown]" if process name cannot be read
- Display error message if /proc directory cannot be opened
---

### **Contributor 4: Continuous Monitoring & Main Control**

**Responsibility**: Implement continuous monitoring mode, menu system, and main program flow

#### Functions to Implement
```c
void continuousMonitor(int interval);
void displayMenu();
int parseArguments(int argc, char *argv[]);
int main(int argc, char *argv[]);
```

#### Implementation Details

##### 1. **Main Function**
- Parse command-line arguments using `parseArguments()`
- Set up signal handler for SIGINT
- Open `syslog.txt` for logging (append mode)
- Route to appropriate mode:
  - Menu mode (no arguments)
  - CPU-only mode (`-m cpu`)
  - Memory-only mode (`-m mem`)
  - Process-only mode (`-m proc`)
  - Continuous mode (`-c [interval]`)
  - Help mode (`-h`)

##### 2. **Menu System**
```
=== SysMonitor++ Main Menu ===
1. CPU Usage
2. Memory Usage
3. Top 5 Processes
4. Continuous Monitoring
5. Exit
Enter your choice:
```
- Read user input (1-5)
- Call corresponding function
- Loop until user selects Exit

##### 3. **Continuous Monitoring Mode**
- Accept interval parameter (seconds)
- Loop while `running == 1`:
  - Clear screen (`system("clear")` or ANSI codes)
  - Display timestamp
  - Call `getCPUUsage()`
  - Call `getMemoryUsage()`
  - Call `listTopProcesses()`
  - Sleep for specified interval
  - Write periodic log entries

##### 4. **Argument Parsing**
```c
./sysmonitor -m cpu    → Return CPU_MODE
./sysmonitor -m mem    → Return MEM_MODE
./sysmonitor -m proc   → Return PROC_MODE
./sysmonitor -c 2      → Return CONTINUOUS_MODE, set interval=2
./sysmonitor -h        → Return HELP_MODE
```

##### 5. **Signal Handling Integration**
- Register `handleSignal()` for SIGINT
- When SIGINT received:
  - Set `running = 0`
  - Display "Exiting... Saving log."
  - Write final log entry: `[TIMESTAMP] Session ended`
  - Close log file
  - Exit cleanly

#### Error Handling
- Missing argument: `./sysmonitor -m` → "Error: missing parameter. Use -m [cpu/mem/proc]"
- Invalid option: `./sysmonitor -x` → "Invalid option. Use -h for help."
- Invalid interval: `./sysmonitor -c abc` → "Error: interval must be a positive integer"
- Log file creation failure → Display error and exit

---

## Integration Guidelines

### File Structure
```
sysmonitor/
├── sysmonitor.c          # Main source file
├── README.md             # This file
├── syslog.txt            # Generated log file (gitignored)
└── .gitignore
```

### Integration Workflow

1. **Phase 1: Setup** (All Contributors)
   - Create shared header section
   - Implement shared helper functions
   - Agree on function signatures

2. **Phase 2: Individual Development** (Parallel)
   - Each contributor implements their module
   - Test functions independently
   - Document any issues

3. **Phase 3: Integration** (Contributor 4 leads)
   - Merge all modules into `sysmonitor.c`
   - Test all modes
   - Resolve conflicts

4. **Phase 4: Testing** (All Contributors)
   - Test error handling scenarios
   - Verify logging functionality
   - Test signal handling
   - Cross-verify calculations

### Git Branch Strategy
```
main
├── feature/cpu-usage       (Contributor 1)
├── feature/memory-usage    (Contributor 2)
├── feature/top-processes   (Contributor 3)
└── feature/main-control    (Contributor 4)
```

---

## Testing Checklist

### Functional Tests
- [ ] Menu displays correctly
- [ ] CPU usage calculates accurately
- [ ] Memory statistics match system values
- [ ] Top 5 processes display correctly
- [ ] Continuous mode refreshes at specified interval
- [ ] All modes write to `syslog.txt`
- [ ] Timestamps are accurate

### Error Handling Tests
- [ ] Missing argument: `./sysmonitor -m`
- [ ] Invalid option: `./sysmonitor -x`
- [ ] Invalid interval: `./sysmonitor -c -5`
- [ ] Non-numeric interval: `./sysmonitor -c abc`
- [ ] Permission errors (run as restricted user)
- [ ] Missing log file (auto-creation)

### Signal Handling Tests
- [ ] Ctrl+C in menu mode
- [ ] Ctrl+C in continuous mode
- [ ] Ctrl+C in single-run modes
- [ ] Final log entry written
- [ ] Clean exit (no zombies)

---

## Log File Format

### Example `syslog.txt`
```
[2025-01-15 14:23:10] Session started
[2025-01-15 14:23:15] CPU Usage: 45.2%
[2025-01-15 14:23:20] Memory - Total: 16384MB, Used: 8192MB, Free: 8192MB (50.0%)
[2025-01-15 14:23:25] Top Process: PID=1234, Name=chrome, CPU=15.3%
[2025-01-15 14:23:25] Top Process: PID=5678, Name=firefox, CPU=12.8%
[2025-01-15 14:23:30] Continuous monitoring started (interval: 2s)
[2025-01-15 14:25:45] SIGINT received
[2025-01-15 14:25:45] Session ended
```

---

## Communication & Coordination

### Recommended Tools
- **GitHub Issues**: Track bugs and feature progress
- **Pull Requests**: Code review before merging
- **GitHub Discussions**: Technical questions
- **Commit Messages**: Use conventional format
  ```
  feat(cpu): implement CPU usage calculation
  fix(memory): correct free memory calculation
  docs(readme): update integration guidelines
  ```

---

## Additional Resources

### Understanding `/proc` Filesystem
- `/proc/stat`: System-wide CPU statistics
- `/proc/meminfo`: Memory usage statistics
- `/proc/[PID]/stat`: Per-process statistics
- `/proc/[PID]/comm`: Process name

### Useful Linux Commands for Testing
```bash
top                    # Compare with your CPU/process data
free -m                # Compare with your memory data
cat /proc/stat         # View raw CPU data
cat /proc/meminfo      # View raw memory data
watch -n 2 ./sysmonitor -m cpu   # Test continuous updates
```

---

## Contact & Support
For questions or issues, create a GitHub Issue or contact the project maintainer.

**Happy Coding!** 🚀
