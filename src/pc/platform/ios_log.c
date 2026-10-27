/* The iOS log file (ios_log.h). */
#include "ios_log.h"

typedef int memories_ios_log_translation_unit_is_not_empty;

#ifdef MEMORIES_IOS
#include "paths.h"
#include <errno.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <os/proc.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

#define LOG_LIMIT (4u << 20)   /* the file is cut when it grows past this */
#define BEAT_SECONDS 10
#define SETTINGS_DUMP 4096

static int log_fd = -1;
static struct timespec started;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static char header_text[1024];

static double uptime(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)(now.tv_sec - started.tv_sec) + (double)(now.tv_nsec - started.tv_nsec) / 1e9;
}

static void put(const char *text)
{
    size_t length = strlen(text);
    if (log_fd >= 0 && length) (void)!write(log_fd, text, length);
}

/* The system's own words for the hardware: "iPhone8,1" is an iPhone 6s. */
static void sysctl_string(const char *name, char *out, size_t size)
{
    size_t length = size;
    if (sysctlbyname(name, out, &length, NULL, 0)) snprintf(out, size, "?");
}

static const char *thermal_state(void)
{
    static const char *const names[] = {"nominal", "fair", "serious", "critical"};
    Class process_info = objc_getClass("NSProcessInfo");
    id info;
    long state;
    if (!process_info) return "?";
    info = ((id(*)(Class, SEL))objc_msgSend)(process_info, sel_registerName("processInfo"));
    if (!info) return "?";
    state = ((long(*)(id, SEL))objc_msgSend)(info, sel_registerName("thermalState"));
    return state >= 0 && state < 4 ? names[state] : "?";
}

static const char *system_version(char *out, size_t size)
{
    Class process_info = objc_getClass("NSProcessInfo");
    id info, text;
    const char *chars;
    snprintf(out, size, "?");
    if (!process_info) return out;
    info = ((id(*)(Class, SEL))objc_msgSend)(process_info, sel_registerName("processInfo"));
    if (!info) return out;
    text = ((id(*)(id, SEL))objc_msgSend)(info, sel_registerName("operatingSystemVersionString"));
    if (!text) return out;
    chars = ((const char *(*)(id, SEL))objc_msgSend)(text, sel_registerName("UTF8String"));
    if (chars) snprintf(out, size, "%s", chars);
    return out;
}

/* Memory the app holds, as the system counts it for killing it. */
static unsigned footprint_mb(void)
{
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) return 0;
    return (unsigned)(info.phys_footprint >> 20);
}

void IosLog_Note(const char *format, ...)
{
    char line[512];
    int n;
    va_list args;
    if (log_fd < 0) return;
    n = snprintf(line, sizeof(line), "[ios %8.2f s] ", uptime());
    va_start(args, format);
    vsnprintf(line + n, sizeof(line) - (size_t)n - 2, format, args);
    va_end(args);
    n = (int)strlen(line);
    line[n] = '\n';
    line[n + 1] = '\0';
    pthread_mutex_lock(&lock);
    put(line);
    pthread_mutex_unlock(&lock);
}

static void write_header(void)
{
    put(header_text);
}

/* The file past its limit starts again with the header and a note. */
static void cut_if_long(void)
{
    struct stat info;
    if (log_fd < 0 || fstat(log_fd, &info) || (unsigned long long)info.st_size < LOG_LIMIT) return;
    if (ftruncate(log_fd, 0)) return;
    write_header();
    IosLog_Note("the log passed %u MB and was cut here; the start of the run is gone", LOG_LIMIT >> 20);
}

static void *beat_thread(void *unused)
{
    (void)unused;
    for (;;) {
        char line[256];
        sleep(BEAT_SECONDS);
        cut_if_long();
        snprintf(line, sizeof(line), "memory %u MB held, %u MB left before the system ends the app, thermal %s",
                 footprint_mb(), (unsigned)(os_proc_available_memory() >> 20), thermal_state());
        IosLog_Note("%s", line);
    }
    return NULL;
}

static void at_exit(void)
{
    IosLog_Note("exit, after %.1f s", uptime());
}

