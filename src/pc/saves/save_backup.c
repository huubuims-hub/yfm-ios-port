/* Backups of the player's saves (save_backup.h). */
#include "save_backup.h"

/* POSIX only (lstat, mkdir with a mode, rename over a file): built everywhere
 * but Windows, and by the unit test with MEMORIES_SAVE_BACKUP. */
#if !defined(_WIN32) || defined(MEMORIES_SAVE_BACKUP)
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAGIC "YFMBAK01"
#define MAGIC_SIZE 8
#define NAME_MAX_LEN 190
#define MAX_FILES 2000
#define MAX_FILE ((size_t)16 << 20)
#define MAX_TOTAL ((size_t)64 << 20)
#define MAX_DEPTH 4
#define KEEP 20
#define PATH_SIZE 1200
#define NAME_PREFIX "yfm-backup-"
#define PENDING "restore-pending.txt"
#define REPORT "restore-report.txt"

static char last_result[480];
static void keep_report(const char *root, const char *text);
static int last_result_set;

static void fail(char *why, size_t size, const char *format, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void fail(char *why, size_t size, const char *format, ...)
{
    va_list args;
    if (!why || !size) return;
    va_start(args, format);
    vsnprintf(why, size, format, args);
    va_end(args);
}

/* --- checksum, little-endian numbers ----------------------------------- */

static uint32_t crc_table[256];

static uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t length)
{
    size_t i;
    if (!crc_table[1]) {
        uint32_t n, k;
        for (n = 0; n < 256; n++) {
            uint32_t c = n;
            for (k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_table[n] = c;
        }
    }
    crc = ~crc;
    for (i = 0; i < length; i++) crc = crc_table[(crc ^ data[i]) & 255] ^ (crc >> 8);
    return ~crc;
}

static void put32(unsigned char *at, uint32_t v)
{
    at[0] = (unsigned char)v;
    at[1] = (unsigned char)(v >> 8);
    at[2] = (unsigned char)(v >> 16);
    at[3] = (unsigned char)(v >> 24);
}
static uint32_t get32(const unsigned char *at)
{
    return (uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24;
}
static void put64(unsigned char *at, uint64_t v)
{
    put32(at, (uint32_t)v);
    put32(at + 4, (uint32_t)(v >> 32));
}
static uint64_t get64(const unsigned char *at) { return (uint64_t)get32(at) | (uint64_t)get32(at + 4) << 32; }

/* --- folders ------------------------------------------------------------ */

static int make_dirs(const char *path)
{
    char copy[PATH_SIZE];
    size_t i;
    if (snprintf(copy, sizeof(copy), "%s", path) >= (int)sizeof(copy)) return -1;
    for (i = 1; copy[i]; i++) {
        if (copy[i] != '/') continue;
        copy[i] = '\0';
        if (mkdir(copy, 0755) && errno != EEXIST) return -1;
        copy[i] = '/';
    }
    return mkdir(copy, 0755) && errno != EEXIST ? -1 : 0;
}

/* A name inside a backup: plain, relative, in saves/ or one of two files. */
static int safe_name(const char *name)
{
    const char *at;
    size_t length = strlen(name);
    if (!length || length > NAME_MAX_LEN) return 0;
    if (strcmp(name, "settings.txt") && strcmp(name, "controls.txt") && strncmp(name, "saves/", 6)) return 0;
    for (at = name; *at; at++) {
        unsigned char c = (unsigned char)*at;
        if (c < 32 || c == '\\' || c == 127) return 0;
    }
    if (name[0] == '/' || strstr(name, "//") || strstr(name, "/./") || strstr(name, "/../")) return 0;
    if (length >= 3 && !strcmp(name + length - 3, "/..")) return 0;
    if (length >= 2 && !strcmp(name + length - 2, "/.")) return 0;
    if (name[length - 1] == '/') return 0;
    return 1;
}

/* A backup file's own name, as it sits in the backups folder. */
/* A copy that is cut to fit, with no warning for it. */
static void copy_text(char *to, size_t size, const char *from)
{
    size_t length = strlen(from);
    if (!size) return;
    if (length >= size) length = size - 1;
    memcpy(to, from, length);
    to[length] = '\0';
}

static int safe_backup_name(const char *name)
{
    size_t length = strlen(name), ext = strlen(SAVE_BACKUP_EXT);
    const char *at;
    if (!length || length >= SAVE_BACKUP_NAME || length <= ext || strcmp(name + length - ext, SAVE_BACKUP_EXT)) return 0;
    for (at = name; *at; at++) {
        unsigned char c = (unsigned char)*at;
        if (c < 32 || c == '/' || c == '\\' || c == 127) return 0;
    }
    return name[0] != '.';
}

typedef struct {
    char (*names)[NAME_MAX_LEN + 1];
    int count;
} Names;

static int ends_with(const char *text, const char *tail)
{
    size_t a = strlen(text), b = strlen(tail);
    return a >= b && !strcmp(text + a - b, tail);
}

static void collect(const char *root, const char *relative, int depth, Names *list)
{
    char path[PATH_SIZE];
    DIR *folder;
    struct dirent *item;
    if (depth > MAX_DEPTH || snprintf(path, sizeof(path), "%s/%s", root, relative) >= (int)sizeof(path)) return;
    folder = opendir(path);
    if (!folder) return;
    while ((item = readdir(folder)) != NULL && list->count < MAX_FILES) {
        char child[NAME_MAX_LEN + 1], full[PATH_SIZE];
        struct stat info;
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        if (ends_with(item->d_name, ".partial") || ends_with(item->d_name, ".tmp") ||
            ends_with(item->d_name, ".restoring"))
            continue;
        if (snprintf(child, sizeof(child), "%s/%s", relative, item->d_name) >= (int)sizeof(child) ||
            snprintf(full, sizeof(full), "%s/%s", root, child) >= (int)sizeof(full) || lstat(full, &info))
            continue;
        if (S_ISDIR(info.st_mode)) collect(root, child, depth + 1, list);
        else if (S_ISREG(info.st_mode) && safe_name(child) && (size_t)info.st_size <= MAX_FILE)
            copy_text(list->names[list->count++], sizeof(list->names[0]), child);
    }
    closedir(folder);
}

static int compare_names(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static int add_if_file(const char *root, const char *name, Names *list)
{
    char full[PATH_SIZE];
    struct stat info;
    if (list->count >= MAX_FILES || snprintf(full, sizeof(full), "%s/%s", root, name) >= (int)sizeof(full)) return 0;
    if (lstat(full, &info) || !S_ISREG(info.st_mode) || (size_t)info.st_size > MAX_FILE) return 0;
    copy_text(list->names[list->count++], sizeof(list->names[0]), name);
    return 1;
}

/* --- reading a backup, checking or restoring ---------------------------- */

/* Walks the file. Checks every entry; with `root` set, also writes them. */
static int walk(const char *backup, const char *root, int *files_out, char *why, size_t why_size)
{
    FILE *file = fopen(backup, "rb");
    unsigned char head[MAGIC_SIZE], number[8];
    unsigned char *data = NULL;
    size_t total = 0;
    int files = 0, ended = 0, result = -1;
    if (!file) {
        fail(why, why_size, "The backup could not be opened: %s.", strerror(errno));
        return -1;
    }
    if (fread(head, 1, MAGIC_SIZE, file) != MAGIC_SIZE || memcmp(head, MAGIC, MAGIC_SIZE)) {
        fail(why, why_size, "This is not a backup of this game, or it is damaged.");
        goto done;
    }
    while (files < MAX_FILES) {
        char name[NAME_MAX_LEN + 1];
        uint32_t length, crc, want;
        uint64_t size;
        if (fread(number, 1, 4, file) != 4) {
            fail(why, why_size, "The backup is cut short.");
            goto done;
        }
        length = get32(number);
        if (length == 0) {
            ended = 1;
            break;
        }
        if (length > NAME_MAX_LEN || fread(name, 1, length, file) != length) {
            fail(why, why_size, "The backup is damaged.");
            goto done;
        }
        name[length] = '\0';
        if (!safe_name(name)) {
            fail(why, why_size, "The backup holds a file name that is not allowed, so it was not used.");
            goto done;
        }
        if (fread(number, 1, 8, file) != 8) {
            fail(why, why_size, "The backup is cut short.");
            goto done;
        }
        size = get64(number);
        if (size > MAX_FILE || total + (size_t)size > MAX_TOTAL) {
            fail(why, why_size, "The backup is larger than any this game makes, so it was not used.");
            goto done;
        }
        total += (size_t)size;
        free(data);
        data = malloc(size ? (size_t)size : 1);
        if (!data || (size && fread(data, 1, (size_t)size, file) != (size_t)size) || fread(number, 1, 4, file) != 4) {
            fail(why, why_size, "The backup is cut short.");
            goto done;
        }
        want = get32(number);
        crc = crc32_update(0, data, (size_t)size);
        if (crc != want) {
            fail(why, why_size, "The backup is damaged (the checksum of %s is wrong).", name);
            goto done;
        }
        if (root) {
            char full[PATH_SIZE], temp[PATH_SIZE + 16], parent[PATH_SIZE];
            char *slash;
            FILE *out;
            if (snprintf(full, sizeof(full), "%s/%s", root, name) >= (int)sizeof(full)) {
                fail(why, why_size, "A path in the backup is too long.");
                goto done;
            }
            snprintf(parent, sizeof(parent), "%s", full);
            slash = strrchr(parent, '/');
            if (slash) {
                *slash = '\0';
                if (make_dirs(parent)) {
                    fail(why, why_size, "Could not make the folder for %s: %s.", name, strerror(errno));
                    goto done;
                }
            }
            snprintf(temp, sizeof(temp), "%s.restoring", full);
            out = fopen(temp, "wb");
            if (!out || (size && fwrite(data, 1, (size_t)size, out) != (size_t)size) || fflush(out) || fclose(out) ||
                rename(temp, full)) {
                fail(why, why_size, "Could not write %s: %s.", name, strerror(errno));
                goto done;
            }
        }
        files++;
    }
    if (!ended) {
        fail(why, why_size, "The backup is cut short.");
        goto done;
    }
    if (files_out) *files_out = files;
    result = 0;
done:
    free(data);
    fclose(file);
    return result;
}

static int backup_path(const char *root, const char *name, char *path, size_t size)
{
    return snprintf(path, size, "%s/backups/%s", root, name) < (int)size ? 0 : -1;
}

int SaveBackup_Verify(const char *root, const char *name, int *files, char *why, size_t why_size)
{
    char path[PATH_SIZE];
    if (!safe_backup_name(name) || backup_path(root, name, path, sizeof(path))) {
        fail(why, why_size, "That is not a backup file name.");
        return -1;
    }
    return walk(path, NULL, files, why, why_size);
}

/* --- listing, creating -------------------------------------------------- */

static int compare_infos(const void *a, const void *b)
{
    return strcmp(((const SaveBackupInfo *)b)->name, ((const SaveBackupInfo *)a)->name); /* newest first */
}

static void show_time(SaveBackupInfo *info, time_t modified)
{
    int y, mo, d, h, mi, s;
    if (sscanf(info->name, NAME_PREFIX "%4d%2d%2d-%2d%2d%2d", &y, &mo, &d, &h, &mi, &s) == 6 && mo >= 1 && mo <= 12) {
        snprintf(info->shown, sizeof(info->shown), "%04d-%02d-%02d %02d:%02d", y, mo, d, h, mi);
    } else {
        struct tm *local = localtime(&modified);
        if (local) strftime(info->shown, sizeof(info->shown), "%Y-%m-%d %H:%M", local);
        else snprintf(info->shown, sizeof(info->shown), "?");
    }
}

int SaveBackup_List(const char *root, SaveBackupInfo *list, int max)
{
    char path[PATH_SIZE], full[PATH_SIZE];
    DIR *folder;
    struct dirent *item;
    int count = 0;
    if (snprintf(path, sizeof(path), "%s/backups", root) >= (int)sizeof(path) || !(folder = opendir(path))) return 0;
    while ((item = readdir(folder)) != NULL) {
        struct stat info;
        if (!safe_backup_name(item->d_name) || snprintf(full, sizeof(full), "%s/%s", path, item->d_name) >= (int)sizeof(full) ||
            stat(full, &info) || !S_ISREG(info.st_mode))
            continue;
        if (count >= max) {
            /* More than asked for: keep the newest by name. */
            qsort(list, (size_t)count, sizeof(list[0]), compare_infos);
            if (strcmp(item->d_name, list[count - 1].name) <= 0) continue;
            count--;
        }
        copy_text(list[count].name, sizeof(list[count].name), item->d_name);
        list[count].size = (long long)info.st_size;
        show_time(&list[count], info.st_mtime);
        count++;
    }
    closedir(folder);
    qsort(list, (size_t)count, sizeof(list[0]), compare_infos);
    return count;
}

int SaveBackup_Import(const char *root, const char *source, SaveBackupInfo *info, char *why, size_t why_size)
{
    char folder[PATH_SIZE], path[PATH_SIZE], temp[PATH_SIZE + 16], name[SAVE_BACKUP_NAME], stem[SAVE_BACKUP_NAME];
    const char *base;
    struct stat stats;
    FILE *in, *out;
    unsigned char buffer[8192];
    size_t got;
    int n, copied = 0;
    if (!source || !*source || stat(source, &stats) || !S_ISREG(stats.st_mode)) {
        fail(why, why_size, "The file could not be read.");
        return -1;
    }
    if ((unsigned long long)stats.st_size > (unsigned long long)MAX_TOTAL + (4u << 20)) {
        fail(why, why_size, "The file is larger than any backup this game makes.");
        return -1;
    }
    if (walk(source, NULL, NULL, why, why_size)) return -1; /* whole and safe, before a byte is copied */
    base = strrchr(source, '/');
    base = base ? base + 1 : source;
    if (safe_backup_name(base)) {
        copy_text(stem, sizeof(stem), base);
        stem[strlen(stem) - strlen(SAVE_BACKUP_EXT)] = '\0';
    } else {
        copy_text(stem, sizeof(stem), NAME_PREFIX "imported");
    }
    if (snprintf(folder, sizeof(folder), "%s/backups", root) >= (int)sizeof(folder) || make_dirs(folder)) {
        fail(why, why_size, "Could not make the backups folder: %s.", strerror(errno));
        return -1;
    }
    for (n = 1; n < 100; n++) {
        if (n == 1) snprintf(name, sizeof(name), "%.80s" SAVE_BACKUP_EXT, stem);
        else snprintf(name, sizeof(name), "%.80s-%d" SAVE_BACKUP_EXT, stem, n);
        if (!safe_backup_name(name) || backup_path(root, name, path, sizeof(path))) {
            fail(why, why_size, "The backup's name is not valid.");
            return -1;
        }
        if (access(path, F_OK)) break;
    }
    in = fopen(source, "rb");
    snprintf(temp, sizeof(temp), "%s.partial", path);
    out = in ? fopen(temp, "wb") : NULL;
    if (!in || !out) {
        if (in) fclose(in);
        fail(why, why_size, "Could not copy the backup: %s.", strerror(errno));
        return -1;
    }
    while ((got = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, got, out) != got) break;
        copied = 1;
    }
    fclose(in);
    if (!copied || fflush(out) || ferror(out) || fclose(out) || rename(temp, path)) {
        unlink(temp);
        fail(why, why_size, "Could not copy the backup: %s.", strerror(errno));
        return -1;
    }
    if (walk(path, NULL, NULL, why, why_size)) {
        unlink(path);
        return -1;
    }
    if (info) {
        struct stat made;
        memset(info, 0, sizeof(*info));
        copy_text(info->name, sizeof(info->name), name);
        if (!stat(path, &made)) {
            info->size = (long long)made.st_size;
            show_time(info, made.st_mtime);
        }
    }
    return 0;
}

/* The game's own backups past the newest KEEP are removed. */
static void prune(const char *root, const char *keep_name)
{
    SaveBackupInfo *list = malloc(sizeof(*list) * 200);
    int count, i, own = 0;
    if (!list) return;
    count = SaveBackup_List(root, list, 200);
    for (i = 0; i < count; i++) {
        char path[PATH_SIZE];
        if (strncmp(list[i].name, NAME_PREFIX, strlen(NAME_PREFIX))) continue;
        if (++own <= KEEP || !strcmp(list[i].name, keep_name)) continue;
        if (!backup_path(root, list[i].name, path, sizeof(path))) unlink(path);
    }
    free(list);
}

int SaveBackup_Create(const char *root, const char *label, char *name, size_t name_size, int *files_out, char *why,
                      size_t why_size)
{
    char folder[PATH_SIZE], path[PATH_SIZE], temp[PATH_SIZE + 16], stem[SAVE_BACKUP_NAME];
    Names list = {NULL, 0};
    FILE *out = NULL;
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    unsigned char number[8];
    int i, n, result = -1;
    list.names = malloc(sizeof(list.names[0]) * MAX_FILES);
    if (!list.names) {
        fail(why, why_size, "Out of memory.");
        return -1;
    }
    collect(root, "saves", 0, &list);
    qsort(list.names, (size_t)list.count, sizeof(list.names[0]), compare_names);
    add_if_file(root, "settings.txt", &list);
    add_if_file(root, "controls.txt", &list);
    if (!list.count) {
        fail(why, why_size, "There is nothing to back up yet: no save has been made.");
        goto done;
    }
    if (snprintf(folder, sizeof(folder), "%s/backups", root) >= (int)sizeof(folder) || make_dirs(folder)) {
        fail(why, why_size, "Could not make the backups folder: %s.", strerror(errno));
        goto done;
    }
    snprintf(stem, sizeof(stem), NAME_PREFIX "%04d%02d%02d-%02d%02d%02d%s%s", local ? local->tm_year + 1900 : 1970,
             local ? local->tm_mon + 1 : 1, local ? local->tm_mday : 1, local ? local->tm_hour : 0,
             local ? local->tm_min : 0, local ? local->tm_sec : 0, label && *label ? "-" : "", label && *label ? label : "");
    for (n = 1; n < 100; n++) {
        if (n == 1) snprintf(name, name_size, "%s" SAVE_BACKUP_EXT, stem);
        else snprintf(name, name_size, "%s-%d" SAVE_BACKUP_EXT, stem, n);
        if (!safe_backup_name(name)) {
            fail(why, why_size, "The backup's name is not valid.");
            goto done;
        }
        if (backup_path(root, name, path, sizeof(path))) goto done;
        if (access(path, F_OK)) break;
    }
    snprintf(temp, sizeof(temp), "%s.partial", path);
    out = fopen(temp, "wb");
    if (!out || fwrite(MAGIC, 1, MAGIC_SIZE, out) != MAGIC_SIZE) {
        fail(why, why_size, "Could not write the backup: %s.", strerror(errno));
        goto done;
    }
    for (i = 0; i < list.count; i++) {
        char full[PATH_SIZE];
        FILE *in;
        unsigned char *data;
        long long size;
        size_t got;
        uint32_t length = (uint32_t)strlen(list.names[i]);
        if (snprintf(full, sizeof(full), "%s/%s", root, list.names[i]) >= (int)sizeof(full)) continue;
        in = fopen(full, "rb");
        if (!in) continue;
        data = malloc(MAX_FILE + 1);
        if (!data) {
            fclose(in);
            fail(why, why_size, "Out of memory.");
            goto done;
        }
        got = fread(data, 1, MAX_FILE + 1, in);
        fclose(in);
        size = (long long)got;
        if (got > MAX_FILE) {
            free(data);
            continue; /* grew past the limit since it was listed */
        }
        put32(number, length);
        fwrite(number, 1, 4, out);
        fwrite(list.names[i], 1, length, out);
        put64(number, (uint64_t)size);
        fwrite(number, 1, 8, out);
        if (got) fwrite(data, 1, got, out);
        put32(number, crc32_update(0, data, got));
        fwrite(number, 1, 4, out);
        free(data);
    }
    put32(number, 0);
    fwrite(number, 1, 4, out);
    if (fflush(out) || ferror(out) || fclose(out)) {
        out = NULL;
        fail(why, why_size, "Could not write the backup: %s.", strerror(errno));
        unlink(temp);
        goto done;
    }
    out = NULL;
    if (rename(temp, path)) {
        fail(why, why_size, "Could not finish the backup: %s.", strerror(errno));
        unlink(temp);
        goto done;
    }
    if (walk(path, NULL, files_out, why, why_size)) { /* read back: never report a backup that is not whole */
        unlink(path);
        goto done;
    }
    prune(root, name);
    result = 0;
done:
    if (out) {
        fclose(out);
        unlink(temp);
    }
    free(list.names);
    return result;
}

/* --- restoring at the next launch --------------------------------------- */

int SaveBackup_Stage(const char *root, const char *name, char *why, size_t why_size)
{
    char path[PATH_SIZE];
    FILE *file;
    if (SaveBackup_Verify(root, name, NULL, why, why_size)) return -1;
    if (snprintf(path, sizeof(path), "%s/" PENDING, root) >= (int)sizeof(path) || !(file = fopen(path, "wb"))) {
        fail(why, why_size, "Could not record the restore: %s.", strerror(errno));
        return -1;
    }
    fprintf(file, "%s\n", name);
    if (fflush(file) || fclose(file)) {
        fail(why, why_size, "Could not record the restore: %s.", strerror(errno));
        return -1;
    }
    return 0;
}

int SaveBackup_ApplyPending(const char *root, char *report, size_t report_size)
{
    char path[PATH_SIZE], name[SAVE_BACKUP_NAME + 8], why[300], before[SAVE_BACKUP_NAME + 40], backup[PATH_SIZE];
    FILE *file;
    int files = 0, ignored;
    size_t length;
    if (snprintf(path, sizeof(path), "%s/" PENDING, root) >= (int)sizeof(path) || !(file = fopen(path, "rb"))) return 0;
    if (!fgets(name, sizeof(name), file)) name[0] = '\0';
    fclose(file);
    unlink(path); /* once: a restore that goes wrong is never tried again by itself */
    length = strlen(name);
    while (length && (name[length - 1] == '\n' || name[length - 1] == '\r')) name[--length] = '\0';
    why[0] = '\0';
    if (SaveBackup_Verify(root, name, NULL, why, sizeof(why))) {
        snprintf(last_result, sizeof(last_result), "The saves were not restored: %.250s Nothing was changed.", why);
        last_result_set = 1;
        keep_report(root, last_result);
        if (report) snprintf(report, report_size, "%s", last_result);
        return -1;
    }
    /* What is there now is kept, so a restore can be undone. */
    if (SaveBackup_Create(root, "before-restore", before, sizeof(before), &ignored, why, sizeof(why))) before[0] = '\0';
    if (backup_path(root, name, backup, sizeof(backup)) || walk(backup, root, &files, why, sizeof(why))) {
        snprintf(last_result, sizeof(last_result), "The restore stopped part way: %.200s Your saves before it are in backups/%.120s.",
                 why, before[0] ? before : "(none was made)");
        last_result_set = 1;
        keep_report(root, last_result);
        if (report) snprintf(report, report_size, "%s", last_result);
        return -1;
    }
    snprintf(last_result, sizeof(last_result), "Restored %d files from %.100s.%s%.120s", files, name,
             before[0] ? " The saves as they were are kept in backups/" : "", before);
    last_result_set = 1;
    keep_report(root, last_result);
    if (report) snprintf(report, report_size, "%s", last_result);
    return 1;
}

/* The report is also kept in a file, as the process that applies a restore (a
 * monitor, on a desktop) is not always the one that shows it. */
static void keep_report(const char *root, const char *text)
{
    char path[PATH_SIZE];
    FILE *file;
    if (snprintf(path, sizeof(path), "%s/" REPORT, root) >= (int)sizeof(path) || !(file = fopen(path, "wb"))) return;
    fputs(text, file);
    fclose(file);
}

int SaveBackup_TakeResult(const char *root, char *text, size_t size)
{
    char path[PATH_SIZE];
    FILE *file;
    size_t n;
    if (last_result_set) {
        last_result_set = 0;
        snprintf(text, size, "%s", last_result);
        if (snprintf(path, sizeof(path), "%s/" REPORT, root) < (int)sizeof(path)) unlink(path);
        return 1;
    }
    if (snprintf(path, sizeof(path), "%s/" REPORT, root) >= (int)sizeof(path) || !(file = fopen(path, "rb"))) return 0;
    n = fread(text, 1, size - 1, file);
    fclose(file);
    unlink(path);
    text[n] = '\0';
    return n > 0;
}

#else
typedef int memories_save_backup_is_not_built_here;
#endif
