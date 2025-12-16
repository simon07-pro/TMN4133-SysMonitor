/*
 * SysMonitor++ - System Monitoring Tool
 * A command-line system monitoring tool using Linux system calls and /proc filesystem
 * 
 * Contributors:
 * - Contributor 1: CPU Usage Module [IMPLEMENTED]
 * - Contributor 2: Memory Usage Module [TODO]
 * - Contributor 3: Top Processes Module [IMPLEMENTED]
 * - Contributor 4: Main Control & Continuous Monitoring [TODO]
 */

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

// ==================== SHARED COMPONENTS ====================

// Global variables
FILE *logFile = NULL;
volatile sig_atomic_t running = 1;

// Function prototypes
void getCPUUsage();
void getMemoryUsage();
void listTopProcesses();
void continuousMonitor(int interval);
void displayMenu();
void handleSignal(int sig);
void writeLog(const char *message);
char* getCurrentTimestamp();
void displayHelp();

// ==================== SHARED HELPER FUNCTIONS ====================

/**
 * getCurrentTimestamp - Get current timestamp as formatted string
 * Returns: Pointer to static buffer containing timestamp
 */
char* getCurrentTimestamp() {
    static char timestamp[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    return timestamp;
}

/**
 * writeLog - Write a timestamped message to log file
 * @message: Message to log
 */
void writeLog(const char *message) {
    if (logFile == NULL) {
        return;
    }
    
    fprintf(logFile, "[%s] %s\n", getCurrentTimestamp(), message);
    fflush(logFile);
}

/**
 * handleSignal - Signal handler for graceful shutdown
 * @sig: Signal number
 */
void handleSignal(int sig) {
    if (sig == SIGINT) {
        running = 0;
        printf("\n\nExiting... Saving log.\n");
        writeLog("SIGINT received");
        writeLog("Session ended");
        
        if (logFile != NULL) {
            fclose(logFile);
        }
        
        exit(0);
    }
}

/**
 * displayHelp - Display usage information
 */
void displayHelp() {
    printf("\nSysMonitor++ - System Monitoring Tool\n");
    printf("=====================================\n\n");
    printf("Usage:\n");
    printf("  ./sysmonitor              Interactive menu mode\n");
    printf("  ./sysmonitor -m cpu       Display CPU usage only\n");
    printf("  ./sysmonitor -m mem       Display memory usage only\n");
    printf("  ./sysmonitor -m proc      List top 5 active processes\n");
    printf("  ./sysmonitor -c <seconds> Continuous monitoring mode\n");
    printf("  ./sysmonitor -h           Display this help message\n\n");
    printf("Examples:\n");
    printf("  ./sysmonitor -c 2         Monitor every 2 seconds\n");
    printf("  ./sysmonitor -m cpu       Show CPU usage once\n\n");
}

// ==================== CPU USAGE MODULE (CONTRIBUTOR 1) ====================

// Static variables to store previous CPU values for delta calculation
static unsigned long long prev_user = 0, prev_nice = 0, prev_system = 0;
static unsigned long long prev_idle = 0, prev_iowait = 0, prev_irq = 0, prev_softirq = 0;
static int first_run = 1;

/**
 * parseCPUStats - Parse CPU statistics from /proc/stat buffer
 */
int parseCPUStats(const char *buffer, unsigned long long *user, unsigned long long *nice,
                  unsigned long long *system, unsigned long long *idle, unsigned long long *iowait,
                  unsigned long long *irq, unsigned long long *softirq) {
    
    const char *line = strstr(buffer, "cpu ");
    if (!line) {
        fprintf(stderr, "Error: Could not find 'cpu' line in /proc/stat\n");
        return -1;
    }
    
    int parsed = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu",
                        user, nice, system, idle, iowait, irq, softirq);
    
    if (parsed < 7) {
        fprintf(stderr, "Error: Failed to parse CPU statistics (parsed %d fields)\n", parsed);
        return -1;
    }
    
    return 0;
}

