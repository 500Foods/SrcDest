// agentc.c
//
// 2024-Mar-18

#define AGENT_VERSION    "1.0.0"
#define AGENT_DB_VERSION "1.0.0"
#define XXH_STATIC_SEED  1234567

// Standard C libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

// Non-standard C libraries
#include <libwebsockets.h>
//#include <libwebsockets/lws-logs.h>
#include <cJSON.h>
#include <sqlite3.h>
#include <xxhash.h>
#include <libfswatch/c/libfswatch.h>
#include <libfswatch/c/cevent.h>

// Global vars 
volatile sig_atomic_t running = 1;
sqlite3 *db;
char launch_timestamp[20];
char *db_path;
int ws_port;

// Global types

// FSWatch structure
typedef struct {
    FSW_HANDLE handle;
    int fileset_id;
    char *filesetname;
    char *filesetroot;
    char *db_path;
} MonitorSession;

// Function prototypes
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
void handle_status_request(struct lws *wsi, const char *db_path);
int initialize_websocket_server(struct lws_context **context, int port);
void cleanup_websocket_server(struct lws_context *context);
void custom_log_callback(int level, const char *line); 

void *monitor_thread(void *arg);
void signal_handler(int signum);
void refresh_fileset(int fileset_id, const char *filesetroot);
void fswatch_callback(fsw_cevent const *const events, const unsigned int event_num, void *data);
int create_database();
int check_database_version(const char *expected_version);
int insert_default_fileset(const char *cwd, time_t now);
int load_configuration(const char *config_file, char **json_str, cJSON **root, char **db_path, int *ws_port);
void update_file_state(int fileset_id, const char *filename, const char *state);
void insert_file_record(int fileset_id, const char *filename, const char *state);
const char *get_file_state(int fileset_id, const char *filename, sqlite3_int64 *mtime);
XXH64_hash_t calculate_file_hash(const char *path);
void format_hash_string(XXH64_hash_t hash, char *hash_str);
void print_summary(int fileset_id, int fileCount, int matchedCount, int addedCount, int updatedCount, int missingCount);

// WebSocket protocol structure
struct lws_protocols protocols[] = {
    {
        "agent-protocol",
        ws_callback,
        0,
        0,
    },
    { NULL, NULL, 0, 0 } // Terminator
};

// WebSocket callback function
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    printf("wscll: Websocket callback initiated\n");
    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            return 0;
            break;
        case LWS_CALLBACK_ESTABLISHED:
            printf("wsest: Connection established\n");
            // New WebSocket connection established
            // Handle HTTP request, including WebSocket handshake
            //if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI) == 1 &&
                //strcmp(lws_hdr_simple_ptr(wsi, WSI_TOKEN_GET_URI), "/") == 0) {
                // Upgrade to WebSocket connection
                //lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_HOST);
                //return lws_http_to_websocket(wsi, buf, protocols);
            //}
            return 0;
            break;
        case LWS_CALLBACK_RECEIVE:
            // Received a message from the client
            // Parse the JSON message and handle the request
            cJSON *request = cJSON_Parse(in);
            if (request == NULL) {
                // Invalid JSON request
                cJSON *error_response = cJSON_CreateObject();
                cJSON_AddStringToObject(error_response, "status", "error");
                cJSON_AddStringToObject(error_response, "message", "Invalid JSON request");
                char *error_response_str = cJSON_PrintUnformatted(error_response);
                lws_write(wsi, error_response_str, strlen(error_response_str), LWS_WRITE_TEXT);
                cJSON_Delete(error_response);
                //NO! cJSON_free(error_response_str);
            } else {
                char *type = cJSON_GetObjectItemCaseSensitive(request, "type")->valuestring;
                if (strcmp(type, "status") == 0) {
                    handle_status_request(wsi, db_path);
                } else {
                    // Unknown request type
                    cJSON *error_response = cJSON_CreateObject();
                    cJSON_AddStringToObject(error_response, "status", "error");
                    cJSON_AddStringToObject(error_response, "message", "Unknown request type");
                    char *error_response_str = cJSON_PrintUnformatted(error_response);
                    lws_write(wsi, error_response_str, strlen(error_response_str), LWS_WRITE_TEXT);
                    cJSON_Delete(error_response);
                    //NO! cJSON_free(error_response_str);
                }
                cJSON_Delete(request);
            }
            return 0;
            break;
        // Add more cases for other callback reasons as needed
    }
    return 0;
}


