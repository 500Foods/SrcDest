// agentc.c

// AGENT constants
#define AGENT_AUTHOR        "Andrew Simard"
#define AGENT_VERSION       "AgentC v1.0.0"
#define AGENT_DB_VERSION    "AgentC DB v1.0.0"
#define AGENT_IP            "127.0.0.1"
#define AGENT_PORT          23432
#define AGENT_PROTOCOL      "agentc-protocol"
#define AGENT_APP           "AgentC"
#define AGENT_CLIENT        "CLI"
#define SEPARATOR           "-------------------------\n"
#define MAX_MESSAGE_SIZE    65536
#define MAX_LOG_SIZE        10000
#define MAX_LOG_EVENT_SIZE  30
#define MAX_LOG_DETAIL_SIZE 100
#define MAX_REST_SERVICES   10
#define DEBUG_WS            0  
#define XXH_STATIC_SEED     1234567

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

// Third-party C libraries 
#include <curl/curl.h>                // For sending e-mails, REST proxy
#include <libwebsockets.h>            // For websockets support
#include <jansson.h>                  // For JSON handling
#include <sqlite3.h>                  // For sqlite database support
#include <xxhash.h>                   // For generating file hashes
#include <libfswatch/c/libfswatch.h>  // For monitoring OS file changes



// Global variabless 

char log_buffer[MAX_LOG_SIZE]; 
char launch_timestamp[20];
volatile sig_atomic_t running = 1;

sqlite3 *db;
char *db_path;

int ws_port = AGENT_PORT;
int ws_connections;
int ws_connections_total;
int ws_requests;

CURLM *curlm;
static char message_buffer[MAX_MESSAGE_SIZE];
static size_t message_length = 0;

char *smtp_host; 
int   smtp_port; 
char *smtp_user;
char *smtp_pass;
char *smtp_from;
char *smtp_name;
char *smtp_mail; 



// Storage for the REST API proxy data loaded from Swagger
// Multiple services, each with multiple endoints
typedef struct {
    char *path;
    char *method;
    char **parameters;
    int num_parameters;
    char *security;
} endpoint_t;

typedef struct {
    char *name;
    char *url;
    endpoint_t *endpoints;
    int num_endpoints;
} service_t;

service_t *services = NULL;
int num_services = 0;



// This is used to simplify logging function calls by
// storing data between callbacks
typedef struct {
    struct timeval start;
    char eventtype[MAX_LOG_EVENT_SIZE];
    char details[MAX_LOG_DETAIL_SIZE];
} LogInfo;



// This is used to store data about a pending REST proxy request
struct rest_callback_data {
    struct lws *wsi;
    LogInfo log_info;
    int timeout;
    CURL *curl;
};



// Function prototypes
void log_message(const char *format, ...); 
void send_email(const char *subject, const char *to_list); 
LogInfo LogInitialize(const char *eventtype);
void LogFinalize(struct lws *wsi, LogInfo log_info);
void LogEvent(const char *ipaddress, const char *app, const char *client, const char *event, const char *details, long duration, int fileset); 
void send_error_response(struct lws *wsi, const char *message); 

size_t rest_response_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
int transfer_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow); 

void handle_status_request(struct lws *wsi);
void handle_events_request(struct lws *wsi, const char *app, const char *client, const char *event, const char *fileset, int age);
void handle_rest_proxy_request(struct lws *wsi, service_t *service, endpoint_t *endpoint, json_t *parameters, int timeout);

int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
void send_error_response(struct lws *wsi, const char *message); 
int initialize_websocket_server(struct lws_context **context, int port);
void cleanup_websocket_server(struct lws_context *context);
void websocket_log_callback(int level, const char *line); 

void *monitor_thread(void *arg);
void signal_handler(int signum);

void refresh_fileset(int fileset_id, const char *filesetroot);
void fswatch_callback(fsw_cevent const *const events, const unsigned int event_num, void *data);
int create_database();
int check_database_version(const char *expected_version);
int insert_default_fileset(const char *cwd, time_t now);
int load_configuration(const char *config_file, char **json_str, json_t **root, char **db_path, int *ws_port);
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
    char *db_path;
} MonitorSession;

// WebSocket protocol structure
// WebSocket connection state to track
typedef struct _ws_session_data {
    char x_forwarded_for[256] ; 
    char *jwt_tokens[MAX_REST_SERVICES]; 
    char request_ip[50];
    char request_app[50];
    char request_client[50];
} ws_session_data;

static struct lws_protocols protocols[] =
{
    {
        .name                  = AGENT_PROTOCOL,     // Protocol name
        .callback              = ws_callback,        // Protocol callback 
        .per_session_data_size = sizeof(ws_session_data),  // Protocol callback 'userdata' size 
        .rx_buffer_size        = 0,                  // Receve buffer size (0 = no restriction) 
        .id                    = 0,                  // Protocol Id (version) (optional) 
        .user                  = NULL,               // 'User data' ptr, to access in 'protocol callback 
        .tx_packet_size        = 0                   // Transmission buffer size restriction (0 = no restriction) 
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }                 // Terminator
};




void log_message(const char *format, ...) {
    va_list args, args_copy;
    va_start(args, format);
    va_copy(args_copy, args);

    // Print the message to the console
    vprintf(format, args);

    // Append the message to the in-memory logfile
    char message[1024];
    vsnprintf(message, sizeof(message), format, args_copy);

    // Check if the message fits within the available space in the log_buffer
    size_t remaining_space = sizeof(log_buffer) - strlen(log_buffer);
    if (strlen(message) < remaining_space) {
        strcat(log_buffer, message);
    } else {
        // Truncate the message to fit within the available space
        strncat(log_buffer, message, remaining_space - 1);
    }

    va_end(args);
    va_end(args_copy);
}
struct upload_status {
  size_t bytes_read;
  const char *payload_text;
};