/**
 * calculateCPUUsage - Calculate CPU usage percentage from current and previous values
 */
double calculateCPUUsage(unsigned long long user, unsigned long long nice,
                        unsigned long long system, unsigned long long idle,
                        unsigned long long iowait, unsigned long long irq,
                        unsigned long long softirq) {
    
    if (first_run) {
        prev_user = user;
        prev_nice = nice;
        prev_system = system;
        prev_idle = idle;
        prev_iowait = iowait;
        prev_irq = irq;
        prev_softirq = softirq;
        first_run = 0;
        return -1.0;
    }
    
    unsigned long long user_delta = user - prev_user;
    unsigned long long nice_delta = nice - prev_nice;
    unsigned long long system_delta = system - prev_system;
    unsigned long long idle_delta = idle - prev_idle;
    unsigned long long iowait_delta = iowait - prev_iowait;
    unsigned long long irq_delta = irq - prev_irq;
    unsigned long long softirq_delta = softirq - prev_softirq;
    
    unsigned long long total_delta = user_delta + nice_delta + system_delta + 
                                     idle_delta + iowait_delta + irq_delta + softirq_delta;
    
    if (total_delta == 0) {
        return 0.0;
    }
    
    double cpu_usage = 100.0 * (1.0 - ((double)idle_delta / (double)total_delta));
    
    prev_user = user;
    prev_nice = nice;
    prev_system = system;
    prev_idle = idle;
    prev_iowait = iowait;
    prev_irq = irq;
    prev_softirq = softirq;
    
    return cpu_usage;
}

/**
 * getCPUUsage - Read CPU statistics and display usage percentage
 */
void getCPUUsage() {
    int fd;
    char buffer[4096];
    ssize_t bytes_read;
    
    fd = open("/proc/stat", O_RDONLY);
    if (fd == -1) {
        perror("Error: Failed to open /proc/stat");
        return;
    }
    
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read == -1) {
        perror("Error: Failed to read /proc/stat");
        close(fd);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    if (close(fd) == -1) {
        perror("Warning: Failed to close /proc/stat");
    }
    
    unsigned long long user, nice, system, idle, iowait, irq, softirq;
    if (parseCPUStats(buffer, &user, &nice, &system, &idle, &iowait, &irq, &softirq) != 0) {
        fprintf(stderr, "Error: Failed to parse CPU statistics\n");
        return;
    }
    
    double cpu_usage = calculateCPUUsage(user, nice, system, idle, iowait, irq, softirq);
    
    if (cpu_usage < 0) {
        printf("\n=== CPU Usage ===\n");
        printf("Initializing CPU monitoring...\n");
        printf("Run again to see CPU usage.\n\n");
        writeLog("CPU monitoring initialized");
        return;
    }
    
    printf("\n=== CPU Usage ===\n");
    printf("CPU Usage: %.1f%%\n\n", cpu_usage);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "CPU Usage: %.1f%%", cpu_usage);
    writeLog(log_msg);
}

// ==================== MEMORY USAGE MODULE (CONTRIBUTOR 2) ====================

/**
 * getMemoryUsage - Read memory statistics and display usage
 * TODO: Implement by Contributor 2
 */
void getMemoryUsage() {
    printf("\n=== Memory Usage ===\n");
    printf("[TODO] Memory module not yet implemented\n");
    printf("Contributor 2: Implement this function\n\n");
    writeLog("Memory module called (not implemented)");
}

// ==================== TOP PROCESSES MODULE (CONTRIBUTOR 3) ====================

//Structure to store process information
struct ProcessInfo {   
    int pid;
    char name[256];
    unsigned long utime;      // User mode CPU time
    unsigned long stime;      // Kernel mode CPU time
    unsigned long total_time; // Total CPU time
    double cpu_percent;
};

/**
 * isNumeric - Check if string contains only digits (helper for PID detection)
 */
int isNumeric(const char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    
    while (*str) {
        if (*str < '0' || *str > '9') {
            return 0;
        }
        str++;
    }
    return 1;
}

