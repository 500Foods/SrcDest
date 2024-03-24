// agentc.c

#define AGENT_VERSION       "AgentC v1.0.0"
#define AGENT_DB_VERSION    "AgentC DB v1.0.0"
#define AGENT_IP            "127.0.0.1"
#define AGENT_PORT          23432
#define AGENT_PROTOCOL      "agentc-protocol"
#define AGENT_APP           "AgentC"
#define AGENT_CLIENT        "CLI"
#define XXH_STATIC_SEED     1234567
#define MAX_MESSAGE_SIZE    65536
#define MAX_LOG_EVENT_SIZE  30
#define MAX_LOG_DETAIL_SIZE 100
#define SEPARATOR           "-------------------------\n"
#define DEBUG_WS            0  

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
#include <arpa/inet.h>

// Non-standard C libraries
#include <libwebsockets.h>
#include <jansson.h>
#include <sqlite3.h>
#include <xxhash.h>
#include <libfswatch/c/libfswatch.h>
#include <libfswatch/c/cevent.h>



// Global vars 

volatile sig_atomic_t running = 1;
sqlite3 *db;
char launch_timestamp[20];
char *agent_name;
char *database_name;
int ws_port = AGENT_PORT;
int ws_connections;
int ws_connections_total;
int ws_requests;
static char message_buffer[MAX_MESSAGE_SIZE];
static size_t message_length = 0;



// Function prototypes
void handle_events_request(struct lws *wsi, const char *app, const char *client, const char *event, const char *fileset, int age, const char *request_ip, const char *request_app, const char *request_client) ; 
void LogEvent(const char *ipaddress, const char *app, const char *client, const char *event, const char *details, long duration, int fileset); 
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
void handle_status_request(struct lws *wsi, const char *request_ip, const char *request_app, const char *request_client);
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
int load_configuration(const char *config_file, char **json_str, json_t **root);
void update_file_state(int fileset_id, const char *filename, const char *state);
void insert_file_record(int fileset_id, const char *filename, const char *state);
const char *get_file_state(int fileset_id, const char *filename, sqlite3_int64 *mtime);
XXH64_hash_t calculate_file_hash(const char *path);
void format_hash_string(XXH64_hash_t hash, char *hash_str);
void print_summary(int fileset_id, int fileCount, int matchedCount, int addedCount, int updatedCount, int missingCount);


// Global types

// FSWatch structure
typedef struct {
    FSW_HANDLE handle;
    int fileset_id;
    char *filesetname;
    char *filesetroot;
} MonitorSession;

// WebSocket protocol structure
// WebSocket connection state to track
typedef struct _ws_session_data {
    char x_forwarded_for[256]; 
} ws_session_data;

static struct lws_protocols protocols[] =
{
    {
        .name                  = AGENT_PROTOCOL,           // Protocol name
        .callback              = ws_callback,              // Protocol callback 
        .per_session_data_size = sizeof(ws_session_data),  // Protocol callback 'userdata' size 
        .rx_buffer_size        = 0,                        // Receve buffer size (0 = no restriction) 
        .id                    = 0,                        // Protocol Id (version) (optional) 
        .user                  = NULL,                     // 'User data' ptr, to access in 'protocol callback 
        .tx_packet_size        = 0                         // Transmission buffer size restriction (0 = no restriction) 
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }                       // Terminator
};



