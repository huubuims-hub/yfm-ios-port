/* Backups of the saves (src/pc/saves/save_backup.h): made, listed, checked,
 * restored at the next launch, and refused when damaged or unsafe. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation" /* paths in a temp folder: far under the buffers */
#endif
#include "pc/saves/save_backup.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[100];

static void put(const char *relative, const void *data, size_t size)
{
    char path[1024], dir[1024];
    char *slash;
    FILE *file;
    snprintf(path, sizeof(path), "%s/%s", root, relative);
    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    *slash = '\0';
    {
        char command[1024];
        snprintf(command, sizeof(command), "mkdir -p '%s'", dir);
        assert(system(command) == 0);
    }
    file = fopen(path, "wb");
    assert(file);
    assert(fwrite(data, 1, size, file) == size);
    fclose(file);
}

static size_t get(const char *relative, unsigned char *out, size_t size)
{
    char path[1024];
    FILE *file;
    size_t n;
    snprintf(path, sizeof(path), "%s/%s", root, relative);
    file = fopen(path, "rb");
    if (!file) return (size_t)-1;
    n = fread(out, 1, size, file);
    fclose(file);
    return n;
}

static void flip_byte(const char *name, long at)
{
    char path[1024];
    FILE *file;
    int c;
    snprintf(path, sizeof(path), "%s/backups/%s", root, name);
    file = fopen(path, "r+b");
    assert(file);
    fseek(file, at, SEEK_SET);
    c = fgetc(file);
    fseek(file, at, SEEK_SET);
    fputc(c ^ 0x55, file);
    fclose(file);
}