static size_t payload_source(char *ptr, size_t size, size_t nmemb, void *userp)
{
  struct upload_status *upload_ctx = (struct upload_status *)userp;
  const char *data;
  size_t room = size * nmemb;

  if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1)) {
    return 0;
  }

  data = &upload_ctx->payload_text[upload_ctx->bytes_read];

  if (data) {
    size_t len = strlen(data);
    if (room < len)
      len = room;
    memcpy(ptr, data, len);
    upload_ctx->bytes_read += len;

    return len;
  }

  return 0;
}

void send_email(const char *subject, const char *to_list)
{
  CURL *curl;
  CURLcode res = CURLE_OK;
  struct curl_slist *recipients = NULL;
  struct upload_status upload_ctx = { 0 };

  // Compose the email body
  char email_body[MAX_LOG_SIZE + 1024];
  snprintf(email_body, sizeof(email_body),
           "Date: Thu, 28 Mar 2024 21:54:29 +1100\r\n"
           "To: %s\r\n"
           "From: %s\r\n"
           "Subject: %s\r\n"
           "\r\n"
           "%s\r\n",
           to_list, smtp_from, subject, log_buffer);

  upload_ctx.payload_text = email_body;

  curl = curl_easy_init();
  if (curl) {
    // Set the SMTP URL
    char smtp_url[256];
    snprintf(smtp_url, sizeof(smtp_url), "smtp://%s:%d", smtp_host, smtp_port);
    curl_easy_setopt(curl, CURLOPT_URL, smtp_url);

    // Set the SMTP username and password
    curl_easy_setopt(curl, CURLOPT_USERNAME, smtp_user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp_pass);

    // Set the email sender and recipients
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, smtp_from);
    recipients = curl_slist_append(recipients, to_list);
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    // Set the email body using the payload source callback
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    // Enable verbose output for debugging
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    // Send the email
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    // Clean up
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
  }
}
















LogInfo LogInitialize(const char *eventtype) {
    LogInfo log_info;
    gettimeofday(&log_info.start, NULL);
    strncpy(log_info.eventtype, eventtype, sizeof(log_info.eventtype));
    strncpy(log_info.details, "", sizeof(log_info.details));
    strcat(log_info.details, AGENT_VERSION);
    strcat(log_info.details, " / ");
    strcat(log_info.details, AGENT_DB_VERSION);
    // Add any other common initialization logic here
    return log_info;
}

void LogFinalize(struct lws *wsi, LogInfo log_info) {
    struct timeval stop;
    int msecs = 0;
    gettimeofday(&stop, NULL);
    msecs = (int)((double)(stop.tv_usec - log_info.start.tv_usec) / 1000 + (double)(stop.tv_sec - log_info.start.tv_sec));

    ws_session_data *session_data = (ws_session_data *)lws_wsi_user(wsi);
    const char *request_ip = session_data ? session_data->request_ip : "";
    const char *request_app = session_data ? session_data->request_app : "";
    const char *request_client = session_data ? session_data->request_client : "";

    LogEvent(request_ip, request_app, request_client, log_info.eventtype, log_info.details, msecs, -1);
}

// Send an error response back to the websockets client
void send_error_response(struct lws *wsi, const char *message) {
    json_t *response = json_object();
    json_object_set_new(response, "status", json_string("error"));
    json_object_set_new(response, "message", json_string(message));

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



size_t rest_response_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {

    struct rest_callback_data *data = (struct rest_callback_data *)userdata;
    size_t data_size = size * nmemb;
    struct lws *wsi = (struct lws *)userdata;

    // Allocate memory for the response data
    char *response_data = malloc(data_size + 1);
    if (response_data == NULL) {
        log_message("Failed to allocate memory for response data\n");
        return 0;
    }
 
    // Copy the response data and null-terminate it
    memcpy(response_data, ptr, data_size);
    response_data[data_size] = '\0';
 
    // Get the content type of the response
    char *content_type = NULL;
    curl_easy_getinfo(data->curl, CURLINFO_CONTENT_TYPE, &content_type);
 
    json_t *response = json_object();
 
    if (content_type && strstr(content_type, "application/json") != NULL) {
        // Parse the response data as JSON
        json_error_t error;
        json_t *response_json = json_loads(response_data, 0, &error);
 
        if (response_json == NULL) {
            log_message("Failed to parse response data as JSON: %s\n", error.text);
            json_object_set_new(response, "status", json_string("error"));
            json_object_set_new(response, "message", json_string("Failed to parse response data as JSON"));
        } else {
            json_object_set_new(response, "status", json_string("success"));
            json_object_set_new(response, "data", response_json);
        }
    } else {
        // Treat the response data as a generic blob
        json_object_set_new(response, "status", json_string("success"));
        json_object_set_new(response, "data", json_string(response_data));
    }

    free(response_data);

    // Send the JSON response back to the WebSocket client
    char *response_str = json_dumps(response, JSON_COMPACT);
    size_t response_len = strlen(response_str);
    unsigned char *response_buf = malloc(LWS_PRE + response_len);
    memcpy(response_buf + LWS_PRE, response_str, response_len);
    lws_write(wsi, response_buf + LWS_PRE, response_len, LWS_WRITE_TEXT);

    // Clean up
    free(response_buf);
    free(response_str);
    json_decref(response);

    snprintf(data->log_info.details, sizeof(data->log_info.details), "Response received");
    LogFinalize(data->wsi, data->log_info);

    return data_size;
}

int transfer_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    struct rest_callback_data *data = (struct rest_callback_data *)clientp;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    long elapsed_time = (current_time.tv_sec - data->log_info.start.tv_sec) * 1000 +
                        (current_time.tv_usec - data->log_info.start.tv_usec) / 1000;

    if (elapsed_time > data->timeout) {
        log_message("Request timed out\n");
        snprintf(data->log_info.eventtype, sizeof(data->log_info.eventtype), "RestTimeout");
        snprintf(data->log_info.details, sizeof(data->log_info.details), "Request timed out");
        LogFinalize(data->wsi, data->log_info);
        return 1; // Abort the request
    }

    return 0; // Continue the request
}