// Function to handle events request
void handle_events_request(struct lws *wsi, const char *app, const char *client, const char *event, const char *fileset, int age, const char *request_ip, const char *request_app, const char *request_client) {

    // Logging setup
    char eventtype[MAX_LOG_EVENT_SIZE] = "Events";
    char details[MAX_LOG_DETAIL_SIZE] = "";
    struct timeval start, stop;
    int msecs = 0;
    gettimeofday(&start, NULL);
    strcat(details, AGENT_VERSION);
    strcat(details, " / ");
    strcat(details, AGENT_DB_VERSION);

    sqlite3_stmt *stmt;

    // Setup query with optional values
    char sql[256] = "SELECT * FROM EVENTS WHERE 1=1";
    if (app)     { strcat(sql, " AND APP = ?"     ); }
    if (client)  { strcat(sql, " AND CLIENT = ?"  ); }
    if (event)   { strcat(sql, " AND EVENT = ?"   ); }
    if (fileset) { strcat(sql, " AND FILESET = ?" ); }
    strcat(sql, " AND LOGSTAMP > datetime('now', -?||' seconds') ORDER BY LOGSTAMP DESC");

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Populate with what we know
    int param_index = 1;
    if (app)     { sqlite3_bind_text(stmt, param_index++, app, -1, SQLITE_STATIC); }
    if (client)  { sqlite3_bind_text(stmt, param_index++, client, -1, SQLITE_STATIC); }
    if (event)   { sqlite3_bind_text(stmt, param_index++, event, -1, SQLITE_STATIC); }
    if (fileset) { sqlite3_bind_text(stmt, param_index++, fileset, -1, SQLITE_STATIC); }
    sqlite3_bind_int(stmt, param_index, age);

    json_t *events = json_array();
    int count = 0;

    // Output what we've found
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json_t *event = json_object();
        json_object_set_new(event, "logstamp",  json_string((const char *)sqlite3_column_text(stmt, 0)));
        json_object_set_new(event, "ipaddress", json_string((const char *)sqlite3_column_text(stmt, 1)));
        json_object_set_new(event, "app",       json_string((const char *)sqlite3_column_text(stmt, 2)));
        json_object_set_new(event, "client",    json_string((const char *)sqlite3_column_text(stmt, 3)));
        json_object_set_new(event, "event",     json_string((const char *)sqlite3_column_text(stmt, 4)));
        json_object_set_new(event, "details",   json_string((const char *)sqlite3_column_text(stmt, 5)));
        json_object_set_new(event, "duration",  json_integer(sqlite3_column_int64(stmt, 6)));
        json_object_set_new(event, "fileset",   json_integer(sqlite3_column_int(stmt, 7)));
        json_array_append_new(events, event);
        count++;
    }

    sqlite3_finalize(stmt);

    // Construct the JSON response
    json_t *response = json_object();
    json_object_set_new(response, "status", json_string("success"));
    json_object_set_new(response, "count", json_integer(count));
    json_object_set_new(response, "events", events);

    // Send the JSON response
    char *response_str = json_dumps(response, JSON_COMPACT);
    size_t len = strlen(response_str);
    unsigned char *buf = malloc(LWS_PRE + len);
    memcpy(buf + LWS_PRE, response_str, len);
    lws_write(wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);

    // Cleanup the JSON response
    free(buf);
    free(response_str);
    json_decref(response);

    // Log the event
    gettimeofday(&stop, NULL);
    msecs = (int)((double)(stop.tv_usec - start.tv_usec) / 1000 + (double)(stop.tv_sec - start.tv_sec));
    LogEvent(request_ip, request_app, request_client, eventtype, details, msecs, -1);
}



// Function to log events
void LogEvent(const char *ipaddress, const char *app, const char *client, const char *event, const char *details, long duration, int fileset) {

    sqlite3_stmt *stmt;

    char sql[] = "INSERT INTO EVENTS (LOGSTAMP, IPADDRESS, APP, CLIENT, EVENT, DETAILS, DURATION, FILESET) VALUES (datetime('now'), ?, ?, ?, ?, ?, ?, ?)";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_text(stmt, 1, ipaddress, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, app, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, client, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, event, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, details, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, duration);
    sqlite3_bind_int(stmt, 7, fileset);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Error executing SQL statement: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}