/**
 * readProcessName - Read process name from /proc/[PID]/comm
 */
int readProcessName(int pid, char *name, size_t name_size) {
    char path[256];
    int fd;
    ssize_t bytes_read;
    
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    
    bytes_read = read(fd, name, name_size - 1);
    close(fd);
    
    if (bytes_read <= 0) {
        return -1;
    }
    
    name[bytes_read] = '\0';
    
    // Remove newline if present
    if (bytes_read > 0 && name[bytes_read - 1] == '\n') {
        name[bytes_read - 1] = '\0';
    }
    
    return 0;
}

/**
 * readProcessStat - Read CPU time from /proc/[PID]/stat
 */
int readProcessStat(int pid, unsigned long *utime, unsigned long *stime) {
    char path[256];
    char buffer[4096];
    int fd;
    ssize_t bytes_read;
    
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    
    if (bytes_read <= 0) {
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    
    // Parse the stat file
    // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime...
    // We need fields 14 (utime) and 15 (stime)
    
    char *ptr = strchr(buffer, ')'); // Find end of process name
    if (ptr == NULL) {
        return -1;
    }
    
    ptr += 2; // Skip ") "
    
    // Skip fields 3-13 (11 fields)
    for (int i = 0; i < 11; i++) {
        ptr = strchr(ptr, ' ');
        if (ptr == NULL) {
            return -1;
        }
        ptr++; // Move past space
    }
    
    // Now we're at field 14 (utime)
    if (sscanf(ptr, "%lu %lu", utime, stime) != 2) {
        return -1;
    }
    
    return 0;
}

/**
 * compareProcesses - Comparison function for qsort (descending order by CPU time)
 */
int compareProcesses(const void *a, const void *b) {
    struct ProcessInfo *proc_a = (struct ProcessInfo *)a;
    struct ProcessInfo *proc_b = (struct ProcessInfo *)b;
    
    if (proc_b->total_time > proc_a->total_time) {
        return 1;
    } else if (proc_b->total_time < proc_a->total_time) {
        return -1;
    }
    return 0;
}

/**
 * listTopProcesses - Display top 5 CPU-consuming processes
 */
void listTopProcesses() {
    DIR *proc_dir;
    struct dirent *entry;
    struct ProcessInfo processes[1024];
    int process_count = 0;
    
    printf("\n=== Top 5 Active Processes ===\n");
    
    // Open /proc directory
    proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("Error: Failed to open /proc directory");
        writeLog("Error: Failed to open /proc directory");
        return;
    }
    
    // Read all process entries
    while ((entry = readdir(proc_dir)) != NULL && process_count < 1024) {
        // Check if entry name is numeric (PID)
        if (!isNumeric(entry->d_name)) {
            continue;
        }
        
        int pid = atoi(entry->d_name);
        unsigned long utime, stime;
        
        // Read process statistics
        if (readProcessStat(pid, &utime, &stime) != 0) {
            continue; // Process may have terminated, skip it
        }
        
        // Read process name
        char name[256];
        if (readProcessName(pid, name, sizeof(name)) != 0) {
            snprintf(name, sizeof(name), "[unknown]");
        }
        
        // Store process information
        processes[process_count].pid = pid;
        strncpy(processes[process_count].name, name, sizeof(processes[process_count].name) - 1);
        processes[process_count].name[sizeof(processes[process_count].name) - 1] = '\0';
        processes[process_count].utime = utime;
        processes[process_count].stime = stime;
        processes[process_count].total_time = utime + stime;
        
        process_count++;
    }
    
    closedir(proc_dir);
    
    if (process_count == 0) {
        printf("No processes found.\n\n");
        writeLog("No processes found");
        return;
    }
    
    // Sort processes by total CPU time (descending)
    qsort(processes, process_count, sizeof(struct ProcessInfo), compareProcesses);
    
    // Calculate CPU percentages (relative to top process)
    unsigned long max_time = processes[0].total_time;
    if (max_time > 0) {
        for (int i = 0; i < process_count; i++) {
            processes[i].cpu_percent = (100.0 * processes[i].total_time) / max_time;
        }
    }
    
    // Display header
    printf("%-10s %-30s %-15s %-10s\n", "PID", "Process Name", "CPU Time", "Relative %");
    printf("=======================================================================\n");
    
    // Display top 5 processes
    int display_count = (process_count < 5) ? process_count : 5;
    for (int i = 0; i < display_count; i++) {
        printf("%-10d %-30s %-15lu %.2f%%\n",
               processes[i].pid,
               processes[i].name,
               processes[i].total_time,
               processes[i].cpu_percent);
    }
    printf("\n");
    
    // Log the results
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Top 5 processes displayed: Top process PID=%d (%s) with %lu CPU time",
             processes[0].pid, processes[0].name, processes[0].total_time);
    writeLog(log_msg);
}