// Function to handle status request
void handle_status_request(struct lws *wsi, const char *db_path) {
    // Retrieve the necessary information
    printf("Serve: Status request\n");

    // Construct the JSON response
    cJSON *response = cJSON_CreateObject();
    if (response == NULL) {
        printf("Error: Failed to create JSON response object\n");
        return;
    }

    cJSON_AddStringToObject(response, "status", "success");
    cJSON_AddStringToObject(response, "launchTime", launch_timestamp);
    cJSON_AddStringToObject(response, "agentVersion", AGENT_VERSION);
    cJSON_AddStringToObject(response, "databaseVersion", AGENT_DB_VERSION);

    if (db_path != NULL) {
        cJSON_AddStringToObject(response, "databaseFilename", db_path);
    }

    // Convert JSON to string
    char *response_str = NULL;
    response_str = cJSON_PrintUnformatted(response);
    if (response_str == NULL) {
        printf("Error: Failed to convert JSON response to string\n");
        cJSON_Delete(response);
        return;
    }

    // Send the response back to the client
    lws_write(wsi, response_str, strlen(response_str), LWS_WRITE_TEXT);

    // Cleanup
    cJSON_Delete(response);
    // This is apparently handled by cJSON internally?
    // cJSON_free(response_str);
}


// Function to initialize WebSocket server
int initialize_websocket_server(struct lws_context **context, int port) {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    *context = lws_create_context(&info);
    if (!*context) {
        printf("Error creating WebSocket context\n");
        return 1;
    }
    return 0;
}


// Function to cleanup WebSocket server
void cleanup_websocket_server(struct lws_context *context) {
    lws_context_destroy(context);
}

void custom_log_callback(int level, const char *line) {
    // Capture and process the log message here
    // You can format the message, write it to a file, or decide to display it

    // Example: Print the message to stderr
    printf("%s", line);
}

// Launch fswatch thread
void *monitor_thread(void *arg) {
    MonitorSession *session = (MonitorSession *)arg;
    fsw_start_monitor(session->handle);
    return NULL;
}


// Deal with Ctrl+C
void signal_handler(int signum) {
    if (signum == SIGINT) {
        running = 0;
    }
}