// WebSocket callback function
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

    switch (reason) {

        case LWS_CALLBACK_RECEIVE: // 6

	        // Global requests counter
            ws_requests++;
    
	        // Gather up the entire message and only the message
            if (message_length + len > MAX_MESSAGE_SIZE) {
                // Handle error: message too large
                printf("Error: Message too large\n");
                message_length = 0; // Reset buffer
                return -1;
            }
            memcpy(message_buffer + message_length, in, len);
            message_length += len;
            if (!lws_is_final_fragment(wsi)) {
                // Wait for more fragments
                return 0;
            }

            // Process the complete message 
            json_t *request = json_loadb(message_buffer, message_length, 0, NULL);
            message_length = 0; // Reset buffer for the next message

            // Get connection info 
            socklen_t len;
            struct sockaddr_storage addr;
            char request_ip[INET6_ADDRSTRLEN];
            int fd, request_port;
            len = sizeof(addr);
            fd = lws_get_socket_fd(wsi);
            getpeername(fd, (struct sockaddr*)&addr, &len);

            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in*)&addr;
                request_port = ntohs(s->sin_port);
                inet_ntop(AF_INET, &s->sin_addr, request_ip, INET6_ADDRSTRLEN);
            } else { // AF_INET6
                struct sockaddr_in6 *s = (struct sockaddr_in6*)&addr;
                request_port = ntohs(s->sin6_port);
                inet_ntop(AF_INET6, &s->sin6_addr, request_ip, INET6_ADDRSTRLEN);
            }

            // printf("Peer IP address: %s\n", request_ip);
            // printf("Peer port      : %d\n", request_port);
            // printf("libws: Websocket callback (%d-Receive) from %s\n", reason, request_ip);
	    
            ws_session_data* data = (ws_session_data*)lws_wsi_user(wsi);
            if (data) {
	            if (strlen(data->x_forwarded_for) > 0) {
	                strcat(request_ip, " -> ");
	                strcat(request_ip, data->x_forwarded_for);
		        }
            }


            if (request == NULL) {
                // Invalid JSON request
                printf("Agent: Invalid JSON request: %.*s\n", (int)len, (char *)in);
                json_t *response = json_object();
                json_object_set_new(response, "status", json_string("error"));
                json_object_set_new(response, "message", json_string("Invalid JSON request"));
    
                // Send the JSON response
                char *response_str = json_dumps(response, JSON_COMPACT);
                size_t len = strlen(response_str);
                unsigned char *buf = malloc(LWS_PRE + len);
                memcpy(buf + LWS_PRE, response_str, len);
                lws_write(wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);

                // Cleanup the JSON response
                free(buf);
                free(response_str);
                json_decref(response);

            } else if (!json_is_object(request)) {
                // Invalid JSON request (not an object)
                printf("Agent: Invalid JSON request (not an object)\n");
                json_t *response = json_object();
                json_object_set_new(response, "status", json_string("error"));
                json_object_set_new(response, "message", json_string("Invalid JSON request (not an object)"));
    
                // Send the JSON response
                char *response_str = json_dumps(response, JSON_COMPACT);
                size_t len = strlen(response_str);
                unsigned char *buf = malloc(LWS_PRE + len);
                memcpy(buf + LWS_PRE, response_str, len);
                lws_write(wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);

                // Cleanup the JSON response
                free(buf);
                free(response_str);
                json_decref(response);

            } else {
                // Valid JSON - let's check the type of request
                const char *type = json_string_value(json_object_get(request, "type"));
                const char *request_key    = json_string_value(json_object_get(request, "agentKey"));
                const char *request_app    = json_string_value(json_object_get(request, "agentApp"));
                const char *request_client = json_string_value(json_object_get(request, "agentClient"));

		        // Check if it is a valid request - Need agentKey, agentApp, and agentClient

		        // Request is authorized, so let's figure out what the request is for
                if (type && strcmp(type, "status") == 0) {
                    // Status - return basic system information mostly for testing
                    printf("Agent: Status request from [ %s ] %s / %s\n", request_ip, request_app, request_client);
                    handle_status_request(wsi, request_ip, request_app, request_client);

                } else if (type && strcmp(type, "events") == 0) {
                    // Events - return event log information
                    printf("Agent: Events request from [ %s ] %s / %s\n", request_ip, request_app, request_client);
                    const char *app     = json_string_value(json_object_get(request, "app"));
                    const char *client  = json_string_value(json_object_get(request, "client"));
                    const char *event   = json_string_value(json_object_get(request, "event"));
                    const char *fileset = json_string_value(json_object_get(request, "fileset"));
                    json_t *age_json = json_object_get(request, "age");
                    int age = age_json ? json_integer_value(age_json) : 600;
                    handle_events_request(wsi, app, client, event, fileset, age, request_ip, request_app, request_client);

                } else {
                    printf("Agent: Unknown request\n");
                    // Unknown request type
                    json_t *response = json_object();
                    json_object_set_new(response, "status", json_string("error"));
                    json_object_set_new(response, "message", json_string("Unknown request type"));
    
                    // Send the JSON response
                    char *response_str = json_dumps(response, JSON_COMPACT);
                    size_t len = strlen(response_str);
                    unsigned char *buf = malloc(LWS_PRE + len);
                    memcpy(buf + LWS_PRE, response_str, len);
                    lws_write(wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);

                    // Cleanup the JSON response
                    free(buf);
                    free(response_str);
                    json_decref(response);
                }
            }

            // Cleanup
            json_decref(request);
            return 0;

        case LWS_CALLBACK_ESTABLISHED: // 0
            uint8_t buf[LWS_PRE + 256];
            int val = lws_hdr_copy(wsi, (char *)buf, sizeof(buf), WSI_TOKEN_X_FORWARDED_FOR);
            if (val > 0) {
                ws_session_data* data = (ws_session_data*)lws_wsi_user(wsi);
                if (data) {
                    strncpy(data->x_forwarded_for, (char*)buf, sizeof(data->x_forwarded_for) - 1);
                    data->x_forwarded_for[sizeof(data->x_forwarded_for) - 1] = '\0'; // Ensure null-termination
                }
            }

	        if (DEBUG_WS) {
              printf("libws: Websocket callback (%d-Connection)\n", reason);
              printf("Agent: Connection established from: (len %d) '%s'\n", (int)strlen((const char *)buf),buf);
	        }
            return 0;
            break;

        case LWS_CALLBACK_CLOSED: // 4
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-Closed)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_CLOSED_HTTP: // 5
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-ClosedHTTP)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_RECEIVE_PONG: // 7
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-ReceivePong)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE: // 11
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-ServerWriteable)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_HTTP: // 12
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-HTTP)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_FILTER_NETWORK_CONNECTION: // 17
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-FilterNetworkConnection)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_FILTER_HTTP_CONNECTION: // 18
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-FilterHTTPConnection)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED: // 19
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-ServerNewClientInstantiated)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION: // 20 
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-FilterProtocolConnection)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_PROTOCOL_INIT: // 27
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-ProtocolInit)\n", reason);
	        }
	        // Initialize spot for data to be retained
	        ws_session_data* setupdata = (
                ws_session_data*)lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                lws_get_protocol(wsi), 
		        sizeof(ws_session_data)
	        );
            return 0;
            break;

        case LWS_CALLBACK_PROTOCOL_DESTROY: // 28
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-ProtocolDestroy)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_WSI_CREATE: // 29
            ws_connections++;
            ws_connections_total++;
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-WSICreate)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_WSI_DESTROY: // 30
            ws_connections--;
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-WSIDestroy)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_GET_THREAD_ID: // 31
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-GetThreadID)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_ADD_POLL_FD: // 32
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-AddPollFD)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_DEL_POLL_FD: // 33
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-DelPollFD)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_CHANGE_MODE_POLL_FD: // 34
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-ChangeModePollFD)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_LOCK_POLL: // 35	
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-GetLockPoll)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_UNLOCK_POLL: // 36	
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-GetLockPoll)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE: // 38
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-WSPeerInitiatedClose)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_HTTP_BIND_PROTOCOL: // 49
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-HTTPBindProtocol)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_ADD_HEADERS: // 53
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-AddHeaders)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_EVENT_WAIT_CANCELLED: // 71
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-EventWaitCancelled)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL: // 78
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-EventWaitCancelled)\n", reason);
	        }
            return 0;
            break;

        case LWS_CALLBACK_HTTP_CONFIRM_UPGRADE: // 86
	        if (DEBUG_WS) {
                printf("libws: Websocket callback (%d-HTTPConfirmUpgrade)\n", reason);
	        }
            return 0;
            break;

        default:
            printf("libws: Websocket callback (%d-Other)\n", reason);
    }
}