/* The file's first few kilobytes, as the log's record of the settings. */
static void dump_file(const char *relative, const char *title)
{
    char path[1024], text[SETTINGS_DUMP + 1];
    FILE *file;
    size_t n;
    if (Paths_User(path, sizeof(path), relative)) return;
    file = fopen(path, "r");
    if (!file) {
        IosLog_Note("%s: none", title);
        return;
    }
    n = fread(text, 1, SETTINGS_DUMP, file);
    fclose(file);
    text[n] = '\0';
    IosLog_Note("%s:", title);
    pthread_mutex_lock(&lock);
    put(text);
    if (n && text[n - 1] != '\n') put("\n");
    pthread_mutex_unlock(&lock);
}

/* Documents/trace.txt: channel names, as MEMORIES_TRACE takes them. */
static void read_trace_channels(void)
{
    char path[1024], text[256];
    FILE *file;
    size_t n, i;
    if (getenv("MEMORIES_TRACE") || Paths_User(path, sizeof(path), "trace.txt")) return;
    file = fopen(path, "r");
    if (!file) return;
    n = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    for (i = 0; i < n && text[i] != '\n' && text[i] != '\r'; i++) {}
    text[i] = '\0';
    if (text[0]) {
        setenv("MEMORIES_TRACE", text, 0);
        IosLog_Note("trace channels from trace.txt: %s", text);
    }
}

void IosLog_Start(void)
{
    char dir[1024], path[1100], previous[1100], machine[64], build[64], version[128];
    struct stat info;
    pthread_t thread;
    sigset_t all, before;
    unsigned long long memory = 0;
    size_t length = sizeof(memory);
    time_t now = time(NULL);
    if (log_fd >= 0 || getenv("MEMORIES_NO_LOG_FILE")) return;
    clock_gettime(CLOCK_MONOTONIC, &started);
    if (Paths_User(dir, sizeof(dir), "logs") || Paths_MakeDirs(dir)) return;
    if (snprintf(path, sizeof(path), "%s/log.txt", dir) >= (int)sizeof(path) ||
        snprintf(previous, sizeof(previous), "%s/log-previous.txt", dir) >= (int)sizeof(previous))
        return;
    if (!stat(path, &info)) rename(path, previous); /* the last launch's log, kept */
    log_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
    if (log_fd < 0) return;
    /* Everything the game prints to stderr and stdout lands in the file. */
    dup2(log_fd, STDERR_FILENO);
    dup2(log_fd, STDOUT_FILENO);
    setvbuf(stdout, NULL, _IOLBF, 0);

    sysctl_string("hw.machine", machine, sizeof(machine));
    sysctl_string("kern.osversion", build, sizeof(build));
    if (sysctlbyname("hw.memsize", &memory, &length, NULL, 0)) memory = 0;
    snprintf(header_text, sizeof(header_text),
             "YFM Re-Decomp iOS log. Send this file (and log-previous.txt, and the reports folder) when something "
             "goes wrong.\nstarted %.24s (local time)\ndevice %s, %s, build %s\nRAM %llu MB, %d CPUs\nuser folder %s\n",
             ctime(&now), machine, system_version(version, sizeof(version)), build, memory >> 20,
             (int)sysconf(_SC_NPROCESSORS_ONLN), Paths_UserDir());
    write_header();
    IosLog_Note("memory %u MB held, %u MB left before the system ends the app, thermal %s", footprint_mb(),
                (unsigned)(os_proc_available_memory() >> 20), thermal_state());
    dump_file("settings.txt", "settings.txt");
    read_trace_channels();
    atexit(at_exit);
    /* The game's timer signal belongs to the main thread: the thread starts with all signals blocked. */
    sigfillset(&all);
    pthread_sigmask(SIG_BLOCK, &all, &before);
    if (!pthread_create(&thread, NULL, beat_thread, NULL)) pthread_detach(thread);
    pthread_sigmask(SIG_SETMASK, &before, NULL);
}
#else
void IosLog_Start(void) {}
void IosLog_Note(const char *format, ...) { (void)format; }
#endif
