#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ---------------- LOAD ----------------
int index_load(Index *index) {
    if (!index) return -1;

    index->count = 0;

    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0;

    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];

        char hash_hex[65];

        int ret = fscanf(f, "%o %64s %ld %zu %s\n",
                         &e->mode,
                         hash_hex,
                         &e->mtime_sec,
                         &e->size,
                         e->path);

        if (ret != 5) break;

        hex_to_hash(hash_hex, &e->hash);
        index->count++;
    }

    fclose(f);
    return 0;
}
int index_status(const Index *index) {
    printf("Staged changes:\n");

    if (index->count == 0) {
        printf("  (nothing to show)\n\n");
    } else {
        for (int i = 0; i < index->count; i++) {
            printf("  staged:     %s\n", index->entries[i].path);
        }
        printf("\n");
    }

    printf("Unstaged changes:\n");
    printf("  (nothing to show)\n\n");

    printf("Untracked files:\n");
    printf("  (nothing to show)\n\n");

    return 0;
}

// ---------------- SAVE ----------------
int index_save(const Index *index) {
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;

    for (int i = 0; i < index->count; i++) {
        char hex[65];
        hash_to_hex(&index->entries[i].hash, hex);

        fprintf(f, "%o %s %ld %zu %s\n",
                index->entries[i].mode,
                hex,
                index->entries[i].mtime_sec,
                index->entries[i].size,
                index->entries[i].path);
    }

    fclose(f);
    rename(".pes/index.tmp", INDEX_FILE);
    return 0;
}

// ---------------- ADD ----------------
int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    // Read file safely
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    void *data = malloc(size ? size : 1);   
    fread(data, 1, size, f);
    fclose(f);

    ObjectID id;
    object_write(OBJ_BLOB, data, size, &id);
    free(data);

    // Check if exists
    int found = -1;
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            found = i;
            break;
        }
    }

    IndexEntry *e;

    if (found >= 0) {
        e = &index->entries[found];
    } else {
        if (index->count >= MAX_INDEX_ENTRIES) return -1;
        e = &index->entries[index->count++];
        strcpy(e->path, path);
    }

    e->mode = get_file_mode(path);
    e->hash = id;
    e->mtime_sec = st.st_mtime;
    e->size = size;

    return index_save(index);
}