void handle_rest_proxy_request(struct lws *wsi, service_t *service, endpoint_t *endpoint, json_t *parameters, int timeout) {
    // Logging setup
    LogInfo log_info = LogInitialize("Events");

    // Create a new CURL easy handle
    CURL *curl = curl_easy_init();
    if (curl) {
        // Set the request URL
        char url[256];
        snprintf(url, sizeof(url), "%s%s", service->url, endpoint->path);
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set the request method
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, endpoint->method);

        // Set the request timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

        // Set the request headers
        struct curl_slist *headers = NULL;
        ws_session_data *session_data = (ws_session_data *)lws_wsi_user(wsi);
        if (session_data) {
            // Find the index of the service in the services array
            int service_index = -1;
            for (size_t i = 0; i < num_services; i++) {
                if (strcmp(services[i].name, service->name) == 0) {
                    service_index = i;
                    break;
                }
            }
    
            if (service_index >= 0) {
                char *jwt = session_data->jwt_tokens[service_index];
                if (jwt) {
                    char auth_header[256];
                    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", jwt);
                    headers = curl_slist_append(headers, auth_header);
                }
            }
    
            char forwarded_header[512];
            if (session_data->x_forwarded_for[0] != '\0') {
                snprintf(forwarded_header, sizeof(forwarded_header), "X-Forwarded-For: %s, %s", session_data->x_forwarded_for, session_data->request_ip);
            } else {
                snprintf(forwarded_header, sizeof(forwarded_header), "X-Forwarded-For: %s", session_data->request_ip);
            }
            headers = curl_slist_append(headers, forwarded_header);
	}

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set the request parameters
        char *parameters_str = json_dumps(parameters, JSON_COMPACT);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, parameters_str);

        // Allocate memory for the callback data
	struct rest_callback_data *data = malloc(sizeof(struct rest_callback_data));
        data->wsi = wsi;
        data->log_info = LogInitialize("RestRequest");
        data->timeout = timeout;
        data->curl = curl; // Store the CURL easy handle in the callback data

        // Set the response callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, rest_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

        // Set the transfer callback for timeout handling
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, data);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        // Add the request to the CURLM instance
        curl_multi_add_handle(curlm, curl);

        // Add the request to the CURLM instance
        curl_multi_add_handle(curlm, curl);

        // Free the allocated memory
        free(parameters_str);
        curl_slist_free_all(headers);
    }
    
    // Add specific details to the 'details' string
    snprintf(log_info.details, sizeof(log_info.details), "Service: %s, Endpoint: %s", service-> name, endpoint->path);
    LogFinalize(wsi, log_info);
}



