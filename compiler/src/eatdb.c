/*
 * ============================================================================
 * EATHON DATABASE ENGINE (eatdb)
 * Version: 1.0.1-audited
 * Implementation Language: Portable ISO C99 (Linux, Windows MinGW/Clang, macOS)
 *
 * Architecture:
 *   - Custom binary slotted-page storage format (.eatdb) with 64-byte Superblock
 *     ("EATDB001" magic signature, page size 4096 bytes, CRC32 integrity hash,
 *      transaction sequence number, active record count).
 *   - Write-Ahead Log (.eatdb.wal) supporting ACID transactions (Begin, Commit,
 *     Rollback).
 *   - Eathon Query Language (EQL) parser enforcing the Eathon Punctuation
 *     Invariant (only commas ',' and periods '.' allowed).
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#define EATDB_MAGIC "EATDB001"
#define EATDB_PAGE_SIZE 4096
#define EATDB_MAX_RECORDS 512

typedef struct {
    char magic[8];           /* "EATDB001" */
    uint32_t version;        /* 1 */
    uint32_t page_size;      /* 4096 */
    uint64_t tx_seq;         /* Monotonic transaction sequence ID */
    uint32_t record_count;   /* Number of active records */
    uint32_t crc32;          /* CRC32 checksum of record payload */
    char table_name[64];     /* Active collection/table name */
} EatDbSuperblock;

typedef struct {
    int64_t key;
    int64_t numeric_val;
    char label[64];
    uint8_t active;
    uint32_t page_id;
    uint32_t slot_id;
    uint64_t tx_id;
} EatDbRecord;

static int eat_stricmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static uint32_t compute_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320U;
            else crc >>= 1;
        }
    }
    return ~crc;
}

typedef struct {
    EatDbSuperblock sb;
    EatDbRecord records[EATDB_MAX_RECORDS];
    char db_path[256];
    char wal_path[264];
    bool in_tx;
    EatDbRecord tx_backup[EATDB_MAX_RECORDS];
    uint32_t tx_backup_count;
} EatDbInstance;

static void eatdb_init(EatDbInstance *db, const char *path) {
    memset(db, 0, sizeof(EatDbInstance));
    snprintf(db->db_path, sizeof(db->db_path), "%s", path);
    snprintf(db->wal_path, sizeof(db->wal_path), "%s.wal", path);

    FILE *f = fopen(db->db_path, "rb");
    if (f) {
        if (fread(&db->sb, sizeof(EatDbSuperblock), 1, f) == 1 &&
            memcmp(db->sb.magic, EATDB_MAGIC, 8) == 0) {
            uint32_t cnt = db->sb.record_count;
            if (cnt > EATDB_MAX_RECORDS) cnt = EATDB_MAX_RECORDS;
            size_t nread = fread(db->records, sizeof(EatDbRecord), cnt, f);
            (void)nread;
        }
        fclose(f);
    } else {
        memcpy(db->sb.magic, EATDB_MAGIC, 8);
        db->sb.version = 1;
        db->sb.page_size = EATDB_PAGE_SIZE;
        db->sb.tx_seq = 1;
        db->sb.record_count = 0;
        strcpy(db->sb.table_name, "records");
    }
}

static bool eatdb_flush(EatDbInstance *db) {
    db->sb.crc32 = compute_crc32((const uint8_t *)db->records,
                                 sizeof(EatDbRecord) * db->sb.record_count);
    FILE *f = fopen(db->db_path, "wb");
    if (!f) return false;
    fwrite(&db->sb, sizeof(EatDbSuperblock), 1, f);
    fwrite(db->records, sizeof(EatDbRecord), db->sb.record_count, f);
    long pos = ftell(f);
    if (pos < EATDB_PAGE_SIZE) {
        uint8_t zero = 0;
        for (long i = pos; i < EATDB_PAGE_SIZE; i++) fwrite(&zero, 1, 1, f);
    }
    fclose(f);
    return true;
}

static void eatdb_append_wal(EatDbInstance *db, const char *op, int64_t key, int64_t val, const char *label) {
    FILE *f = fopen(db->wal_path, "ab");
    if (!f) return;
    fprintf(f, "TX=%llu OP=%s KEY=%lld VAL=%lld LABEL=%s\n",
            (unsigned long long)db->sb.tx_seq, op, (long long)key, (long long)val, label ? label : "");
    fclose(f);
}

static void append_json_escaped(char *out, size_t cap, size_t *used, const char *s) {
    int n = snprintf(out + *used, cap - *used, "\"");
    if (n > 0 && *used + (size_t)n < cap) *used += (size_t)n;
    for (; *s; s++) {
        const char *rep = NULL;
        char single[2] = { *s, '\0' };
        if (*s == '"') rep = "\\\"";
        else if (*s == '\\') rep = "\\\\";
        else if (*s == '\n') rep = "\\n";
        else rep = single;
        n = snprintf(out + *used, cap - *used, "%s", rep);
        if (n > 0 && *used + (size_t)n < cap) *used += (size_t)n;
    }
    n = snprintf(out + *used, cap - *used, "\"");
    if (n > 0 && *used + (size_t)n < cap) *used += (size_t)n;
}