// Function to handle status request
void handle_status_request(struct lws *wsi, const char *request_ip, const char *request_app, const char *request_client) {

    // Logging setup
    char eventtype[MAX_LOG_EVENT_SIZE] = "Status";
    char details[MAX_LOG_DETAIL_SIZE] = "";
    struct timeval start, stop;
    int msecs = 0;
    gettimeofday(&start, NULL);
    strcat(details, AGENT_VERSION);
    strcat(details, " / ");
    strcat(details, AGENT_DB_VERSION);

    // Construct the JSON response
    json_t *response = json_object();
    json_object_set_new(response, "status", json_string("success"));
    json_object_set_new(response, "server", json_string(agent_name));
    json_object_set_new(response, "launchTime", json_string(launch_timestamp));
    json_object_set_new(response, "agentVersion", json_string(AGENT_VERSION));
    json_object_set_new(response, "databaseVersion", json_string(AGENT_DB_VERSION));
    json_object_set_new(response, "databaseFilename", json_string(database_name));
    json_object_set_new(response, "activeConnections", json_integer(ws_connections));
    json_object_set_new(response, "totalConnections", json_integer(ws_connections_total));
    json_object_set_new(response, "requests", json_integer(ws_requests));

    // Send the JSON response
    char *response_str = json_dumps(response, JSON_COMPACT);
    size_t len = strlen(response_str);
    unsigned char *buf = malloc(LWS_PRE + len);
    memcpy(buf + LWS_PRE, response_str, len);
    lws_write(wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);

    // Cleanup the JSON response
    free(buf);
    free(response_str);
    json_decref(response);

    // Log the event
    gettimeofday(&stop, NULL);
    msecs = (int)((double)(stop.tv_usec - start.tv_usec) / 1000 + (double)(stop.tv_sec - start.tv_sec));
    LogEvent(request_ip, request_app, request_client, eventtype, details, msecs, -1);
}
    