// Function to handle events request
void handle_events_request(struct lws *wsi, const char *app, const char *client, const char *event, const char *fileset, int age) {

    // Logging setup
    LogInfo log_info = LogInitialize("Events");

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
        log_message("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
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
    snprintf(log_info.details, sizeof(log_info.details), "Events returned: %d", count);
    LogFinalize(wsi, log_info);
}



// Function to log events
void LogEvent(const char *ipaddress, const char *app, const char *client, const char *event, const char *details, long duration, int fileset) {

    sqlite3_stmt *stmt;

    char sql[] = "INSERT INTO EVENTS (LOGSTAMP, IPADDRESS, APP, CLIENT, EVENT, DETAILS, DURATION, FILESET) VALUES (datetime('now'), ?, ?, ?, ?, ?, ?, ?)";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_message("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
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
        log_message("Error executing SQL statement: %s\n", sqlite3_errmsg(db));
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
                log_message("Error: Message too large\n");
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

            // log_message("Peer IP address: %s\n", request_ip);
            // log_message("Peer port      : %d\n", request_port);
            // log_message("libws: Websocket callback (%d-Receive) from %s\n", reason, request_ip);
	    
            ws_session_data* session_data = (ws_session_data*)lws_wsi_user(wsi);
            if (session_data) {
	         if (strlen(session_data->x_forwarded_for) > 0) {
	             strcat(request_ip, " -> ");
	             strcat(request_ip, session_data->x_forwarded_for);
		 }
            }


            if (request == NULL) {
                // Invalid JSON request
                log_message("Agent: Invalid JSON request: %.*s\n", (int)len, (char *)in);
		send_error_response(wsi, "Invalid JSON request (not an object?)");

            } else if (!json_is_object(request)) {
                // Invalid JSON request (not an object)
                log_message("Agent: Invalid JSON request (not an object)\n");
		send_error_response(wsi, "Invalid JSON request (not an object)");

            } else {
                // Valid JSON - let's check the type of request
                const char *type = json_string_value(json_object_get(request, "type"));
                const char *request_key    = json_string_value(json_object_get(request, "agentKey"));
                const char *request_app    = json_string_value(json_object_get(request, "agentApp"));
                const char *request_client = json_string_value(json_object_get(request, "agentClient"));

		// Store request information in the WebSocket user information
                strncpy(session_data->request_ip, request_ip, sizeof(session_data->request_ip));
                strncpy(session_data->request_app, request_app, sizeof(session_data->request_app));
                strncpy(session_data->request_client, request_client, sizeof(session_data->request_client));

		// Check if it is a valid request - Need agentKey, agentApp, and agentClient

		// Request is authorized, so let's figure out what the request is for
                if (type && strcmp(type, "status") == 0) {
                    // Status - return basic system information mostly for testing
                    log_message("Agent: Status request from [ %s ] %s / %s\n", request_ip, request_app, request_client);
                    handle_status_request(wsi);

                } else if (type && strcmp(type, "events") == 0) {
                    // Events - return event log information
                    log_message("Agent: Events request from [ %s ] %s / %s\n", request_ip, request_app, request_client);
                    const char *app     = json_string_value(json_object_get(request, "app"));
                    const char *client  = json_string_value(json_object_get(request, "client"));
                    const char *event   = json_string_value(json_object_get(request, "event"));
                    const char *fileset = json_string_value(json_object_get(request, "fileset"));
                    json_t *age_json = json_object_get(request, "age");
                    int age = age_json ? json_integer_value(age_json) : 600;
                    handle_events_request(wsi, app, client, event, fileset, age);

                } else if (type && strcmp(type, "rest") == 0) {
                    // REST API proxy request
                    const char *service = json_string_value(json_object_get(request, "service"));
                    const char *endpoint = json_string_value(json_object_get(request, "endpoint"));
                    json_t *parameters = json_object_get(request, "parameters");
        
                    if (service && endpoint && parameters && json_is_object(parameters)) {
                        // Find the matching service and endpoint in the loaded configuration
                        service_t *matched_service = NULL;
                        endpoint_t *matched_endpoint = NULL;
        
                        for (size_t i = 0; i < num_services; i++) {
                            if (strcmp(services[i].name, service) == 0) {
                                matched_service = &services[i];
                                for (size_t j = 0; j < matched_service->num_endpoints; j++) {
                                    if (strcmp(matched_service->endpoints[j].path, endpoint) == 0) {
                                        matched_endpoint = &matched_service->endpoints[j];
                                        break;
                                    }
                                }
                                break;
                            }
                        }
        
		        json_t *timeout_json = json_object_get(request, "timeout");
                        int timeout = timeout_json ? json_integer_value(timeout_json) : 60;

                        json_t *jwt_json = json_object_get(request, "jwt");
                        const char *jwt = jwt_json ? json_string_value(jwt_json) : NULL;

                        if (jwt) {
                            // Store the JWT token in the user data, associated with the service
                            ws_session_data *session_data = (ws_session_data *)lws_wsi_user(wsi);
                            if (session_data) {
                                int service_index = -1;
                                for (size_t i = 0; i < num_services; i++) {
                                    if (strcmp(services[i].name, service) == 0) {
                                        service_index = i;
                                        break;
                                    }
                                }
                                if (service_index >= 0) {
                                    free(session_data->jwt_tokens[service_index]);
                                    session_data->jwt_tokens[service_index] = strdup(jwt);
                                }
                            }
                        }

                        if (matched_service && matched_endpoint) {
                            // Service and endpoint matched, trigger the proxy request
                            handle_rest_proxy_request(wsi, matched_service, matched_endpoint, parameters, timeout);
                        } else {
                            // Service or endpoint not found
                            log_message("Agent: Service or endpoint not found for REST API proxy request\n");
                            send_error_response(wsi, "Service or endpoint not found");
                        }
                    } else {
                        // Invalid REST API proxy request
                        log_message("Agent: Invalid REST API proxy request (require service, endpoint, and parameters)\n");
                        send_error_response(wsi, "Invalid REST API proxy request");
                    }


                } else {
                    log_message("Agent: Unknown request type\n");
		    send_error_response(wsi, "Unknown request type");
                }
            }

            // Cleanup
            json_decref(request);
            return 0;

        case LWS_CALLBACK_ESTABLISHED: // 0
	    // Always want this initialized
            ws_session_data* data = (ws_session_data*)lws_wsi_user(wsi);

            uint8_t buf[LWS_PRE + 256];
            int val = lws_hdr_copy(wsi, (char *)buf, sizeof(buf), WSI_TOKEN_X_FORWARDED_FOR);
            if (val > 0) {
                if (data) {
                    strncpy(data->x_forwarded_for, (char*)buf, sizeof(data->x_forwarded_for) - 1);
                    data->x_forwarded_for[sizeof(data->x_forwarded_for) - 1] = '\0'; // Ensure null-termination
                }
            }

	    if (DEBUG_WS) {
              log_message("libws: Websocket callback (%d-Connection)\n", reason);
              log_message("Agent: Connection established from: (len %d) '%s'\n", (int)strlen((const char *)buf),buf);
	    }
            return 0;
            break;

        case LWS_CALLBACK_CLOSED: // 4
	    if (DEBUG_WS) {
               log_message("libws: Websocket callback (%d-Closed)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_CLOSED_HTTP: // 5
	    if (DEBUG_WS) {
               log_message("libws: Websocket callback (%d-ClosedHTTP)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_RECEIVE_PONG: // 7
	    if (DEBUG_WS) {
               log_message("libws: Websocket callback (%d-ReceivePong)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE: // 11
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-ServerWriteable)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_HTTP: // 12
	    //if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-HTTP)\n", reason);
	    //}


            return 0;
            break;

        case LWS_CALLBACK_FILTER_NETWORK_CONNECTION: // 17
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-FilterNetworkConnection)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_FILTER_HTTP_CONNECTION: // 18
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-FilterHTTPConnection)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED: // 19
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-ServerNewClientInstantiated)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION: // 20 
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-FilterProtocolConnection)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_PROTOCOL_INIT: // 27
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-ProtocolInit)\n", reason);
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
                log_message("libws: Websocket callback (%d-ProtocolDestroy)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_WSI_CREATE: // 29
            ws_connections++;
            ws_connections_total++;
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-WSICreate)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_WSI_DESTROY: // 30
            ws_connections--;
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-WSIDestroy)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_GET_THREAD_ID: // 31
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-GetThreadID)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_ADD_POLL_FD: // 32
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-AddPollFD)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_DEL_POLL_FD: // 33
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-DelPollFD)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_CHANGE_MODE_POLL_FD: // 34
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-ChangeModePollFD)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_LOCK_POLL: // 35	
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-GetLockPoll)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_UNLOCK_POLL: // 36	
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-GetLockPoll)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE: // 38
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-WSPeerInitiatedClose)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_HTTP_BIND_PROTOCOL: // 49
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-HTTPBindProtocol)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_ADD_HEADERS: // 53
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-AddHeaders)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_EVENT_WAIT_CANCELLED: // 71
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-EventWaitCancelled)\n", reason);
	    }
            return 0;
            break;

        case LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL: // 78
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-EventWaitCancelled)\n", reason);
	    }
	    log_message("ERROR: Connection dropped due to protocol mismatch\n");
            return 0;
            break;

        case LWS_CALLBACK_HTTP_CONFIRM_UPGRADE: // 86
	    if (DEBUG_WS) {
                log_message("libws: Websocket callback (%d-HTTPConfirmUpgrade)\n", reason);
	    }
            return 0;
            break;

        default:
            log_message("libws: Websocket callback (%d-Other)\n", reason);
    }
}