// ==================== MAIN CONTROL & CONTINUOUS MONITORING (CONTRIBUTOR 4) ====================

/**
 * displayMenu - Display main menu
 * TODO: Implement by Contributor 4
 */
void displayMenu() {
    printf("\n=== SysMonitor++ Main Menu ===\n");
    printf("1. CPU Usage\n");
    printf("2. Memory Usage\n");
    printf("3. Top 5 Processes\n");
    printf("4. Continuous Monitoring\n");
    printf("5. Exit\n");
    printf("Enter your choice: ");
}

/**
 * continuousMonitor - Continuous monitoring mode
 * @interval: Refresh interval in seconds
 * TODO: Implement by Contributor 4
 */
void continuousMonitor(int interval) {
    printf("\n=== Continuous Monitoring Mode ===\n");
    printf("[TODO] Continuous monitoring not yet implemented\n");
    printf("Contributor 4: Implement this function\n");
    printf("Requested interval: %d seconds\n\n", interval);
    writeLog("Continuous monitoring called (not implemented)");
}

/**
 * main - Program entry point
 * TODO: Full implementation by Contributor 4
 * Current version: Basic testing framework for CPU module
 */
int main(int argc, char *argv[]) {
    // Set up signal handler
    signal(SIGINT, handleSignal);
    
    // Open log file
    logFile = fopen("syslog.txt", "a");
    if (logFile == NULL) {
        perror("Warning: Could not open syslog.txt");
    } else {
        writeLog("Session started");
    }
    
    // Simple argument parsing for testing CPU module
    if (argc == 1) {
        // No arguments - test CPU module
        printf("SysMonitor++ - Testing CPU Module\n");
        printf("==================================\n");
        getCPUUsage();  // Initialize
        sleep(1);
        getCPUUsage();  // Get actual reading
        
    } else if (argc == 3 && strcmp(argv[1], "-m") == 0) {
        if (strcmp(argv[2], "cpu") == 0) {
            getCPUUsage();
            sleep(1);
            getCPUUsage();
        } else if (strcmp(argv[2], "mem") == 0) {
            getMemoryUsage();
        } else if (strcmp(argv[2], "proc") == 0) {
            listTopProcesses();
        } else {
            fprintf(stderr, "Error: Invalid parameter '%s'. Use -m [cpu/mem/proc]\n", argv[2]);
            displayHelp();
        }
    } else if (strcmp(argv[1], "-h") == 0) {
        displayHelp();
    } else {
        fprintf(stderr, "Invalid option. Use -h for help.\n");
        displayHelp();
    }
    
    // Clean up
    if (logFile != NULL) {
        writeLog("Session ended");
        fclose(logFile);
    }
    
    return 0;
}

/*
 * COMPILATION AND TESTING:
 * 
 * Compile:
 *   gcc sysmonitor.c -o sysmonitor
 * 
 * Test CPU module:
 *   ./sysmonitor           # Default test mode
 *   ./sysmonitor -m cpu    # CPU mode
 *   ./sysmonitor -h        # Help
 * 
 * Check logs:
 *   cat syslog.txt
 * 
 * Contributors 2, 3, 4: Replace your respective stub functions with full implementations
 */