int main(void)
{
    unsigned char slot[0x2000], other[0x2000], back[0x2000];
    char name[SAVE_BACKUP_NAME], why[300], report[500], text[500];
    SaveBackupInfo list[8];
    int files = 0, i;
    snprintf(root, sizeof(root), "/tmp/yfm_backup_test_%d", (int)getpid());
    {
        char command[1024];
        snprintf(command, sizeof(command), "rm -rf '%s' && mkdir -p '%s'", root, root);
        assert(system(command) == 0);
    }
    for (i = 0; i < 0x2000; i++) {
        slot[i] = (unsigned char)(i * 7 + 1);
        other[i] = (unsigned char)(i * 3);
    }

    /* Nothing to back up yet. */
    assert(SaveBackup_Create(root, NULL, name, sizeof(name), &files, why, sizeof(why)) == -1);
    assert(strstr(why, "nothing to back up"));
    assert(SaveBackup_List(root, list, 8) == 0);

    put("saves/slot01.sav", slot, sizeof(slot));
    put("saves/cards/deadbeef.bin", other, 100);
    put("saves/empty.dat", "", 0);
    put("settings.txt", "internal_scale=2\n", 17);
    put("controls.txt", "a=b\n", 4);
    put("game/rpg-yfm.bin", "DISC", 4); /* not part of a backup */
    put("saves/slot02.sav.partial", "junk", 4);

    assert(SaveBackup_Create(root, NULL, name, sizeof(name), &files, why, sizeof(why)) == 0);
    assert(files == 5); /* slot01, cards/deadbeef.bin, empty.dat, settings, controls */
    assert(strncmp(name, "yfm-backup-", 11) == 0);
    assert(SaveBackup_List(root, list, 8) == 1 && !strcmp(list[0].name, name) && list[0].size > 8192);
    assert(list[0].shown[4] == '-' && list[0].shown[10] == ' ');
    assert(SaveBackup_Verify(root, name, &files, why, sizeof(why)) == 0 && files == 5);

    /* A second backup in the same second gets its own name. */
    {
        char second[SAVE_BACKUP_NAME];
        assert(SaveBackup_Create(root, NULL, second, sizeof(second), &files, why, sizeof(why)) == 0);
        assert(strcmp(second, name) != 0);
        assert(SaveBackup_List(root, list, 8) == 2);
    }

    /* Change everything, stage the first, apply at the "next launch". */
    put("saves/slot01.sav", other, sizeof(other));
    put("saves/slot03.sav", other, 50); /* a file the backup lacks stays */
    put("settings.txt", "internal_scale=4\n", 17);
    assert(SaveBackup_Stage(root, name, why, sizeof(why)) == 0);
    assert(access("/tmp", F_OK) == 0);
    {
        char pending[1024];
        snprintf(pending, sizeof(pending), "%s/restore-pending.txt", root);
        assert(access(pending, F_OK) == 0);
    }
    assert(SaveBackup_ApplyPending(root, report, sizeof(report)) == 1);
    assert(strstr(report, "Restored 5 files") && strstr(report, "before-restore"));
    assert(get("saves/slot01.sav", back, sizeof(back)) == sizeof(slot) && !memcmp(back, slot, sizeof(slot)));
    assert(get("saves/cards/deadbeef.bin", back, sizeof(back)) == 100);
    assert(get("saves/slot03.sav", back, sizeof(back)) == 50);
    assert(get("settings.txt", back, sizeof(back)) == 17 && !memcmp(back, "internal_scale=2\n", 17));
    assert(SaveBackup_ApplyPending(root, report, sizeof(report)) == 0); /* only once */
    assert(SaveBackup_TakeResult(text, sizeof(text)) == 1 && SaveBackup_TakeResult(text, sizeof(text)) == 0);
    {
        int n = SaveBackup_List(root, list, 8), found = 0;
        for (i = 0; i < n; i++) found += strstr(list[i].name, "-before-restore") != NULL;
        assert(found == 1); /* the saves as they were are kept */
    }

    /* Damaged: a flipped byte in the data, in the name, a cut file. */
    assert(SaveBackup_Create(root, NULL, name, sizeof(name), &files, why, sizeof(why)) == 0);
    flip_byte(name, 200);
    assert(SaveBackup_Verify(root, name, NULL, why, sizeof(why)) == -1);
    assert(SaveBackup_Stage(root, name, why, sizeof(why)) == -1);
    {
        char pending[1024], path[400];
        snprintf(pending, sizeof(pending), "%s/restore-pending.txt", root);
        assert(access(pending, F_OK) != 0); /* nothing staged */
        snprintf(path, sizeof(path), "%s/backups/cut.yfmbak", root);
        {
            char command[1024];
            snprintf(command, sizeof(command), "head -c 300 '%s/backups/%s' > '%s'", root, name, path);
            assert(system(command) == 0);
        }
        assert(SaveBackup_Verify(root, "cut.yfmbak", NULL, why, sizeof(why)) == -1);
        /* not a backup at all */
        {
            FILE *f = fopen(path, "wb");
            fputs("hello", f);
            fclose(f);
        }
        assert(SaveBackup_Verify(root, "cut.yfmbak", NULL, why, sizeof(why)) == -1);
    }

    /* Names that are not plain paths are refused, whatever the checksum says. */
    {
        static const char *const bad[] = {"../evil.txt", "saves/../../evil.txt", "/etc/passwd", "game/rpg-yfm.bin",
                                          "saves//x", "saves/a\\b"};
        size_t k;
        for (k = 0; k < sizeof(bad) / sizeof(bad[0]); k++) {
            char path[1024];
            FILE *f;
            unsigned char n4[4] = {0, 0, 0, 0}, n8[8] = {0}, crc[4] = {0, 0, 0, 0};
            uint32_t len = (uint32_t)strlen(bad[k]);
            snprintf(path, sizeof(path), "%s/backups/evil%u.yfmbak", root, (unsigned)k);
            f = fopen(path, "wb");
            fwrite("YFMBAK01", 1, 8, f);
            n4[0] = (unsigned char)len;
            fwrite(n4, 1, 4, f);
            fwrite(bad[k], 1, len, f);
            fwrite(n8, 1, 8, f); /* size 0: crc of nothing is 0 */
            fwrite(crc, 1, 4, f);
            memset(n4, 0, 4);
            fwrite(n4, 1, 4, f);
            fclose(f);
            snprintf(path, sizeof(path), "evil%u.yfmbak", (unsigned)k);
            assert(SaveBackup_Verify(root, path, NULL, why, sizeof(why)) == -1);
        }
    }
    /* A file name that climbs out of the backups folder is not a backup name. */
    assert(SaveBackup_Verify(root, "../settings.txt", NULL, why, sizeof(why)) == -1);
    assert(SaveBackup_Verify(root, "a/b.yfmbak", NULL, why, sizeof(why)) == -1);
    assert(SaveBackup_Stage(root, "nope.yfmbak", why, sizeof(why)) == -1);

    /* A staged file that went bad since: nothing is changed. */
    {
        char pending[1024];
        FILE *f;
        snprintf(pending, sizeof(pending), "%s/restore-pending.txt", root);
        f = fopen(pending, "wb");
        fputs("cut.yfmbak\n", f);
        fclose(f);
        assert(SaveBackup_ApplyPending(root, report, sizeof(report)) == -1);
        assert(strstr(report, "Nothing was changed"));
        assert(get("saves/slot01.sav", back, sizeof(back)) == sizeof(slot) && !memcmp(back, slot, sizeof(slot)));
    }

    /* A file from outside, as the system's picker hands it over. */
    {
        char outside[1024], bad[1024], command[1024];
        SaveBackupInfo info;
        int before = SaveBackup_List(root, list, 8);
        (void)before;
        assert(SaveBackup_Create(root, NULL, name, sizeof(name), &files, why, sizeof(why)) == 0);
        snprintf(outside, sizeof(outside), "%s/My Backup (1).yfmbak", root);
        snprintf(command, sizeof(command), "cp '%s/backups/%s' '%s'", root, name, outside);
        assert(system(command) == 0);
        assert(SaveBackup_Import(root, outside, &info, why, sizeof(why)) == 0);
        assert(!strcmp(info.name, "My Backup (1).yfmbak") && info.size > 0);
        assert(SaveBackup_Verify(root, info.name, &files, why, sizeof(why)) == 0);
        /* the same name again does not replace it */
        assert(SaveBackup_Import(root, outside, &info, why, sizeof(why)) == 0 && !strcmp(info.name, "My Backup (1)-2.yfmbak"));
        /* a name that is not a backup's becomes an imported one */
        snprintf(bad, sizeof(bad), "%s/notes.txt", root);
        snprintf(command, sizeof(command), "cp '%s' '%s'", outside, bad);
        assert(system(command) == 0);
        assert(SaveBackup_Import(root, bad, &info, why, sizeof(why)) == 0 && !strcmp(info.name, "yfm-backup-imported.yfmbak"));
        /* something that is not a backup adds nothing */
        {
            FILE *f = fopen(bad, "wb");
            fputs("not a backup", f);
            fclose(f);
        }
        {
            int count = SaveBackup_List(root, list, 8);
            assert(SaveBackup_Import(root, bad, &info, why, sizeof(why)) == -1);
            assert(SaveBackup_List(root, list, 8) == count);
        }
        assert(SaveBackup_Import(root, "/nonexistent/file.yfmbak", &info, why, sizeof(why)) == -1);
    }


    /* Only the newest 20 the game made are kept. */
    for (i = 0; i < 25; i++) {
        char label[16];
        snprintf(label, sizeof(label), "n%02d", i);
        assert(SaveBackup_Create(root, label, name, sizeof(name), &files, why, sizeof(why)) == 0);
    }
    {
        SaveBackupInfo all[100];
        int n = SaveBackup_List(root, all, 100), own = 0;
        for (i = 0; i < n; i++) own += !strncmp(all[i].name, "yfm-backup-", 11);
        assert(own == 20);
    }
    {
        char command[1024];
        snprintf(command, sizeof(command), "rm -rf '%s'", root);
        assert(system(command) == 0);
    }
    puts("save backup: create, list, verify, restore at launch, damaged and unsafe files, import, pruning passed");
    return 0;
}
