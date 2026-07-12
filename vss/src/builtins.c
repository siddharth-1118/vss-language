#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "value.h"
#include "env.h"
#include "platform.h"
#include "json.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define strdup _strdup
#define popen _popen
#define pclose _pclose
#else
/* Enable POSIX extensions for popen, usleep, getaddrinfo etc. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include <unistd.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

// MD5 and SHA256 self-contained implementations
#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

static void md5(const uint8_t *initial_msg, size_t initial_len, uint8_t *digest) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xefcdab89;
    uint32_t h2 = 0x98badcfe;
    uint32_t h3 = 0x10325476;

    static const uint32_t r[] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };

    static const uint32_t k[] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    size_t new_len = ((((initial_len + 8) / 64) + 1) * 64);
    uint8_t *msg = calloc(new_len, 1);
    memcpy(msg, initial_msg, initial_len);
    msg[initial_len] = 128;

    uint32_t bits_len = (uint32_t)(initial_len * 8);
    memcpy(msg + new_len - 8, &bits_len, 4);

    for (size_t offset = 0; offset < new_len; offset += 64) {
        uint32_t *w = (uint32_t *)(msg + offset);
        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;

        for (uint32_t i = 0; i < 64; i++) {
            uint32_t f, g;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7 * i) % 16;
            }
            uint32_t temp = d;
            d = c;
            c = b;
            b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
            a = temp;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
    }
    free(msg);

    memcpy(digest, &h0, 4);
    memcpy(digest + 4, &h1, 4);
    memcpy(digest + 8, &h2, 4);
    memcpy(digest + 12, &h3, 4);
}

// SHA-256 implementation
static const uint32_t sha256_k[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256(const uint8_t *data, size_t len, uint8_t *digest) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    size_t new_len = (len + 8 + 64) & ~63;
    uint8_t *msg = calloc(new_len, 1);
    memcpy(msg, data, len);
    msg[len] = 0x80;
    
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) {
        msg[new_len - 1 - i] = (uint8_t)(bits >> (i * 8));
    }
    
    for (size_t offset = 0; offset < new_len; offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            uint8_t *p = msg + offset + i * 4;
            w[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = ((w[i-15] >> 7) | (w[i-15] << 25)) ^ ((w[i-15] >> 18) | (w[i-15] << 14)) ^ (w[i-15] >> 3);
            uint32_t s1 = ((w[i-2] >> 17) | (w[i-2] << 15)) ^ ((w[i-2] >> 19) | (w[i-2] << 13)) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
        
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = k + S1 + ch + sha256_k[i] + w[i];
            uint32_t S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            
            k = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += k;
    }
    free(msg);
    
    for (int i = 0; i < 8; i++) {
        digest[i*4]   = (uint8_t)(h[i] >> 24);
        digest[i*4+1] = (uint8_t)(h[i] >> 16);
        digest[i*4+2] = (uint8_t)(h[i] >> 8);
        digest[i*4+3] = (uint8_t)h[i];
    }
}

// Helper to duplicate string safely
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

// ─────────────────────────────────────────────────────────────────────────────
// SQLITE DYNAMIC LINKER & MOCK ENGINE
// ─────────────────────────────────────────────────────────────────────────────
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef int (*fn_sqlite3_open)(const char *filename, sqlite3 **ppDb);
typedef int (*fn_sqlite3_close)(sqlite3*);
typedef int (*fn_sqlite3_exec)(sqlite3*, const char *sql, int (*callback)(void*,int,char**,char**), void*, char **errmsg);
typedef int (*fn_sqlite3_prepare_v2)(sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail);
typedef int (*fn_sqlite3_step)(sqlite3_stmt*);
typedef int (*fn_sqlite3_finalize)(sqlite3_stmt*);
typedef int (*fn_sqlite3_column_count)(sqlite3_stmt *pStmt);
typedef const char *(*fn_sqlite3_column_name)(sqlite3_stmt*, int N);
typedef const unsigned char *(*fn_sqlite3_column_text)(sqlite3_stmt*, int iCol);

static fn_sqlite3_open p_sqlite3_open = NULL;
static fn_sqlite3_close p_sqlite3_close = NULL;
static fn_sqlite3_exec p_sqlite3_exec = NULL;
static fn_sqlite3_prepare_v2 p_sqlite3_prepare_v2 = NULL;
static fn_sqlite3_step p_sqlite3_step = NULL;
static fn_sqlite3_finalize p_sqlite3_finalize = NULL;
static fn_sqlite3_column_count p_sqlite3_column_count = NULL;
static fn_sqlite3_column_name p_sqlite3_column_name = NULL;
static fn_sqlite3_column_text p_sqlite3_column_text = NULL;

static bool sqlite_loaded = false;
static void *sqlite_lib = NULL;

static void load_sqlite(void) {
    if (sqlite_loaded) return;
#ifdef _WIN32
    sqlite_lib = LoadLibraryA("sqlite3.dll");
    if (sqlite_lib) {
        p_sqlite3_open = (fn_sqlite3_open)GetProcAddress(sqlite_lib, "sqlite3_open");
        p_sqlite3_close = (fn_sqlite3_close)GetProcAddress(sqlite_lib, "sqlite3_close");
        p_sqlite3_exec = (fn_sqlite3_exec)GetProcAddress(sqlite_lib, "sqlite3_exec");
        p_sqlite3_prepare_v2 = (fn_sqlite3_prepare_v2)GetProcAddress(sqlite_lib, "sqlite3_prepare_v2");
        p_sqlite3_step = (fn_sqlite3_step)GetProcAddress(sqlite_lib, "sqlite3_step");
        p_sqlite3_finalize = (fn_sqlite3_finalize)GetProcAddress(sqlite_lib, "sqlite3_finalize");
        p_sqlite3_column_count = (fn_sqlite3_column_count)GetProcAddress(sqlite_lib, "sqlite3_column_count");
        p_sqlite3_column_name = (fn_sqlite3_column_name)GetProcAddress(sqlite_lib, "sqlite3_column_name");
        p_sqlite3_column_text = (fn_sqlite3_column_text)GetProcAddress(sqlite_lib, "sqlite3_column_text");
    }
#else
    const char *libs[] = {"libsqlite3.dylib", "libsqlite3.so", "libsqlite3.so.0", "libsqlite3.so.3", NULL};
    for (int i = 0; libs[i]; i++) {
        sqlite_lib = dlopen(libs[i], RTLD_LAZY);
        if (sqlite_lib) {
            p_sqlite3_open = (fn_sqlite3_open)dlsym(sqlite_lib, "sqlite3_open");
            p_sqlite3_close = (fn_sqlite3_close)dlsym(sqlite_lib, "sqlite3_close");
            p_sqlite3_exec = (fn_sqlite3_exec)dlsym(sqlite_lib, "sqlite3_exec");
            p_sqlite3_prepare_v2 = (fn_sqlite3_prepare_v2)dlsym(sqlite_lib, "sqlite3_prepare_v2");
            p_sqlite3_step = (fn_sqlite3_step)dlsym(sqlite_lib, "sqlite3_step");
            p_sqlite3_finalize = (fn_sqlite3_finalize)dlsym(sqlite_lib, "sqlite3_finalize");
            p_sqlite3_column_count = (fn_sqlite3_column_count)dlsym(sqlite_lib, "sqlite3_column_count");
            p_sqlite3_column_name = (fn_sqlite3_column_name)dlsym(sqlite_lib, "sqlite3_column_name");
            p_sqlite3_column_text = (fn_sqlite3_column_text)dlsym(sqlite_lib, "sqlite3_column_text");
            break;
        }
    }
#endif
    if (p_sqlite3_open && p_sqlite3_close && p_sqlite3_prepare_v2 && p_sqlite3_step && p_sqlite3_finalize) {
        sqlite_loaded = true;
    }
}

typedef struct {
    bool is_mock;
    char *filepath;
    sqlite3 *real_db;
} VssDatabase;

static VSS_Value mock_db_execute(VssDatabase *db, const char *sql, char **out_error_msg) {
    // A simple mock for SQLite that supports CREATE TABLE and INSERT INTO using JSON storage
    // Format in db file: { "tables": { "tablename": [ { "col": "val" } ] } }
    FILE *f = fopen(db->filepath, "rb");
    VSS_Value db_state;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        rewind(f);
        char *buf = malloc(sz + 1);
        size_t rb = fread(buf, 1, sz, f);
        buf[rb] = '\0';
        fclose(f);
        bool parse_err = false;
        char *parse_err_msg = NULL;
        db_state = vss_json_parse(buf, &parse_err, &parse_err_msg);
        free(buf);
        if (parse_err) {
            vss_value_release(db_state);
            db_state = vss_value_new_map();
            VSS_Value tables = vss_value_new_map();
            vss_env_define_const((VSS_Env*)db_state.as.map, "tables", tables);
            vss_value_release(tables);
        }
    } else {
        db_state = vss_value_new_map();
        VSS_Value tables = vss_value_new_map();
        // Since VSS_Env defines properties, we can just define keys directly inside map
        VSS_ValMap *m = db_state.as.map;
        m->entries = realloc(m->entries, sizeof(VSS_ValMapEntry) * 1);
        m->entries[0].key = safe_strdup("tables");
        m->entries[0].value = tables;
        vss_value_retain(tables);
        m->count = 1;
        vss_value_release(tables);
    }

    VSS_Value tables_val;
    // Helper search
    VSS_ValMap *state_map = db_state.as.map;
    VSS_ValMap *tables_map = NULL;
    for (size_t i = 0; i < state_map->count; i++) {
        if (strcmp(state_map->entries[i].key, "tables") == 0) {
            tables_map = state_map->entries[i].value.as.map;
            break;
        }
    }

    char sql_lower[512];
    strncpy(sql_lower, sql, sizeof(sql_lower)-1);
    sql_lower[sizeof(sql_lower)-1] = '\0';
    for (int i = 0; sql_lower[i]; i++) sql_lower[i] = tolower((unsigned char)sql_lower[i]);

    if (strstr(sql_lower, "create table")) {
        // extract table name
        char tablename[128] = {0};
        sscanf(sql, "%*s %*s %127s", tablename);
        // remove parentheses if any
        char *paren = strchr(tablename, '(');
        if (paren) *paren = '\0';
        
        // Add empty list for table
        VSS_Value empty_list = vss_value_new_list();
        tables_map->entries = realloc(tables_map->entries, sizeof(VSS_ValMapEntry) * (tables_map->count + 1));
        tables_map->entries[tables_map->count].key = safe_strdup(tablename);
        tables_map->entries[tables_map->count].value = empty_list;
        vss_value_retain(empty_list);
        tables_map->count++;
        vss_value_release(empty_list);

    } else if (strstr(sql_lower, "insert into")) {
        char tablename[128] = {0};
        sscanf(sql, "%*s %*s %127s", tablename);
        char *values_ptr = strstr(sql_lower, "values");
        if (values_ptr) {
            // Find corresponding array in tables
            VSS_ValList *table_rows = NULL;
            for (size_t i = 0; i < tables_map->count; i++) {
                if (strcmp(tables_map->entries[i].key, tablename) == 0) {
                    table_rows = tables_map->entries[i].value.as.list;
                    break;
                }
            }
            if (table_rows) {
                // simple parser for values (e.g. values ('Asha', 22))
                VSS_Value row = vss_value_new_map();
                VSS_ValMap *row_map = row.as.map;
                
                // We'll just parse strings and numbers in parenthesis
                const char *val_str = sql + (values_ptr - sql_lower) + 6;
                while (*val_str && *val_str != '(') val_str++;
                if (*val_str == '(') val_str++;
                
                int col_idx = 0;
                while (*val_str && *val_str != ')') {
                    while (*val_str && (isspace((unsigned char)*val_str) || *val_str == ',')) val_str++;
                    if (*val_str == '\'') {
                        val_str++;
                        char val_buf[256] = {0};
                        int v_len = 0;
                        while (*val_str && *val_str != '\'') {
                            val_buf[v_len++] = *val_str++;
                        }
                        if (*val_str == '\'') val_str++;
                        
                        char key_name[32];
                        sprintf(key_name, "col%d", col_idx++);
                        VSS_Value v_val = vss_value_new_string(val_buf);
                        row_map->entries = realloc(row_map->entries, sizeof(VSS_ValMapEntry) * (row_map->count + 1));
                        row_map->entries[row_map->count].key = safe_strdup(key_name);
                        row_map->entries[row_map->count].value = v_val;
                        vss_value_retain(v_val);
                        row_map->count++;
                        vss_value_release(v_val);
                    } else if (isdigit((unsigned char)*val_str) || *val_str == '-') {
                        char num_buf[32] = {0};
                        int n_len = 0;
                        while (*val_str && (isdigit((unsigned char)*val_str) || *val_str == '.' || *val_str == '-')) {
                            num_buf[n_len++] = *val_str++;
                        }
                        double d = atof(num_buf);
                        char key_name[32];
                        sprintf(key_name, "col%d", col_idx++);
                        VSS_Value v_val = vss_value_new_number(d);
                        row_map->entries = realloc(row_map->entries, sizeof(VSS_ValMapEntry) * (row_map->count + 1));
                        row_map->entries[row_map->count].key = safe_strdup(key_name);
                        row_map->entries[row_map->count].value = v_val;
                        vss_value_retain(v_val);
                        row_map->count++;
                        vss_value_release(v_val);
                    } else {
                        val_str++;
                    }
                }
                
                // Add row to table
                if (table_rows->count >= table_rows->capacity) {
                    table_rows->capacity = table_rows->capacity == 0 ? 8 : table_rows->capacity * 2;
                    table_rows->items = realloc(table_rows->items, sizeof(VSS_Value) * table_rows->capacity);
                }
                table_rows->items[table_rows->count++] = row;
                vss_value_retain(row);
                vss_value_release(row);
            }
        }
    }

    // Write back
    char *serialized = vss_json_serialize(db_state);
    f = fopen(db->filepath, "wb");
    if (f) {
        fwrite(serialized, 1, strlen(serialized), f);
        fclose(f);
    }
    free(serialized);
    vss_value_release(db_state);
    return vss_value_new_bool(true);
}

static VSS_Value mock_db_query(VssDatabase *db, const char *sql, char **out_error_msg) {
    // Simple mock select: select * from <table> returns the table array
    char sql_lower[512];
    strncpy(sql_lower, sql, sizeof(sql_lower)-1);
    sql_lower[sizeof(sql_lower)-1] = '\0';
    for (int i = 0; sql_lower[i]; i++) sql_lower[i] = tolower((unsigned char)sql_lower[i]);

    char tablename[128] = {0};
    char *from_ptr = strstr(sql_lower, "from");
    if (from_ptr) {
        sscanf(sql + (from_ptr - sql_lower) + 4, "%127s", tablename);
    }
    
    FILE *f = fopen(db->filepath, "rb");
    if (!f) return vss_value_new_list();

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 1);
    size_t rb = fread(buf, 1, sz, f);
    buf[rb] = '\0';
    fclose(f);

    bool parse_err = false;
    char *parse_err_msg = NULL;
    VSS_Value db_state = vss_json_parse(buf, &parse_err, &parse_err_msg);
    free(buf);
    if (parse_err) {
        vss_value_release(db_state);
        return vss_value_new_list();
    }

    VSS_ValMap *state_map = db_state.as.map;
    VSS_ValMap *tables_map = NULL;
    for (size_t i = 0; i < state_map->count; i++) {
        if (strcmp(state_map->entries[i].key, "tables") == 0) {
            tables_map = state_map->entries[i].value.as.map;
            break;
        }
    }

    VSS_Value result_list = vss_value_new_list();
    if (tables_map) {
        for (size_t i = 0; i < tables_map->count; i++) {
            if (strcmp(tables_map->entries[i].key, tablename) == 0) {
                // Copy list
                VSS_ValList *src = tables_map->entries[i].value.as.list;
                VSS_ValList *dest = result_list.as.list;
                dest->items = malloc(sizeof(VSS_Value) * src->count);
                dest->count = src->count;
                dest->capacity = src->count;
                for (size_t j = 0; j < src->count; j++) {
                    dest->items[j] = src->items[j];
                    vss_value_retain(src->items[j]);
                }
                break;
            }
        }
    }
    vss_value_release(db_state);
    return result_list;
}

// ─────────────────────────────────────────────────────────────────────────────
// BUILT-IN IMPLEMENTATIONS
// ─────────────────────────────────────────────────────────────────────────────

// Standard library file operations
static VSS_Value builtin_exists(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("__exists expects a string path");
        return vss_value_new_empty();
    }
    return vss_value_new_bool(vss_file_exists(args[0].as.string->chars));
}

static VSS_Value builtin_read(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("__read expects a string path");
        return vss_value_new_empty();
    }
    const char *path = args[0].as.string->chars;
    FILE *f = fopen(path, "rb");
    if (!f) {
        *out_error = true;
        *out_error_msg = malloc(strlen(path) + 32);
        sprintf(*out_error_msg, "Could not open file: %s", path);
        return vss_value_new_empty();
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc(size + 1);
    size_t rb = fread(buf, 1, size, f);
    fclose(f);
    buf[rb] = '\0';
    VSS_Value res = vss_value_new_string(buf);
    free(buf);
    return res;
}

static VSS_Value builtin_write(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_STRING || args[1].type != VSS_VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("__write expects content and path");
        return vss_value_new_empty();
    }
    const char *content = args[0].as.string->chars;
    const char *path = args[1].as.string->chars;
    FILE *f = fopen(path, "wb");
    if (!f) {
        *out_error = true;
        *out_error_msg = safe_strdup("Could not open file for writing");
        return vss_value_new_empty();
    }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    return vss_value_new_empty();
}

static VSS_Value builtin_add(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_STRING || args[1].type != VSS_VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("__add expects content and path");
        return vss_value_new_empty();
    }
    const char *content = args[0].as.string->chars;
    const char *path = args[1].as.string->chars;
    FILE *f = fopen(path, "ab");
    if (!f) {
        *out_error = true;
        *out_error_msg = safe_strdup("Could not open file for appending");
        return vss_value_new_empty();
    }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    return vss_value_new_empty();
}

static VSS_Value builtin_erase(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("__erase expects a string path");
        return vss_value_new_empty();
    }
    remove(args[0].as.string->chars);
    return vss_value_new_empty();
}

static VSS_Value builtin_file_list(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("__file_list expects a directory path string");
        return vss_value_new_empty();
    }
    char **filenames = NULL;
    int count = vss_list_dir(args[0].as.string->chars, &filenames);
    VSS_Value list_val = vss_value_new_list();
    VSS_ValList *l = list_val.as.list;
    l->items = malloc(sizeof(VSS_Value) * count);
    l->count = count;
    l->capacity = count;
    for (int i = 0; i < count; i++) {
        l->items[i] = vss_value_new_string(filenames[i]);
        free(filenames[i]);
    }
    free(filenames);
    return list_val;
}

// Math Builtins
static VSS_Value builtin_math_sin(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("sin expects a number"); return vss_value_new_empty();
    }
    return vss_value_new_number(sin(args[0].as.number));
}
static VSS_Value builtin_math_cos(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("cos expects a number"); return vss_value_new_empty();
    }
    return vss_value_new_number(cos(args[0].as.number));
}
static VSS_Value builtin_math_tan(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("tan expects a number"); return vss_value_new_empty();
    }
    return vss_value_new_number(tan(args[0].as.number));
}
static VSS_Value builtin_math_sqrt(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("sqrt expects a number"); return vss_value_new_empty();
    }
    return vss_value_new_number(sqrt(args[0].as.number));
}
static VSS_Value builtin_math_log(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("log expects a number"); return vss_value_new_empty();
    }
    return vss_value_new_number(log(args[0].as.number));
}
static VSS_Value builtin_math_ceil(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("ceil expects a number"); return vss_value_new_empty();
    }
    return vss_value_new_number(ceil(args[0].as.number));
}
static VSS_Value builtin_math_floor(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("floor expects a number"); return vss_value_new_empty();
    }
    return vss_value_new_number(floor(args[0].as.number));
}
static VSS_Value builtin_math_pow(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_NUMBER || args[1].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("pow expects base and exponent numbers"); return vss_value_new_empty();
    }
    return vss_value_new_number(pow(args[0].as.number, args[1].as.number));
}

// String Builtins
static VSS_Value builtin_string_length(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("length expects string"); return vss_value_new_empty();
    }
    return vss_value_new_number(strlen(args[0].as.string->chars));
}

static VSS_Value builtin_string_lower(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("lower expects string"); return vss_value_new_empty();
    }
    char *copy = safe_strdup(args[0].as.string->chars);
    for (int i = 0; copy[i]; i++) copy[i] = tolower((unsigned char)copy[i]);
    VSS_Value res = vss_value_new_string(copy);
    free(copy);
    return res;
}

static VSS_Value builtin_string_upper(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("upper expects string"); return vss_value_new_empty();
    }
    char *copy = safe_strdup(args[0].as.string->chars);
    for (int i = 0; copy[i]; i++) copy[i] = toupper((unsigned char)copy[i]);
    VSS_Value res = vss_value_new_string(copy);
    free(copy);
    return res;
}

static VSS_Value builtin_string_trim(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("trim expects string"); return vss_value_new_empty();
    }
    const char *start = args[0].as.string->chars;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    
    size_t len = end - start;
    char *trimmed = malloc(len + 1);
    memcpy(trimmed, start, len);
    trimmed[len] = '\0';
    VSS_Value res = vss_value_new_string(trimmed);
    free(trimmed);
    return res;
}

static VSS_Value builtin_string_substring(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 3 || args[0].type != VSS_VAL_STRING || args[1].type != VSS_VAL_NUMBER || args[2].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("substring expects string, start, and end index"); return vss_value_new_empty();
    }
    const char *s = args[0].as.string->chars;
    int len = (int)strlen(s);
    int start = (int)args[1].as.number;
    int end = (int)args[2].as.number;
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start > end) start = end;
    
    int sublen = end - start;
    char *sub = malloc(sublen + 1);
    memcpy(sub, s + start, sublen);
    sub[sublen] = '\0';
    VSS_Value res = vss_value_new_string(sub);
    free(sub);
    return res;
}

static VSS_Value builtin_string_find(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_STRING || args[1].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("find expects string and search-sub"); return vss_value_new_empty();
    }
    const char *s = args[0].as.string->chars;
    const char *sub = args[1].as.string->chars;
    const char *found = strstr(s, sub);
    if (!found) return vss_value_new_number(-1);
    return vss_value_new_number(found - s);
}

static VSS_Value builtin_string_replace(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 3 || args[0].type != VSS_VAL_STRING || args[1].type != VSS_VAL_STRING || args[2].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("replace expects string, old, and new substring"); return vss_value_new_empty();
    }
    const char *orig = args[0].as.string->chars;
    const char *rep = args[1].as.string->chars;
    const char *with = args[2].as.string->chars;
    
    size_t rep_len = strlen(rep);
    size_t with_len = strlen(with);
    if (rep_len == 0) return vss_value_new_string(orig);

    size_t count = 0;
    const char *tmp = orig;
    while ((tmp = strstr(tmp, rep))) {
        count++;
        tmp += rep_len;
    }
    
    size_t new_len = strlen(orig) + (with_len - rep_len) * count;
    char *result = malloc(new_len + 1);
    char *dst = result;
    const char *src = orig;
    while ((tmp = strstr(src, rep))) {
        size_t skip = tmp - src;
        memcpy(dst, src, skip);
        dst += skip;
        memcpy(dst, with, with_len);
        dst += with_len;
        src = tmp + rep_len;
    }
    strcpy(dst, src);
    VSS_Value res = vss_value_new_string(result);
    free(result);
    return res;
}

static VSS_Value builtin_string_split(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_STRING || args[1].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("split expects string and delimiter"); return vss_value_new_empty();
    }
    const char *orig = args[0].as.string->chars;
    const char *sep = args[1].as.string->chars;
    VSS_Value list = vss_value_new_list();
    VSS_ValList *l = list.as.list;
    
    size_t sep_len = strlen(sep);
    if (sep_len == 0) {
        // split into chars
        for (int i = 0; orig[i]; i++) {
            char ch[2] = {orig[i], '\0'};
            VSS_Value str_val = vss_value_new_string(ch);
            if (l->count >= l->capacity) {
                l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
                l->items = realloc(l->items, sizeof(VSS_Value) * l->capacity);
            }
            l->items[l->count++] = str_val;
            vss_value_retain(str_val);
            vss_value_release(str_val);
        }
        return list;
    }

    const char *src = orig;
    const char *tmp;
    while ((tmp = strstr(src, sep))) {
        size_t part_len = tmp - src;
        char *part = malloc(part_len + 1);
        memcpy(part, src, part_len);
        part[part_len] = '\0';
        
        VSS_Value str_val = vss_value_new_string(part);
        free(part);
        if (l->count >= l->capacity) {
            l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
            l->items = realloc(l->items, sizeof(VSS_Value) * l->capacity);
        }
        l->items[l->count++] = str_val;
        vss_value_retain(str_val);
        vss_value_release(str_val);
        src = tmp + sep_len;
    }
    VSS_Value str_val = vss_value_new_string(src);
    if (l->count >= l->capacity) {
        l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
        l->items = realloc(l->items, sizeof(VSS_Value) * l->capacity);
    }
    l->items[l->count++] = str_val;
    vss_value_retain(str_val);
    vss_value_release(str_val);
    
    return list;
}

static void append_str(char **buf, size_t *len, size_t *cap, const char *s) {
    size_t slen = strlen(s);
    while (*len + slen >= *cap) {
        *cap = *cap == 0 ? 64 : *cap * 2;
        *buf = realloc(*buf, *cap);
    }
    strcpy(*buf + *len, s);
    *len += slen;
}

static VSS_Value builtin_string_join(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_LIST || args[1].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("join expects list and separator string"); return vss_value_new_empty();
    }
    VSS_ValList *l = args[0].as.list;
    const char *sep = args[1].as.string->chars;
    
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    for (size_t i = 0; i < l->count; i++) {
        if (i > 0) append_str(&buf, &len, &cap, sep);
        char *item_str = vss_value_to_string(l->items[i]);
        append_str(&buf, &len, &cap, item_str);
        free(item_str);
    }
    VSS_Value res = vss_value_new_string(buf ? buf : "");
    if (buf) free(buf);
    return res;
}

// JSON Builtins
static VSS_Value builtin_json_read(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("json.read expects file path string"); return vss_value_new_empty();
    }
    VSS_Value content = builtin_read(1, args, out_error, out_error_msg);
    if (*out_error) return vss_value_new_empty();
    
    VSS_Value val = vss_json_parse(content.as.string->chars, out_error, out_error_msg);
    vss_value_release(content);
    return val;
}

static VSS_Value builtin_json_write(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("json.write expects file path and value"); return vss_value_new_empty();
    }
    char *serialized = vss_json_serialize(args[1]);
    VSS_Value write_args[2] = { vss_value_new_string(serialized), args[0] };
    free(serialized);
    VSS_Value res = builtin_write(2, write_args, out_error, out_error_msg);
    vss_value_release(write_args[0]);
    return res;
}

static VSS_Value builtin_json_parse(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("json.parse expects string"); return vss_value_new_empty();
    }
    return vss_json_parse(args[0].as.string->chars, out_error, out_error_msg);
}

static VSS_Value builtin_json_stringify(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1) {
        *out_error = true; *out_error_msg = safe_strdup("json.stringify expects value"); return vss_value_new_empty();
    }
    char *serialized = vss_json_serialize(args[0]);
    VSS_Value res = vss_value_new_string(serialized);
    free(serialized);
    return res;
}

// HTTP Subprocess curl Builtin
static VSS_Value builtin_http_request(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    // args: [method, url, headers_map, body]
    if (arg_count != 4 || args[0].type != VSS_VAL_STRING || args[1].type != VSS_VAL_STRING ||
        args[2].type != VSS_VAL_MAP || args[3].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("http_request expects method, url, headers, and body"); return vss_value_new_empty();
    }
    
    const char *method = args[0].as.string->chars;
    const char *url = args[1].as.string->chars;
    VSS_ValMap *headers = args[2].as.map;
    const char *body = args[3].as.string->chars;
    
    // Write body to a temporary file
    FILE *f_body = fopen("vss_temp_req_body.txt", "wb");
    if (f_body) {
        fwrite(body, 1, strlen(body), f_body);
        fclose(f_body);
    }
    
    // Construct command
    char cmd[4096];
    sprintf(cmd, "curl -s -i -X %s --data-binary @vss_temp_req_body.txt ", method);
    
    // Append headers
    for (size_t i = 0; i < headers->count; i++) {
        char *val_str = vss_value_to_string(headers->entries[i].value);
        char header_flag[512];
        snprintf(header_flag, sizeof(header_flag), "-H \"%s: %s\" ", headers->entries[i].key, val_str);
        strcat(cmd, header_flag);
        free(val_str);
    }
    
    // output body and header files
    strcat(cmd, "-o vss_temp_res_body.txt -D vss_temp_res_headers.txt ");
    // Append URL
    strcat(cmd, "\"");
    strcat(cmd, url);
    strcat(cmd, "\"");
    
    vss_execute_cmd(cmd);
    
    remove("vss_temp_req_body.txt");
    
    // Read status code and headers
    int status_code = 500;
    VSS_Value headers_map = vss_value_new_map();
    FILE *f_h = fopen("vss_temp_res_headers.txt", "r");
    if (f_h) {
        char line[512];
        if (fgets(line, sizeof(line), f_h)) {
            // Read HTTP/1.1 200 OK
            sscanf(line, "%*s %d", &status_code);
        }
        while (fgets(line, sizeof(line), f_h)) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char *val = colon + 1;
                while (*val && isspace((unsigned char)*val)) val++;
                int v_len = strlen(val);
                while (v_len > 0 && isspace((unsigned char)val[v_len-1])) {
                    val[v_len-1] = '\0';
                    v_len--;
                }
                
                VSS_Value v_val = vss_value_new_string(val);
                VSS_ValMap *m = headers_map.as.map;
                m->entries = realloc(m->entries, sizeof(VSS_ValMapEntry) * (m->count + 1));
                m->entries[m->count].key = safe_strdup(line);
                m->entries[m->count].value = v_val;
                vss_value_retain(v_val);
                m->count++;
                vss_value_release(v_val);
            }
        }
        fclose(f_h);
    }
    remove("vss_temp_res_headers.txt");
    
    // Read body
    char *res_body_str = "";
    FILE *f_b = fopen("vss_temp_res_body.txt", "rb");
    if (f_b) {
        fseek(f_b, 0, SEEK_END);
        long sz = ftell(f_b);
        rewind(f_b);
        char *buf = malloc(sz + 1);
        size_t rb = fread(buf, 1, sz, f_b);
        buf[rb] = '\0';
        fclose(f_b);
        res_body_str = buf;
    }
    remove("vss_temp_res_body.txt");
    
    VSS_Value response = vss_value_new_map();
    VSS_ValMap *resp_m = response.as.map;
    
    VSS_Value status_val = vss_value_new_number(status_code);
    VSS_Value body_val = vss_value_new_string(res_body_str);
    if (strlen(res_body_str) > 0) free(res_body_str);
    
    resp_m->entries = realloc(resp_m->entries, sizeof(VSS_ValMapEntry) * 3);
    resp_m->entries[0].key = safe_strdup("status");
    resp_m->entries[0].value = status_val;
    vss_value_retain(status_val);
    resp_m->entries[1].key = safe_strdup("body");
    resp_m->entries[1].value = body_val;
    vss_value_retain(body_val);
    resp_m->entries[2].key = safe_strdup("headers");
    resp_m->entries[2].value = headers_map;
    vss_value_retain(headers_map);
    resp_m->count = 3;
    
    vss_value_release(status_val);
    vss_value_release(body_val);
    vss_value_release(headers_map);
    
    return response;
}

// Database SQLite dynamic loader Builtins
static VSS_Value builtin_db_open(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("database.open expects file path string"); return vss_value_new_empty();
    }
    load_sqlite();
    VssDatabase *db = malloc(sizeof(VssDatabase));
    db->filepath = safe_strdup(args[0].as.string->chars);
    db->real_db = NULL;
    db->is_mock = !sqlite_loaded;
    
    if (!db->is_mock) {
        int rc = p_sqlite3_open(db->filepath, &db->real_db);
        if (rc != 0) {
            *out_error = true;
            *out_error_msg = safe_strdup("Could not open real SQLite database");
            free(db->filepath);
            free(db);
            return vss_value_new_empty();
        }
    }
    
    // We package the database wrapper object inside a list containing [is_mock, filepath, real_db_ptr]
    VSS_Value db_val = vss_value_new_list();
    VSS_ValList *l = db_val.as.list;
    l->items = malloc(sizeof(VSS_Value) * 3);
    l->items[0] = vss_value_new_bool(db->is_mock);
    l->items[1] = vss_value_new_string(db->filepath);
    l->items[2] = vss_value_new_number((double)(uintptr_t)db);
    l->count = 3;
    l->capacity = 3;
    return db_val;
}

static VSS_Value builtin_db_execute(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_LIST || args[1].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("__db_execute expects db_handle and sql"); return vss_value_new_empty();
    }
    VSS_ValList *l = args[0].as.list;
    VssDatabase *db = (VssDatabase*)(uintptr_t)l->items[2].as.number;
    const char *sql = args[1].as.string->chars;
    
    if (db->is_mock) {
        return mock_db_execute(db, sql, out_error_msg);
    } else {
        char *errmsg = NULL;
        int rc = p_sqlite3_exec(db->real_db, sql, NULL, NULL, &errmsg);
        if (rc != 0) {
            *out_error = true;
            *out_error_msg = safe_strdup(errmsg ? errmsg : "SQLite exec failed");
            return vss_value_new_empty();
        }
        return vss_value_new_bool(true);
    }
}

static VSS_Value builtin_db_query(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_LIST || args[1].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("__db_query expects db_handle and sql"); return vss_value_new_empty();
    }
    VSS_ValList *l = args[0].as.list;
    VssDatabase *db = (VssDatabase*)(uintptr_t)l->items[2].as.number;
    const char *sql = args[1].as.string->chars;
    
    if (db->is_mock) {
        return mock_db_query(db, sql, out_error_msg);
    } else {
        sqlite3_stmt *stmt = NULL;
        int rc = p_sqlite3_prepare_v2(db->real_db, sql, -1, &stmt, NULL);
        if (rc != 0) {
            *out_error = true;
            *out_error_msg = safe_strdup("SQLite query preparation failed");
            return vss_value_new_empty();
        }
        
        VSS_Value list = vss_value_new_list();
        VSS_ValList *dest = list.as.list;
        
        int col_count = p_sqlite3_column_count(stmt);
        while (p_sqlite3_step(stmt) == 100) { // SQLITE_ROW
            VSS_Value row = vss_value_new_map();
            VSS_ValMap *m = row.as.map;
            for (int i = 0; i < col_count; i++) {
                const char *name = p_sqlite3_column_name(stmt, i);
                const char *text = (const char*)p_sqlite3_column_text(stmt, i);
                
                VSS_Value text_val = vss_value_new_string(text ? text : "");
                m->entries = realloc(m->entries, sizeof(VSS_ValMapEntry) * (m->count + 1));
                m->entries[m->count].key = safe_strdup(name);
                m->entries[m->count].value = text_val;
                vss_value_retain(text_val);
                m->count++;
                vss_value_release(text_val);
            }
            if (dest->count >= dest->capacity) {
                dest->capacity = dest->capacity == 0 ? 8 : dest->capacity * 2;
                dest->items = realloc(dest->items, sizeof(VSS_Value) * dest->capacity);
            }
            dest->items[dest->count++] = row;
            vss_value_retain(row);
            vss_value_release(row);
        }
        p_sqlite3_finalize(stmt);
        return list;
    }
}

// Time Builtins
static VSS_Value builtin_time_now(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    (void)args; (void)arg_count; (void)out_error; (void)out_error_msg;
    return vss_value_new_number((double)time(NULL));
}

static VSS_Value builtin_time_sleep(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_NUMBER) {
        *out_error = true; *out_error_msg = safe_strdup("sleep expects seconds number"); return vss_value_new_empty();
    }
    double sec = args[0].as.number;
#ifdef _WIN32
    Sleep((DWORD)(sec * 1000.0));
#else
    usleep((unsigned int)(sec * 1000000.0));
#endif
    return vss_value_new_empty();
}

static VSS_Value builtin_time_format(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2 || args[0].type != VSS_VAL_NUMBER || args[1].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("time.format expects timestamp and format string"); return vss_value_new_empty();
    }
    time_t ts = (time_t)args[0].as.number;
    const char *fmt = args[1].as.string->chars;
    struct tm *tm_info = localtime(&ts);
    char buf[128];
    strftime(buf, sizeof(buf), fmt, tm_info);
    return vss_value_new_string(buf);
}

// Random Builtins
static VSS_Value builtin_random_number(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    (void)args; (void)arg_count; (void)out_error; (void)out_error_msg;
    static bool seeded = false;
    if (!seeded) { srand((unsigned int)time(NULL)); seeded = true; }
    double r = (double)rand() / (double)RAND_MAX;
    return vss_value_new_number(r);
}

// System Builtins
static int global_argc = 0;
static char **global_argv = NULL;

void vss_set_args(int argc, char **argv) {
    global_argc = argc;
    global_argv = argv;
}

static VSS_Value builtin_system_args(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    (void)args; (void)arg_count; (void)out_error; (void)out_error_msg;
    VSS_Value list = vss_value_new_list();
    VSS_ValList *l = list.as.list;
    l->items = malloc(sizeof(VSS_Value) * global_argc);
    l->count = global_argc;
    l->capacity = global_argc;
    for (int i = 0; i < global_argc; i++) {
        l->items[i] = vss_value_new_string(global_argv[i]);
    }
    return list;
}

static VSS_Value builtin_system_env(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("env expects environment variable name string"); return vss_value_new_empty();
    }
    const char *val = getenv(args[0].as.string->chars);
    return vss_value_new_string(val ? val : "");
}

static VSS_Value builtin_system_exit(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    int code = 0;
    if (arg_count == 1 && args[0].type == VSS_VAL_NUMBER) {
        code = (int)args[0].as.number;
    }
    exit(code);
    return vss_value_new_empty();
}

static VSS_Value builtin_system_platform(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    (void)args; (void)arg_count; (void)out_error; (void)out_error_msg;
#ifdef _WIN32
    return vss_value_new_string("windows");
#elif __APPLE__
    return vss_value_new_string("macos");
#else
    return vss_value_new_string("linux");
#endif
}

static VSS_Value builtin_system_run(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("system.run expects shell command string"); return vss_value_new_empty();
    }
    // Runs shell command, capture stdout in a string and return it
    const char *cmd = args[0].as.string->chars;
    FILE *pf = popen(cmd, "r");
    if (!pf) {
        *out_error = true; *out_error_msg = safe_strdup("Failed to run shell command"); return vss_value_new_empty();
    }
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    char line[512];
    while (fgets(line, sizeof(line), pf)) {
        append_str(&buf, &len, &cap, line);
    }
    pclose(pf);
    VSS_Value res = vss_value_new_string(buf ? buf : "");
    if (buf) free(buf);
    return res;
}

// Crypto Builtins
static VSS_Value builtin_crypto_md5(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("md5 expects text string"); return vss_value_new_empty();
    }
    const char *text = args[0].as.string->chars;
    uint8_t digest[16];
    md5((const uint8_t*)text, strlen(text), digest);
    
    char hex[33];
    for (int i = 0; i < 16; i++) {
        sprintf(hex + i*2, "%02x", digest[i]);
    }
    return vss_value_new_string(hex);
}

static VSS_Value builtin_crypto_sha256(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("sha256 expects text string"); return vss_value_new_empty();
    }
    const char *text = args[0].as.string->chars;
    uint8_t digest[32];
    sha256((const uint8_t*)text, strlen(text), digest);
    
    char hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hex + i*2, "%02x", digest[i]);
    }
    return vss_value_new_string(hex);
}

// Network Builtins
static VSS_Value builtin_network_resolve(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("resolve expects hostname string"); return vss_value_new_empty();
    }
    const char *host = args[0].as.string->chars;
    struct addrinfo hints, *infoptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    
    int result = getaddrinfo(host, NULL, &hints, &infoptr);
    if (result != 0) {
        return vss_value_new_string("");
    }
    
    char ip[64] = {0};
    getnameinfo(infoptr->ai_addr, infoptr->ai_addrlen, ip, sizeof(ip), NULL, 0, NI_NUMERICHOST);
    freeaddrinfo(infoptr);
    return vss_value_new_string(ip);
}

static VSS_Value builtin_network_ping(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1 || args[0].type != VSS_VAL_STRING) {
        *out_error = true; *out_error_msg = safe_strdup("ping expects host string"); return vss_value_new_empty();
    }
    // Runs shell ping command based on platform
    const char *host = args[0].as.string->chars;
    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "ping -n 1 -w 1000 %s >nul 2>&1", host);
#else
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s >/dev/null 2>&1", host);
#endif
    int res = vss_execute_cmd(cmd);
    return vss_value_new_bool(res == 0);
}

// VSS Size built-in (generic list/map/string length helper)
static VSS_Value builtin_size(size_t arg_count, VSS_Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1) {
        *out_error = true; *out_error_msg = safe_strdup("size of expects exactly 1 argument"); return vss_value_new_empty();
    }
    VSS_Value val = args[0];
    if (val.type == VSS_VAL_LIST) {
        return vss_value_new_number(val.as.list->count);
    } else if (val.type == VSS_VAL_MAP) {
        return vss_value_new_number(val.as.map->count);
    } else if (val.type == VSS_VAL_STRING) {
        return vss_value_new_number(strlen(val.as.string->chars));
    } else {
        *out_error = true; *out_error_msg = safe_strdup("size of expects a list, map, or string"); return vss_value_new_empty();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// REGISTRATION
// ─────────────────────────────────────────────────────────────────────────────
void vss_register_builtins(VSS_Env *env) {
    vss_env_define(env, "__size", vss_value_new_native(builtin_size));
    vss_env_define(env, "__exists", vss_value_new_native(builtin_exists));
    vss_env_define(env, "__read", vss_value_new_native(builtin_read));
    vss_env_define(env, "__write", vss_value_new_native(builtin_write));
    vss_env_define(env, "__add", vss_value_new_native(builtin_add));
    vss_env_define(env, "__erase", vss_value_new_native(builtin_erase));
    vss_env_define(env, "__file_list", vss_value_new_native(builtin_file_list));

    // Math
    vss_env_define(env, "__math_sin", vss_value_new_native(builtin_math_sin));
    vss_env_define(env, "__math_cos", vss_value_new_native(builtin_math_cos));
    vss_env_define(env, "__math_tan", vss_value_new_native(builtin_math_tan));
    vss_env_define(env, "__math_sqrt", vss_value_new_native(builtin_math_sqrt));
    vss_env_define(env, "__math_log", vss_value_new_native(builtin_math_log));
    vss_env_define(env, "__math_ceil", vss_value_new_native(builtin_math_ceil));
    vss_env_define(env, "__math_floor", vss_value_new_native(builtin_math_floor));
    vss_env_define(env, "__math_pow", vss_value_new_native(builtin_math_pow));

    // String
    vss_env_define(env, "__string_length", vss_value_new_native(builtin_string_length));
    vss_env_define(env, "__string_lower", vss_value_new_native(builtin_string_lower));
    vss_env_define(env, "__string_upper", vss_value_new_native(builtin_string_upper));
    vss_env_define(env, "__string_trim", vss_value_new_native(builtin_string_trim));
    vss_env_define(env, "__string_substring", vss_value_new_native(builtin_string_substring));
    vss_env_define(env, "__string_find", vss_value_new_native(builtin_string_find));
    vss_env_define(env, "__string_replace", vss_value_new_native(builtin_string_replace));
    vss_env_define(env, "__string_split", vss_value_new_native(builtin_string_split));
    vss_env_define(env, "__string_join", vss_value_new_native(builtin_string_join));

    // JSON
    vss_env_define(env, "__json_read", vss_value_new_native(builtin_json_read));
    vss_env_define(env, "__json_write", vss_value_new_native(builtin_json_write));
    vss_env_define(env, "__json_parse", vss_value_new_native(builtin_json_parse));
    vss_env_define(env, "__json_stringify", vss_value_new_native(builtin_json_stringify));

    // HTTP
    vss_env_define(env, "__http_request", vss_value_new_native(builtin_http_request));

    // Database
    vss_env_define(env, "__db_open", vss_value_new_native(builtin_db_open));
    vss_env_define(env, "__db_execute", vss_value_new_native(builtin_db_execute));
    vss_env_define(env, "__db_query", vss_value_new_native(builtin_db_query));

    // Time
    vss_env_define(env, "__time_now", vss_value_new_native(builtin_time_now));
    vss_env_define(env, "__time_sleep", vss_value_new_native(builtin_time_sleep));
    vss_env_define(env, "__time_format", vss_value_new_native(builtin_time_format));

    // Random
    vss_env_define(env, "__random_number", vss_value_new_native(builtin_random_number));

    // System
    vss_env_define(env, "__system_args", vss_value_new_native(builtin_system_args));
    vss_env_define(env, "__system_env", vss_value_new_native(builtin_system_env));
    vss_env_define(env, "__system_exit", vss_value_new_native(builtin_system_exit));
    vss_env_define(env, "__system_platform", vss_value_new_native(builtin_system_platform));
    vss_env_define(env, "__system_run", vss_value_new_native(builtin_system_run));

    // Crypto
    vss_env_define(env, "__crypto_md5", vss_value_new_native(builtin_crypto_md5));
    vss_env_define(env, "__crypto_sha256", vss_value_new_native(builtin_crypto_sha256));

    // Network
    vss_env_define(env, "__network_resolve", vss_value_new_native(builtin_network_resolve));
    vss_env_define(env, "__network_ping", vss_value_new_native(builtin_network_ping));
}