int initialize_websocket_server(struct lws_context **context, int port) {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = 0;

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
    char* hash_str = (char*)malloc(17); 

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

    free(hash_str);
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

    for (unsigned int i = 0; i < event_num; ++i) {
        const char *path = events[i].path;
        const char *filename = path + strlen(session->filesetroot) + 1;

        if (strncmp(filename, database_name, strlen(database_name)) == 0) {
            continue;
        }

        for (unsigned int j = 0; j < events[i].flags_num; ++j) {
            enum fsw_event_flag flag = events[i].flags[j];

            if (flag & Created) {
                printf("%5d: C %s\n", fileset_id, path);
                LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "File Created", path, 0, fileset_id);
                sqlite3_int64 db_mtime;
                const char *state = get_file_state(fileset_id, filename, &db_mtime);
                if (state && strcmp(state, "deleted") == 0) {
                    update_file_state(fileset_id, filename, "created");

                } else {
                    insert_file_record(fileset_id, filename, "created");
                }
            } else if (flag & Updated) {
                printf("%5d: U %s\n", fileset_id, path);
                LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "File Updated", path, 0, fileset_id);
                sqlite3_int64 db_mtime;
                const char *state = get_file_state(fileset_id, filename, &db_mtime);
                if (state && strcmp(state, "deleted") != 0) {
                    update_file_state(fileset_id, filename, "updated");
                }
            } else if (flag & Removed) {
                printf("%5d: D %s\n", fileset_id, path);
                LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "File Deleted", path, 0, fileset_id);
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

    // Create EVENTS table
    sql = "CREATE TABLE IF NOT EXISTS EVENTS ("
          "LOGSTAMP TEXT, "
          "IPADDRESS TEXT, "
          "APP TEXT, "
          "CLIENT TEXT, "
          "EVENT TEXT, "
          "DETAILS TEXT, "
          "DURATION INTEGER, "
          "FILESET INTEGER)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Error: Create EVENTS table failed: %s\n", sqlite3_errmsg(db));
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
	printf("Agent: Database created/updated\n");
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

        printf("Agent: Database version is %s [Current]\n", expected_version);
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
        printf("Agent: Creating default fileset\n");
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
int load_configuration(const char *config_file, char **json_str, json_t **root) {
    FILE *fp = fopen(config_file, "r");

    // If configuration not found, create one out of thin air, load that, and then continue
    if (!fp) {
        printf("Configuration file '%s' not found.\n", config_file);
	return 1;
    }

    // Load configuration file
    printf("Agent: Using JSON configuration: '%s'\n", config_file);
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Get JSON
    printf("Agent: Parsing JSON configuration\n");

    *json_str = malloc(fsize + 1);
    fread(*json_str, 1, fsize, fp);
    (*json_str)[fsize] = '\0';
    fclose(fp);

    json_error_t error;
    *root = json_loads(*json_str, 0, &error);
    if (!*root) {
        printf("Error parsing JSON configuration: %s\n", error.text);
        free(*json_str);
        return 1;
    }

    // Start with Agent Server
    printf("Agent: Configuring server\n");
    json_t *server_json = json_object_get(*root, "Agent Server");
    if (!server_json) {
        printf("Error reading 'Agent Server' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }

    // Agent Server -> Name 
    // Global: agent_name
    printf("Agent: Configuring name\n");
    json_t *agent_name_json = json_object_get(server_json, "Name");
    // Not fatal
    if (!agent_name_json || !json_is_string(agent_name_json)) {
        printf("Error reading 'Agent Server/Name' from configuration\n");
        agent_name = strdup(AGENT_APP);
    } else {
        agent_name = strdup(json_string_value(agent_name_json));
    }

    // Agent Server -> Database
    // Global: database_name
    printf("Agent: Configuring database\n");
    json_t *database_name_json = json_object_get(server_json, "Database");
    if (!database_name_json || !json_is_string(database_name_json)) {
        printf("Error reading 'Agent Server/Database' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    database_name = strdup(json_string_value(database_name_json));

    // Agent Server -> Port
    // Global: ws_port
    printf("Agent: Configuring network\n");
    json_t *ws_port_json = json_object_get(server_json, "Port");
    if (!ws_port_json || !json_is_integer(ws_port_json)) {
        printf("Error reading 'Agent Server/Port' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    ws_port = json_integer_value(ws_port_json);

    printf(SEPARATOR);

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
    snprintf(hash_str, 17, "%016lx", hash);
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
    printf("%sAgent: %s online at %s\n%s", SEPARATOR, AGENT_VERSION, launch_timestamp, SEPARATOR);

    // Set up signal handler for Ctrl+C
    signal(SIGINT, signal_handler);

    // Load configuration
    json_t *root;
    char *json_str;
    if (load_configuration(argv[1], &json_str, &root) != 0) {
        return 1;
    }

    // Open database
    printf("Agent: Using dataase: '%s'\n", database_name);
    int rc = sqlite3_open(database_name, &db);
    if (rc != SQLITE_OK) {
        printf("Error opening SQLite database: %s\n", sqlite3_errmsg(db));
        free(json_str);
        return 1;
    }

    // Create the database and tables
    if (create_database() != 0) {
        printf("Error creating SQLite database\n");
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "Agent Started", AGENT_VERSION, 0, 0);

    // Check the database version
    if (check_database_version(AGENT_DB_VERSION) != 0) {
        printf("Error checking database version\n");
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Use this for the default fileset
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("Error getting current working directory\n");
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Insert the default file set only if the FILESETS table is empty
    if (insert_default_fileset(cwd, now) != 0) {
        sqlite3_close(db);
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
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    printf("%sAgent: Initializing fileset monitors\n", SEPARATOR);

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

        MonitorSession session = {handle, fileset_id, strdup(filesetname), strdup(filesetroot)};
        sessions = realloc(sessions, (session_count + 1) * sizeof(MonitorSession));
        sessions[session_count++] = session;

        fsw_set_callback(handle, fswatch_callback, &sessions[session_count - 1]);

        if (fsw_add_path(handle, filesetroot) != FSW_OK) {
            printf("Error adding path to fswatch session for fileset %d\n", fileset_id);
            fsw_destroy_session(handle);
            free(session.filesetname);
            free(session.filesetroot);
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
    printf("%sAgent: Initializing websocket interface\n", SEPARATOR);
    printf("Agent: Using websocket name: %s\n", agent_name);
    printf("Agent: Using websocket port: %d\n", ws_port);
    printf("Agent: Using websocket protocol: %s\n", AGENT_PROTOCOL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = ws_port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = 0;

    if (DEBUG_WS) {
        lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, custom_log_callback);
    } else {
        lws_set_log_level(LLL_ERR | LLL_WARN, custom_log_callback);
    }

    struct lws_context *ws_context = NULL;
    if (initialize_websocket_server(&ws_context, ws_port) != 0) {
        // Cleanup and exit
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Main loop
    printf("%sAgent: Waiting...\n%s", SEPARATOR, SEPARATOR);

    while (running) {
        lws_service(ws_context, 100);
        usleep(100);
    }

    // Clean up
    LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "Agent Stopped", AGENT_VERSION, 0, 0);

    for (int i = 0; i < session_count; i++) {
        fsw_destroy_session(sessions[i].handle);
        free(sessions[i].filesetname);
        free(sessions[i].filesetroot);
    }
    free(sessions);
    free(json_str);
    json_decref(root);
    sqlite3_close(db);
    cleanup_websocket_server(ws_context);

    return 0;
}