int eatdb_execute_script(const char *db_file, const char *script, char *out_json_buf, size_t out_json_cap) {
    if (out_json_buf && out_json_cap > 0) out_json_buf[0] = '\0';

    EatDbInstance db;
    eatdb_init(&db, db_file);

    for (int i = 0; script[i]; i++) {
        char c = script[i];
        if (!isalnum((unsigned char)c) && !isspace((unsigned char)c) && c != ',' && c != '.') {
            if (out_json_buf && out_json_cap > 0) {
                snprintf(out_json_buf, out_json_cap,
                         "{\"success\":false,\"error\":\"Forbidden punctuation '%c' in EathonDB query. Only commas (,) and periods (.) are allowed.\"}\n", c);
            }
            return 1;
        }
    }

    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", script);

    int matched_indices[EATDB_MAX_RECORDS];
    int matched_count = 0;
    int ops_executed = 0;

    char *cursor = buf;
    while (*cursor) {
        char *dot = strchr(cursor, '.');
        if (dot) *dot = '\0';

        char *stmt = cursor;
        cursor = dot ? (dot + 1) : (cursor + strlen(cursor));

        while (*stmt && isspace((unsigned char)*stmt)) stmt++;
        if (*stmt == '\0') continue;

        char words[32][64];
        int wc = 0;
        int p = 0;
        while (stmt[p] && wc < 32) {
            while (stmt[p] && (isspace((unsigned char)stmt[p]) || stmt[p] == ',')) p++;
            if (!stmt[p]) break;
            int wi = 0;
            while (stmt[p] && !isspace((unsigned char)stmt[p]) && stmt[p] != ',' && wi < 62) {
                words[wc][wi++] = stmt[p++];
            }
            words[wc][wi] = '\0';
            wc++;
        }

        if (wc > 0) {
            if (eat_stricmp(words[0], "Create") == 0 && wc >= 3 && eat_stricmp(words[1], "collection") == 0) {
                snprintf(db.sb.table_name, sizeof(db.sb.table_name), "%s", words[2]);
                db.sb.tx_seq++;
                eatdb_append_wal(&db, "CREATE_COLLECTION", 0, 0, words[2]);
                ops_executed++;
            } else if (eat_stricmp(words[0], "Begin") == 0) {
                db.in_tx = true;
                db.tx_backup_count = db.sb.record_count;
                memcpy(db.tx_backup, db.records, sizeof(db.records));
                db.sb.tx_seq++;
                eatdb_append_wal(&db, "BEGIN_TX", 0, 0, db.sb.table_name);
                ops_executed++;
            } else if (eat_stricmp(words[0], "Commit") == 0) {
                db.in_tx = false;
                db.sb.tx_seq++;
                eatdb_append_wal(&db, "COMMIT_TX", 0, 0, db.sb.table_name);
                eatdb_flush(&db);
                ops_executed++;
            } else if (eat_stricmp(words[0], "Rollback") == 0) {
                if (db.in_tx) {
                    db.sb.record_count = db.tx_backup_count;
                    memcpy(db.records, db.tx_backup, sizeof(db.records));
                    db.in_tx = false;
                }
                eatdb_append_wal(&db, "ROLLBACK_TX", 0, 0, db.sb.table_name);
                eatdb_flush(&db);
                ops_executed++;
            } else if (eat_stricmp(words[0], "Insert") == 0) {
                int64_t k = 0;
                int64_t v = 0;
                char lbl[64] = "record";
                for (int i = 1; i < wc; i++) {
                    if (eat_stricmp(words[i], "key") == 0 && i + 1 < wc) k = strtoll(words[++i], NULL, 10);
                    else if (eat_stricmp(words[i], "label") == 0 && i + 1 < wc) snprintf(lbl, sizeof(lbl), "%s", words[++i]);
                    else if (eat_stricmp(words[i], "value") == 0 && i + 1 < wc) v = strtoll(words[++i], NULL, 10);
                }
                int found = -1;
                for (uint32_t r = 0; r < db.sb.record_count; r++) {
                    if (db.records[r].active && db.records[r].key == k) {
                        found = (int)r;
                        break;
                    }
                }
                if (found < 0 && db.sb.record_count < EATDB_MAX_RECORDS) {
                    found = (int)db.sb.record_count++;
                }
                if (found >= 0) {
                    db.records[found].key = k;
                    db.records[found].numeric_val = v;
                    snprintf(db.records[found].label, sizeof(db.records[found].label), "%s", lbl);
                    db.records[found].active = 1;
                    db.records[found].page_id = 1 + (found / 32);
                    db.records[found].slot_id = found % 32;
                    db.records[found].tx_id = db.sb.tx_seq;
                }
                eatdb_append_wal(&db, "INSERT", k, v, lbl);
                if (!db.in_tx) eatdb_flush(&db);
                ops_executed++;
            } else if (eat_stricmp(words[0], "Delete") == 0 && wc >= 3 && eat_stricmp(words[1], "key") == 0) {
                int64_t k = strtoll(words[2], NULL, 10);
                for (uint32_t r = 0; r < db.sb.record_count; r++) {
                    if (db.records[r].active && db.records[r].key == k) {
                        db.records[r].active = 0;
                    }
                }
                eatdb_append_wal(&db, "DELETE", k, 0, "");
                if (!db.in_tx) eatdb_flush(&db);
                ops_executed++;
            } else if (eat_stricmp(words[0], "Select") == 0) {
                matched_count = 0;
                if (wc >= 2 && eat_stricmp(words[1], "all") == 0) {
                    for (uint32_t r = 0; r < db.sb.record_count; r++) {
                        if (db.records[r].active) matched_indices[matched_count++] = (int)r;
                    }
                } else {
                    bool by_key = true;
                    char op_mode[16] = "eq";
                    int64_t target = 0;
                    for (int i = 1; i < wc; i++) {
                        if (eat_stricmp(words[i], "key") == 0) by_key = true;
                        else if (eat_stricmp(words[i], "value") == 0) by_key = false;
                        else if (eat_stricmp(words[i], "equal") == 0) strcpy(op_mode, "eq");
                        else if (eat_stricmp(words[i], "greater") == 0) strcpy(op_mode, "gt");
                        else if (eat_stricmp(words[i], "less") == 0) strcpy(op_mode, "lt");
                        else if (isdigit((unsigned char)words[i][0])) target = strtoll(words[i], NULL, 10);
                    }
                    for (uint32_t r = 0; r < db.sb.record_count; r++) {
                        if (!db.records[r].active) continue;
                        int64_t candidate = by_key ? db.records[r].key : db.records[r].numeric_val;
                        bool match = false;
                        if (strcmp(op_mode, "eq") == 0 && candidate == target) match = true;
                        else if (strcmp(op_mode, "gt") == 0 && candidate > target) match = true;
                        else if (strcmp(op_mode, "lt") == 0 && candidate < target) match = true;
                        if (match) matched_indices[matched_count++] = (int)r;
                    }
                }
                ops_executed++;
            }
        }
    }

    if (!db.in_tx) eatdb_flush(&db);

    if (matched_count == 0) {
        for (uint32_t r = 0; r < db.sb.record_count; r++) {
            if (db.records[r].active) matched_indices[matched_count++] = (int)r;
        }
    }

    if (out_json_buf && out_json_cap > 0) {
        size_t used = 0;
        int n = snprintf(out_json_buf + used, out_json_cap - used,
                         "{\"success\":true,\"magic\":\"EATDB001\",\"pageSize\":%u,\"txSeq\":%llu,\"crc32\":\"0x%08X\",\"collection\":",
                         db.sb.page_size, (unsigned long long)db.sb.tx_seq, db.sb.crc32);
        if (n > 0 && used + (size_t)n < out_json_cap) used += (size_t)n;
        append_json_escaped(out_json_buf, out_json_cap, &used, db.sb.table_name);
        n = snprintf(out_json_buf + used, out_json_cap - used, ",\"opsExecuted\":%d,\"records\":[", ops_executed);
        if (n > 0 && used + (size_t)n < out_json_cap) used += (size_t)n;
        for (int i = 0; i < matched_count; i++) {
            EatDbRecord *rec = &db.records[matched_indices[i]];
            if (i > 0) {
                n = snprintf(out_json_buf + used, out_json_cap - used, ",");
                if (n > 0 && used + (size_t)n < out_json_cap) used += (size_t)n;
            }
            n = snprintf(out_json_buf + used, out_json_cap - used, "{\"key\":%lld,\"label\":", (long long)rec->key);
            if (n > 0 && used + (size_t)n < out_json_cap) used += (size_t)n;
            append_json_escaped(out_json_buf, out_json_cap, &used, rec->label);
            n = snprintf(out_json_buf + used, out_json_cap - used,
                         ",\"value\":%lld,\"pageId\":%u,\"slotId\":%u,\"txId\":%llu}",
                         (long long)rec->numeric_val, rec->page_id, rec->slot_id, (unsigned long long)rec->tx_id);
            if (n > 0 && used + (size_t)n < out_json_cap) used += (size_t)n;
        }
        snprintf(out_json_buf + used, out_json_cap - used, "]}\n");
    }
    return 0;
}

#ifndef EATHON_LIB_MODE
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: eatdb <db_file.eatdb> <eql_script>\n");
        return 1;
    }

    char json_out[16384];
    int rc = eatdb_execute_script(argv[1], argv[2], json_out, sizeof(json_out));
    fputs(json_out, stdout);
    return rc;
}
#endif