// Update the Fileset in the database with what is currently in the filesystem
void refresh_fileset(int fileset_id, const char *filesetroot) {
    sqlite3_stmt *insert_stmt;
    sqlite3_stmt *select_stmt;
    int rc;

    // Prepare the SQL statement to insert or update file data
    rc = sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO FILEDATA (FILESET, FILENAME, FILETYPE, FILESIZE, FILEHASH, FILEMODIFIED, FILECREATED, FILESTATE) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", -1, &insert_stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Prepare the SQL statement to select file data
    rc = sqlite3_prepare_v2(db, "SELECT FILEMODIFIED FROM FILEDATA WHERE FILESET = ? AND FILENAME = ?", -1, &select_stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(insert_stmt);
        return;
    }

    // Initialize counters
    int fileCount = 0;
    int matchedCount = 0;
    int addedCount = 0;
    int updatedCount = 0;

    // Recursive function to process files and directories
    void process_entry(const char *path) {
        struct stat st;
        if (stat(path, &st) == 0) {
            sqlite3_bind_int(insert_stmt, 1, fileset_id);
            sqlite3_bind_text(insert_stmt, 2, path + strlen(filesetroot) + 1, -1, SQLITE_STATIC);

            if (S_ISDIR(st.st_mode)) {
                sqlite3_bind_text(insert_stmt, 3, "directory", -1, SQLITE_STATIC);
                sqlite3_bind_int(insert_stmt, 4, 0);
                sqlite3_bind_null(insert_stmt, 5);
            } else if (S_ISREG(st.st_mode)) {
                sqlite3_bind_text(insert_stmt, 3, "file", -1, SQLITE_STATIC);
                sqlite3_bind_int64(insert_stmt, 4, st.st_size);

                XXH64_hash_t hash = calculate_file_hash(path);
                char hash_str[17];
                format_hash_string(hash, hash_str);
                sqlite3_bind_text(insert_stmt, 5, hash_str, -1, SQLITE_STATIC);
            }

            sqlite3_bind_int64(insert_stmt, 6, st.st_mtime);
            sqlite3_bind_int64(insert_stmt, 7, st.st_ctime);

            sqlite3_int64 db_mtime;
            const char *state = get_file_state(fileset_id, path + strlen(filesetroot) + 1, &db_mtime);

            if (state) {
                if (strcmp(state, "deleted") == 0) {
                    sqlite3_bind_text(insert_stmt, 8, "created", -1, SQLITE_STATIC);
                    addedCount++;
                } else if (st.st_mtime != db_mtime) {
                    sqlite3_bind_text(insert_stmt, 8, "updated", -1, SQLITE_STATIC);
                    updatedCount++;
                } else {
                    sqlite3_bind_text(insert_stmt, 8, "unchanged", -1, SQLITE_STATIC);
                    matchedCount++;
                }
            } else {
                sqlite3_bind_text(insert_stmt, 8, "created", -1, SQLITE_STATIC);
                addedCount++;
            }

            rc = sqlite3_step(insert_stmt);
            if (rc != SQLITE_DONE) {
                printf("Error executing SQL statement: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_reset(insert_stmt);
            fileCount++;

            if (S_ISDIR(st.st_mode)) {
                DIR *dir = opendir(path);
                if (dir) {
                    struct dirent *entry;
                    while ((entry = readdir(dir)) != NULL) {
                        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                            char subpath[PATH_MAX];
                            snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
                            process_entry(subpath);
                        }
                    }
                    closedir(dir);
                }
            }
        }
    }

    // Start processing from the fileset root directory
    process_entry(filesetroot);

    sqlite3_finalize(insert_stmt);
    sqlite3_finalize(select_stmt);

    // Check for missing files in the database
    int missingCount = 0;
    rc = sqlite3_prepare_v2(db, "SELECT FILENAME FROM FILEDATA WHERE FILESET = ? AND FILESTATE != 'deleted'", -1, &select_stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(select_stmt, 1, fileset_id);
        while (sqlite3_step(select_stmt) == SQLITE_ROW) {
            const char *filename = (const char *)sqlite3_column_text(select_stmt, 0);
            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", filesetroot, filename);
            if (access(fullpath, F_OK) != 0) {
                update_file_state(fileset_id, filename, "deleted");
                missingCount++;
            }
        }
        sqlite3_finalize(select_stmt);
    }

    print_summary(fileset_id, fileCount, matchedCount, addedCount, updatedCount, missingCount);
}

// When fswatch detects a change, this is called so we can decide 
// what do do with it - typically log it into the database
void fswatch_callback(fsw_cevent const *const events, const unsigned int event_num, void *data) {
    MonitorSession *session = (MonitorSession *)data;
    int fileset_id = session->fileset_id;
    char *db_path = session->db_path;

    for (unsigned int i = 0; i < event_num; ++i) {
        const char *path = events[i].path;
        const char *filename = path + strlen(session->filesetroot) + 1;

        if (strncmp(filename, db_path, strlen(db_path)) == 0) {
            continue;
        }

        for (unsigned int j = 0; j < events[i].flags_num; ++j) {
            enum fsw_event_flag flag = events[i].flags[j];

            if (flag & Created) {
                printf("%5d: C %s\n", fileset_id, path);
                sqlite3_int64 db_mtime;
                const char *state = get_file_state(fileset_id, filename, &db_mtime);
                if (state && strcmp(state, "deleted") == 0) {
                    update_file_state(fileset_id, filename, "created");
                } else {
                    insert_file_record(fileset_id, filename, "created");
                }
            } else if (flag & Updated) {
                printf("%5d: U %s\n", fileset_id, path);
                sqlite3_int64 db_mtime;
                const char *state = get_file_state(fileset_id, filename, &db_mtime);
                if (state && strcmp(state, "deleted") != 0) {
                    update_file_state(fileset_id, filename, "updated");
                }
            } else if (flag & Removed) {
                printf("%5d: D %s\n", fileset_id, path);
                sqlite3_int64 db_mtime;
                const char *state = get_file_state(fileset_id, filename, &db_mtime);
                if (state && strcmp(state, "deleted") != 0) {
                    update_file_state(fileset_id, filename, "deleted");
                }
            }
        }
    }
}

// Create database if it didn't already exist
int create_database() {
    char *sql; // SQL to execute
    int rc;    // Return code from execution

    // Create INFO table
    sql = "CREATE TABLE IF NOT EXISTS INFO (KEY TEXT PRIMARY KEY, VALUE TEXT)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Error: Create INFO table failed: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Create FILEDATA table
    sql = "CREATE TABLE IF NOT EXISTS FILEDATA ("
          "FILESET INTEGER, "
          "FILENAME TEXT, "
          "FILETYPE TEXT, "
          "FILESIZE INTEGER, "
          "FILEHASH TEXT, "
          "FILEMODIFIED INTEGER, "
          "FILECREATED INTEGER, "
          "FILESTATE TEXT, "
          "PRIMARY KEY (FILESET, FILENAME))";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Error: Create FILEDATA table failed: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Create FILESETS table
    sql = "CREATE TABLE IF NOT EXISTS FILESETS ("
          "FILESET INTEGER PRIMARY KEY, "
          "FILESETNAME TEXT, "
          "FILESETROOT TEXT, "
          "FILESETDATE INTEGER)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Error: Create FILESETS table failed: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}

// Check at the database version matches. 
int check_database_version(const char *expected_version) {
    int rc;              // Return code from execution
    char *db_version;    // Version reported
    sqlite3_stmt *stmt;  // SQL statement to process

    // Fetch version from database
    rc = sqlite3_prepare_v2(db, "SELECT VALUE FROM INFO WHERE KEY = 'db_version'", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error: Prepare SQL statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    rc = sqlite3_step(stmt);

    // Got a record 
    if (rc == SQLITE_ROW) {
        db_version = (char *)sqlite3_column_text(stmt, 0);
        if (strcmp(db_version, expected_version) != 0) {
            printf("Error: Database version mismatch. Expected %s, found %s\n", expected_version, db_version);
            sqlite3_finalize(stmt);
            return 1;
        } else {
            printf("Agent: Database version %s [CURRENT]\n", db_version);
        }

    // No record returned - generate a new one
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        rc = sqlite3_prepare_v2(db, "INSERT INTO INFO (KEY, VALUE) VALUES ('db_version', ?)", -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            printf("Error: Prepare SQL statement: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 1;
        }

        sqlite3_bind_text(stmt, 1, expected_version, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("Error inserting database version: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 1;
        }

        printf("Agent: Database version: %s [CURRENT]\n", expected_version);
        sqlite3_finalize(stmt);

    // Something unexpected
    } else {
        printf("Error: Execute SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 1;
    }

    return 0;
}

// Can't do much without a Fileset, so here one is created
// if none have been previously supplied
int insert_default_fileset(const char *cwd, time_t now) {
    int rc;
    int count;
    sqlite3_stmt *stmt;

    // Check if FILESETS table is empty
    rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM FILESETS", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_step(stmt);
    count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    // Insert the default file set only if the FILESETS table is empty
    if (count == 0) {
        printf("Agent: Initializing setting database defaults\n");
        rc = sqlite3_prepare_v2(db, "INSERT INTO FILESETS (FILESET, FILESETNAME, FILESETROOT, FILESETDATE) VALUES (?, ?, ?, ?)", -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            printf("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
            return 1;
        }

        sqlite3_bind_int(stmt, 1, 0);
        sqlite3_bind_text(stmt, 2, "Default Agent Fileset", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, cwd, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, (int64_t)now);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("Error inserting default file set: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 1;
        }

        sqlite3_finalize(stmt);
    }

    return 0;
}

// Run at startup
int load_configuration(const char *config_file, char **json_str, cJSON **root, char **db_path, int *ws_port) {
    FILE *fp = fopen(config_file, "r");

    // If configuration not found, create one out of thin air, load that, and then continue
    if (!fp) {
        printf("Configuration file '%s' not found. Creating default configuration.\n", config_file);

        // Create default configuration
        fp = fopen(config_file, "w");
        if (!fp) {
            printf("Error creating configuration file: %s\n", config_file);
            return 1;
        }

        fprintf(fp, "{\n    \"Agent Database\": \"agentc.sqlite\",\n    \"Agent Websocket\": 23432\n}\n");
        fclose(fp);

        fp = fopen(config_file, "r");
        if (!fp) {
            printf("Error opening configuration file: %s\n", config_file);
            return 1;
        }
    }

    printf("Agent: Loading configuration from '%s'\n", config_file);

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *json_str = malloc(fsize + 1);
    fread(*json_str, 1, fsize, fp);
    (*json_str)[fsize] = '\0';
    fclose(fp);

    printf("Agent: Parsing configuration\n");

    *root = cJSON_Parse(*json_str);
    if (!*root) {
        printf("Error parsing JSON configuration file\n");
        free(*json_str);
        return 1;
    }

    printf("Agent: Identifying database\n");

    if (!cJSON_GetObjectItem(*root, "Agent Database")) {
        printf("Error reading 'Agent Database' from configuration\n");
        cJSON_Delete(*root);
        free(*json_str);
        return 1;
    }

    *db_path = cJSON_GetObjectItemCaseSensitive(*root, "Agent Database")->valuestring;
    printf("Agent: Database identified as '%s'\n", *db_path);

    printf("Agent: Identifying WebSocket port\n");

    cJSON *ws_port_json = cJSON_GetObjectItem(*root, "Agent Websocket");
    if (!ws_port_json || !cJSON_IsNumber(ws_port_json)) {
        printf("Error reading 'Agent Websocket' from configuration\n");
        cJSON_Delete(*root);
        free(*json_str);
        return 1;
    }

    *ws_port = ws_port_json->valueint;
    printf("Agent: WebSocket port identified as %d\n", *ws_port);

    return 0;
}

void update_file_state(int fileset_id, const char *filename, const char *state) {
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, "UPDATE FILEDATA SET FILESTATE = ? WHERE FILESET = ? AND FILENAME = ?", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, state, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, fileset_id);
        sqlite3_bind_text(stmt, 3, filename, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void insert_file_record(int fileset_id, const char *filename, const char *state) {
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, "INSERT INTO FILEDATA (FILESET, FILENAME, FILESTATE) VALUES (?, ?, ?)", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, fileset_id);
        sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, state, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

const char *get_file_state(int fileset_id, const char *filename, sqlite3_int64 *mtime) {
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, "SELECT FILESTATE, FILEMODIFIED FROM FILEDATA WHERE FILESET = ? AND FILENAME = ?", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, fileset_id);
        sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *state = (const char *)sqlite3_column_text(stmt, 0);
            *mtime = sqlite3_column_int64(stmt, 1);
            sqlite3_finalize(stmt);
            return state;
        }
        sqlite3_finalize(stmt);
    }
    return NULL;
}

XXH64_hash_t calculate_file_hash(const char *path) {
    XXH64_hash_t hash = XXH64(NULL, 0, XXH_STATIC_SEED);
    FILE *file = fopen(path, "rb");
    if (file != NULL) {
        const size_t buffer_size = 4096;
        char buffer[buffer_size];
        size_t length;
        while ((length = fread(buffer, 1, buffer_size, file)) != 0) {
            hash = XXH64(buffer, length, hash);
        }
        fclose(file);
    }
    return hash;
}

void format_hash_string(XXH64_hash_t hash, char *hash_str) {
    snprintf(hash_str, 17, "%016llx", hash);
}

void print_summary(int fileset_id, int fileCount, int matchedCount, int addedCount, int updatedCount, int missingCount) {
    printf("%5d: Files: %d, Matched: %d, Added: %d, Updated: %d, Missing: %d\n",
           fileset_id, fileCount, matchedCount, addedCount, updatedCount, missingCount);
}

// Main program
int main(int argc, char *argv[]) {
    // Need a configuration file
    if (argc != 2) {
        printf("Agent Usage: %s <config_file.json>\n", argv[0]);
        return 1;
    }

    // Current timestamp
    time_t now = time(NULL);
    struct tm *now_tm = localtime(&now);
    strftime(launch_timestamp, sizeof(launch_timestamp), "%Y-%m-%d %H:%M:%S", now_tm);
    printf("Agent: v%s online at %s\n", AGENT_VERSION, launch_timestamp);

    // Set up signal handler for Ctrl+C
    signal(SIGINT, signal_handler);

    // Load configuration
    char *json_str;
    cJSON *root;
    char *db_path;
    if (load_configuration(argv[1], &json_str, &root, &db_path, &ws_port) != 0) {
        return 1;
    }

    // Open database
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        printf("Error opening SQLite database: %s\n", sqlite3_errmsg(db));
        cJSON_Delete(root);
        free(json_str);
        return 1;
    }

    // Create the database and tables
    if (create_database() != 0) {
        printf("Error creating SQLite database\n");
        cJSON_Delete(root);
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Check the database version
    if (check_database_version(AGENT_DB_VERSION) != 0) {
        printf("Error checking database version\n");
        cJSON_Delete(root);
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Use this for the default fileset
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("Error getting current working directory\n");
        cJSON_Delete(root);
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Insert the default file set only if the FILESETS table is empty
    if (insert_default_fileset(cwd, now) != 0) {
        sqlite3_close(db);
        cJSON_Delete(root);
        free(json_str);
        return 1;
    }

    // Load filesets
    printf("Agent: Loading filesets\n");

    MonitorSession *sessions = NULL;
    int session_count = 0;

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, "SELECT FILESET, FILESETNAME, FILESETROOT FROM FILESETS", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        cJSON_Delete(root);
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    printf("Agent: Initializing monitors\n");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int fileset_id = sqlite3_column_int(stmt, 0);
        const char *filesetname = (const char *)sqlite3_column_text(stmt, 1);
        const char *filesetroot = (const char *)sqlite3_column_text(stmt, 2);

        FSW_HANDLE handle = fsw_init_session(system_default_monitor_type);
        if (handle == NULL) {
            printf("Error initializing fswatch session for fileset %d\n", fileset_id);
            continue;
        }

        fsw_set_recursive(handle, true);

        MonitorSession session = {handle, fileset_id, strdup(filesetname), strdup(filesetroot), strdup(db_path)};
        sessions = realloc(sessions, (session_count + 1) * sizeof(MonitorSession));
        sessions[session_count++] = session;

        fsw_set_callback(handle, fswatch_callback, &sessions[session_count - 1]);

        if (fsw_add_path(handle, filesetroot) != FSW_OK) {
            printf("Error adding path to fswatch session for fileset %d\n", fileset_id);
            fsw_destroy_session(handle);
            free(session.filesetname);
            free(session.filesetroot);
            free(session.db_path);
            session_count--;
        }
        printf("%5d: %s\n", fileset_id, strdup(filesetname));
        refresh_fileset(fileset_id, filesetroot);
    }
    sqlite3_finalize(stmt);

    // Start monitoring for each session
    for (int i = 0; i < session_count; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, monitor_thread, &sessions[i]);
        pthread_detach(thread);
    }

    // WebSocket server initialization
    printf("Agent: Initializing websocket interface\n",ws_port);

    struct lws_context_creation_info info;

    memset(&info, 0, sizeof(info));
    info.port = ws_port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    //lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, custom_log_callback);
    lws_set_log_level(LLL_ERR | LLL_WARN, custom_log_callback);

    struct lws_context *ws_context = NULL;
    if (initialize_websocket_server(&ws_context, ws_port) != 0) {
        // Cleanup and exit
        cJSON_Delete(root);
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Main loop
    printf("Agent: Waiting...\n");
    while (running) {
        lws_service(ws_context, 100);
        usleep(100);
    }

    // Clean up
    for (int i = 0; i < session_count; i++) {
        fsw_destroy_session(sessions[i].handle);
        free(sessions[i].filesetname);
        free(sessions[i].filesetroot);
        free(sessions[i].db_path);
    }
    free(sessions);

    cJSON_Delete(root);
    free(json_str);
    sqlite3_close(db);
    cleanup_websocket_server(ws_context);

    return 0;
}