// Function to handle status request
void handle_status_request(struct lws *wsi) {

    // Initialize Logging
    LogInfo log_info = LogInitialize("Status");

    // Construct the JSON response
    json_t *response = json_object();
    json_object_set_new(response, "status", json_string("success"));
    json_object_set_new(response, "launchTime", json_string(launch_timestamp));
    json_object_set_new(response, "agentVersion", json_string(AGENT_VERSION));
    json_object_set_new(response, "databaseVersion", json_string(AGENT_DB_VERSION));
    json_object_set_new(response, "databaseFilename", json_string(db_path));
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
    snprintf(log_info.details, sizeof(log_info.details), "Active: %d, Total: %d", ws_connections, ws_connections_total);
    LogFinalize(wsi, log_info);
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
        log_message("Error creating WebSocket context\n");
        return 1;
    }
    return 0;
}


// Function to cleanup WebSocket server
void cleanup_websocket_server(struct lws_context *context) {
    lws_context_destroy(context);
}

 

// If websocket debugging is enabled, this will be called with each log entry. 
// Here, we just pass it over to log_message() to deal with
void websocket_log_callback(int level, const char *line) {
   log_message("%s", line);
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
        log_message("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Prepare the SQL statement to select file data
    rc = sqlite3_prepare_v2(db, "SELECT FILEMODIFIED FROM FILEDATA WHERE FILESET = ? AND FILENAME = ?", -1, &select_stmt, NULL);
    if (rc != SQLITE_OK) {
        log_message("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
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
                log_message("Error executing SQL statement: %s\n", sqlite3_errmsg(db));
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
                log_message("%5d: C %s\n", fileset_id, path);
                LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "File Created", path, 0, fileset_id);
                sqlite3_int64 db_mtime;
                const char *state = get_file_state(fileset_id, filename, &db_mtime);
                if (state && strcmp(state, "deleted") == 0) {
                    update_file_state(fileset_id, filename, "created");

                } else {
                    insert_file_record(fileset_id, filename, "created");
                }
            } else if (flag & Updated) {
                log_message("%5d: U %s\n", fileset_id, path);
                LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "File Updated", path, 0, fileset_id);
                sqlite3_int64 db_mtime;
                const char *state = get_file_state(fileset_id, filename, &db_mtime);
                if (state && strcmp(state, "deleted") != 0) {
                    update_file_state(fileset_id, filename, "updated");
                }
            } else if (flag & Removed) {
                log_message("%5d: D %s\n", fileset_id, path);
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
        log_message("Error: Create INFO table failed: %s\n", sqlite3_errmsg(db));
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
        log_message("Error: Create FILEDATA table failed: %s\n", sqlite3_errmsg(db));
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
        log_message("Error: Create FILESETS table failed: %s\n", sqlite3_errmsg(db));
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
        log_message("Error: Create EVENTS table failed: %s\n", sqlite3_errmsg(db));
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
        log_message("Error: Prepare SQL statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    rc = sqlite3_step(stmt);

    // Got a record 
    if (rc == SQLITE_ROW) {
        db_version = (char *)sqlite3_column_text(stmt, 0);
        if (strcmp(db_version, expected_version) != 0) {
            log_message("Error: Database version mismatch. Expected %s, found %s\n", expected_version, db_version);
            sqlite3_finalize(stmt);
            return 1;
        } else {
            log_message("Agent: Database version %s [CURRENT]\n", db_version);
        }

    // No record returned - generate a new one
    } else if (rc == SQLITE_DONE) {
	printf("Agent: Database created/updated\n");
        sqlite3_finalize(stmt);
        rc = sqlite3_prepare_v2(db, "INSERT INTO INFO (KEY, VALUE) VALUES ('db_version', ?)", -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            log_message("Error: Prepare SQL statement: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 1;
        }

        sqlite3_bind_text(stmt, 1, expected_version, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            log_message("Error inserting database version: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 1;
        }

        log_message("Agent: Database version is %s [Current]\n", expected_version);
        sqlite3_finalize(stmt);

    // Something unexpected
    } else {
        log_message("Error: Execute SQL statement: %s\n", sqlite3_errmsg(db));
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
        log_message("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_step(stmt);
    count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    // Insert the default file set only if the FILESETS table is empty
    if (count == 0) {
        log_message("Agent: Creating default fileset\n");
        rc = sqlite3_prepare_v2(db, "INSERT INTO FILESETS (FILESET, FILESETNAME, FILESETROOT, FILESETDATE) VALUES (?, ?, ?, ?)", -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            log_message("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
            return 1;
        }

        sqlite3_bind_int(stmt, 1, 0);
        sqlite3_bind_text(stmt, 2, "Default Agent Fileset", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, cwd, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, (int64_t)now);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            log_message("Error inserting default file set: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 1;
        }

        sqlite3_finalize(stmt);
    }

    return 0;
}

// Run at startup
int load_configuration(const char *config_file, char **json_str, json_t **root, char **db_path, int *ws_port) {
    FILE *fp = fopen(config_file, "r");

    // If configuration not found, create one out of thin air, load that, and then continue
    if (!fp) {
        log_message("Configuration file '%s' not found.\n", config_file);
	return 1;
    }

    // Load configuration file
    log_message("Agent: Using JSON configuration: '%s'\n", config_file);
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Get JSON
    log_message("Agent: Parsing JSON configuration\n");

    *json_str = malloc(fsize + 1);
    fread(*json_str, 1, fsize, fp);
    (*json_str)[fsize] = '\0';
    fclose(fp);

    json_error_t error;
    *root = json_loads(*json_str, 0, &error);
    if (!*root) {
        log_message("Error parsing JSON configuration: %s\n", error.text);
        free(*json_str);
        return 1;
    }

    // Start with Agent Server
    log_message("Agent: Configuring server\n");
    json_t *server_json = json_object_get(*root, "Agent Server");
    if (!server_json) {
        log_message("Error reading 'Agent Server' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }

    // Agent Server -> Database
    log_message("Agent: Configuring database\n");
    json_t *db_path_json = json_object_get(server_json, "Database");
    if (!db_path_json || !json_is_string(db_path_json)) {
        log_message("Error reading 'Agent Server/Database' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    *db_path = strdup(json_string_value(db_path_json));

    // Agent Server -> Port
    log_message("Agent: Configuring network\n");
    json_t *ws_port_json = json_object_get(server_json, "Port");
    if (!ws_port_json || !json_is_integer(ws_port_json)) {
        log_message("Error reading 'Agent Server/Port' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    *ws_port = json_integer_value(ws_port_json);

    // Mail Server
    log_message("Agent: Configuring mail server\n");
    json_t *mail_server_json = json_object_get(*root, "Mail Server");
    if (!mail_server_json || !json_is_object(mail_server_json)) {
        log_message("Error reading 'Mail Server' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }

    // Mail Server -> SMTP Host
    json_t *smtp_host_json = json_object_get(mail_server_json, "SMTP Host");
    if (!smtp_host_json || !json_is_string(smtp_host_json)) {
        log_message("Error reading 'Mail Server/SMTP Host' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    smtp_host = strdup(json_string_value(smtp_host_json));

    // Mail Server -> SMTP Port
    json_t *smtp_port_json = json_object_get(mail_server_json, "SMTP Port");
    if (!smtp_port_json || !json_is_integer(smtp_port_json)) {
        log_message("Error reading 'Mail Server/SMTP Port' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    smtp_port = json_integer_value(smtp_port_json);

    // Mail Server -> SMTP User
    json_t *smtp_user_json = json_object_get(mail_server_json, "SMTP User");
    if (!smtp_user_json || !json_is_string(smtp_user_json)) {
        log_message("Error reading 'Mail Server/SMTP User' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    smtp_user = strdup(json_string_value(smtp_user_json));

    // Mail Server -> SMTP Pass
    json_t *smtp_pass_json = json_object_get(mail_server_json, "SMTP Pass");
    if (!smtp_pass_json || !json_is_string(smtp_pass_json)) {
        log_message("Error reading 'Mail Server/SMTP Pass' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    smtp_pass = strdup(json_string_value(smtp_pass_json));

    // Mail Server -> SMTP From
    json_t *smtp_from_json = json_object_get(mail_server_json, "SMTP From");
    if (!smtp_from_json || !json_is_string(smtp_from_json)) {
        log_message("Error reading 'Mail Server/SMTP From' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    smtp_from = strdup(json_string_value(smtp_from_json));

    // Mail Server -> SMTP Name
    json_t *smtp_name_json = json_object_get(mail_server_json, "SMTP Name");
    if (!smtp_name_json || !json_is_string(smtp_name_json)) {
        log_message("Error reading 'Mail Server/SMTP Name' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }
    smtp_name = strdup(json_string_value(smtp_name_json));
    
    // Process REST Services
    log_message("Agent: Configuring REST services\n");
    json_t *rest_services_json = json_object_get(*root, "REST Services");
    if (!rest_services_json || !json_is_array(rest_services_json)) {
        log_message("Error reading 'REST Services' from configuration\n");
        json_decref(*root);
        free(*json_str);
        return 1;
    }

    size_t num_services = json_array_size(rest_services_json);
    log_message("Agent: Found %zu REST services\n", num_services);

    services = malloc(num_services * sizeof(service_t));

    for (size_t service_counter = 0; service_counter < num_services; service_counter++) {
        //log_message("Agent: Processing REST service %zu\n", service_counter);

        json_t *service_json = json_array_get(rest_services_json, service_counter);
        if (!service_json || !json_is_object(service_json)) {
            log_message("Error reading 'REST Services' item %zu from configuration\n", service_counter);
            json_decref(*root);
            free(*json_str);
            free(services);
            return 1;
        }

        // log_message("Agent: Service JSON: %s\n", json_dumps(service_json, JSON_INDENT(2)));
    
        const char *service_name = json_object_iter_key(json_object_iter(service_json));
        // log_message("Agent: Service name: %s\n", service_name);

        json_t *service_config = json_object_get(service_json, service_name);
        // log_message("Agent: Service config: %s\n", json_dumps(service_config, JSON_INDENT(2)));

        json_t *swagger_file_json = json_object_get(service_config, "Swagger");
        if (!swagger_file_json || !json_is_string(swagger_file_json)) {
            log_message("Error reading 'Swagger' for service '%s' from configuration\n", service_name);
            json_decref(*root);
            free(*json_str);
            free(services);
            return 1;
        }

        const char *swagger_file = json_string_value(swagger_file_json);
        // log_message("Agent: Loading Swagger data for '%s' from '%s'\n", service_name, swagger_file);

        // Load and parse Swagger file
        FILE *fp_swagger = fopen(swagger_file, "r");
        if (!fp_swagger) {
            log_message("Swagger file '%s' not found for service '%s'\n", swagger_file, service_name);
            json_decref(*root);
            free(*json_str);
            free(services);
            return 1;
        }

        fseek(fp_swagger, 0, SEEK_END);
        long swagger_fsize = ftell(fp_swagger);
        fseek(fp_swagger, 0, SEEK_SET);

        char *swagger_json_str = malloc(swagger_fsize + 1);
        fread(swagger_json_str, 1, swagger_fsize, fp_swagger);
        swagger_json_str[swagger_fsize] = '\0';
        fclose(fp_swagger);

        json_error_t swagger_error;
        json_t *swagger_root = json_loads(swagger_json_str, 0, &swagger_error);
        if (!swagger_root) {
            log_message("Error parsing Swagger file '%s' for service '%s': %s\n", swagger_file, service_name, swagger_error.text);
            free(swagger_json_str);
            json_decref(*root);
            free(*json_str);
            free(services);
            return 1;
        }

        // Extract relevant information from the Swagger JSON and store it for later use
        json_t *paths_json = json_object_get(swagger_root, "paths");
        if (paths_json && json_is_object(paths_json)) {
            size_t num_endpoints = json_object_size(paths_json);
            endpoint_t *endpoints = malloc(num_endpoints * sizeof(endpoint_t));
    
            const char *path;
            json_t *path_item;
            size_t endpoint_counter = 0;
            json_object_foreach(paths_json, path, path_item) {
                const char *method;
                json_t *operation;
                json_object_foreach(path_item, method, operation) {
                    endpoints[endpoint_counter].path = strdup(path);
                    endpoints[endpoint_counter].method = strdup(method);
    
                    // Extract parameters
                    json_t *parameters_json = json_object_get(operation, "parameters");
                    if (parameters_json && json_is_array(parameters_json)) {
                        size_t num_parameters = json_array_size(parameters_json);
                        endpoints[endpoint_counter].parameters = malloc(num_parameters * sizeof(char *));
                        endpoints[endpoint_counter].num_parameters = num_parameters;
    
                        for (size_t i = 0; i < num_parameters; i++) {
                            json_t *parameter_json = json_array_get(parameters_json, i);
                            json_t *name_json = json_object_get(parameter_json, "name");
                            if (name_json && json_is_string(name_json)) {
                                endpoints[endpoint_counter].parameters[i] = strdup(json_string_value(name_json));
                            }
                        }
                    }

                    // Extract security requirement
                    json_t *security_json = json_object_get(operation, "security");
                    if (security_json && json_is_array(security_json)) {
                        json_t *security_item = json_array_get(security_json, 0);
                        const char *security_name = json_object_iter_key(json_object_iter(security_item));
                        endpoints[endpoint_counter].security = strdup(security_name);
                    }
    
                    endpoint_counter++;
                }
            }
    
            // Update the services array using the current service_counter
            services[service_counter].name = strdup(service_name);
            services[service_counter].url = strdup(json_string_value(json_object_get(service_config, "URL")));
            services[service_counter].endpoints = endpoints;
            services[service_counter].num_endpoints = num_endpoints;

	    // Print summary of loaded endpoints and parameters for the current service
            size_t total_parameters = 0;
            for (size_t i = 0; i < num_endpoints; i++) {
                total_parameters += endpoints[i].num_parameters;
            }
            log_message("Agent: Loaded %zu calls, %zu params for '%s' from '%s'\n",
               num_endpoints, total_parameters, service_name, swagger_file);
            }
    
        free(swagger_json_str);
        json_decref(swagger_root);
    }

   log_message(SEPARATOR);

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
    const char *state = NULL;
    int rc = sqlite3_prepare_v2(db, "SELECT FILESTATE, FILEMODIFIED FROM FILEDATA WHERE FILESET = ? AND FILENAME = ?", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, fileset_id);
        sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *temp_state = (const char *)sqlite3_column_text(stmt, 0);
            if (temp_state) {
                // Make a copy of the string to return
                state = strdup(temp_state);
            }
            *mtime = sqlite3_column_int64(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
    return state; // Caller must free this
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
    log_message("%5d: Files: %d, Matched: %d, Added: %d, Updated: %d, Missing: %d\n",
           fileset_id, fileCount, matchedCount, addedCount, updatedCount, missingCount);
}

// Main program
int main(int argc, char *argv[]) {

    // Initialize log_buffer to empty
    memset(log_buffer, 0, sizeof(log_buffer));

    // Need a configuration file
    if (argc != 2) {
        log_message("Agent Usage: %s <config_file.json>\n", argv[0]);
        return 1;
    }

    // Current timestamp
    time_t now = time(NULL);
    struct tm *now_tm = localtime(&now);
    strftime(launch_timestamp, sizeof(launch_timestamp), "%Y-%m-%d %H:%M:%S", now_tm);
    log_message("%sAgent: %s online at %s\n%s", SEPARATOR, AGENT_VERSION, launch_timestamp, SEPARATOR);

    // Set up signal handler for Ctrl+C
    signal(SIGINT, signal_handler);

    // Load configuration
    json_t *root;
    char *json_str;
    if (load_configuration(argv[1], &json_str, &root, &db_path, &ws_port) != 0) {
        free(json_str);
        return 1;
    }

    // Open database
    log_message("Agent: Using dataase: '%s'\n", db_path);
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        log_message("Error opening SQLite database: %s\n", sqlite3_errmsg(db));
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Create the database and tables
    if (create_database() != 0) {
        log_message("Error creating SQLite database\n");
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Check the database version
    if (check_database_version(AGENT_DB_VERSION) != 0) {
        log_message("Error checking database version\n");
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Use this for the default fileset
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        log_message("Error getting current working directory\n");
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
    log_message("Agent: Loading filesets\n");
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, "SELECT FILESET, FILESETNAME, FILESETROOT FROM FILESETS", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_message("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // A separate FSWATCH session is created for each fileset3yy
    MonitorSession *fsw_sessions = NULL;
    int fsw_session_count = 0;
    log_message("%sAgent: Initializing fileset monitor(s)s:\n", SEPARATOR);

    // Loop through all the FILESET records returned
    while (sqlite3_step(stmt) == SQLITE_ROW) {

	// Get FILESET fields
        int fileset_id = sqlite3_column_int(stmt, 0);
        const char *filesetname = (const char *)sqlite3_column_text(stmt, 1);
        const char *filesetroot = (const char *)sqlite3_column_text(stmt, 2);

	// Create FSWATCH handle
        FSW_HANDLE handle = fsw_init_session(system_default_monitor_type);
        if (handle == NULL) {
            log_message("Error initializing fswatch session for fileset %d\n", fileset_id);
            continue;
        }

	// Monitoring all subdirectories
        fsw_set_recursive(handle, true);

	// Create FSWATCH session
        MonitorSession fsw_session = {handle, fileset_id, strdup(filesetname), strdup(filesetroot), strdup(db_path)};
        fsw_sessions = realloc(fsw_sessions, (fsw_session_count + 1) * sizeof(MonitorSession));
        fsw_sessions[fsw_session_count++] = fsw_session;

	// Set FSWATCH callback function
        fsw_set_callback(handle, fswatch_callback, &fsw_sessions[fsw_session_count - 1]);

	// Add directory to be monitored
        if (fsw_add_path(handle, filesetroot) != FSW_OK) {
            log_message("Error adding path to fswatch session for fileset %d\n", fileset_id);
            fsw_destroy_session(handle);
            free(fsw_session.filesetname);
            free(fsw_session.filesetroot);
            free(fsw_session.db_path);
            fsw_session_count--;
        }

	// Report on what has been configured
        log_message("%5d: %s\n", fileset_id, strdup(filesetname));

	// Update FILEDATA with the most current information
        refresh_fileset(fileset_id, filesetroot);

    }

    // Start monitoring for each session
    for (int i = 0; i < fsw_session_count; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, monitor_thread, &fsw_sessions[i]);
        pthread_detach(thread);
    }
    
    // All done with the query and with FSWATCH setup
    sqlite3_finalize(stmt);

    // WebSocket server initialization
    log_message("%sAgent: Initializing websocket interface\n", SEPARATOR);
    log_message("Agent: Using websocket protocol: %s\n", AGENT_PROTOCOL);
    log_message("Agent: Using websocket port: %d\n", ws_port);

    // Create websocket info
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = ws_port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = 0;

    // Debugging creates a lot of information. Manageable but entirely unnecessary
    // once things are up and running normally
    if (DEBUG_WS) {
        lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, websocket_log_callback);
    } else {
        lws_set_log_level(LLL_ERR | LLL_WARN, websocket_log_callback);
    }

    // Create websocket context
    struct lws_context *ws_context = NULL;
    if (initialize_websocket_server(&ws_context, ws_port) != 0) {
        free(json_str);
        sqlite3_close(db);
        return 1;
    }

    // Make a note in the database event log
    LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "Agent Started", AGENT_VERSION, 0, 0);

    // Send Notification EMail
    const char *subject = "Agent Server Startup Log";
    const char *to_list = "andrew@500foods.com";
    send_email(subject, to_list);

    // Main loop
    log_message("%sAgent: Waiting...\n%s", SEPARATOR, SEPARATOR);

    while (running) {
        lws_service(ws_context, 100);
        usleep(100);
    }

    // Clean up
    LogEvent(AGENT_IP, AGENT_APP, AGENT_CLIENT, "Agent Stopped", AGENT_VERSION, 0, 0);

    for (int i = 0; i < fsw_session_count; i++) {
        fsw_destroy_session(fsw_sessions[i].handle);
        free(fsw_sessions[i].filesetname);
        free(fsw_sessions[i].filesetroot);
        free(fsw_sessions[i].db_path);
    }
    free(fsw_sessions);
    free(json_str);
    json_decref(root);
    sqlite3_close(db);
    cleanup_websocket_server(ws_context);

    // Free allocated memory
    for (int i = 0; i < num_services; i++) {
        service_t *service = &services[i];
        free(service->name);
        free(service->url);

        for (int j = 0; j < service->num_endpoints; j++) {
            endpoint_t *endpoint = &service->endpoints[j];
            free(endpoint->path);
            free(endpoint->method);
            free(endpoint->security);

            for (int k = 0; k < endpoint->num_parameters; k++) {
                free(endpoint->parameters[k]);
            }
            free(endpoint->parameters);
        }
        free(service->endpoints);
    }
    free(services);

    return 0;
}


