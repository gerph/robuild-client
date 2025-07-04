#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "getopt.h"

#ifdef __riscos
#include "swis.h"
#endif

#include "VersionNum"


/* Enable this to output some debug messages */
//#define DEBUG


#define BUFSIZE_INPUT  (1024*3) /* Must be a multiple of 3 to keep to base64 boundaries */
#define BUFSIZE_BASE64 (BUFSIZE_INPUT / 3 * 4) /* Based on the input size */


/* The wsclient.h header appears to be intended for internal use, but it does define
 * some structures. It appears to omit the external API, however.
 */
#define errors libwsclienterrors
#define libwsclient_flags hidden_libwsclient_flags
#include <sys/types.h>
#include <netinet/in.h>
#include <wsclient.h>
int libwsclient_send(wsclient *client, char *strdata);
void libwsclient_onclose(wsclient *client, int (*cb)(wsclient *c));
void libwsclient_onopen(wsclient *client, int (*cb)(wsclient *c));
void libwsclient_onmessage(wsclient *client, int (*cb)(wsclient *c, wsclient_message *msg));
void libwsclient_onerror(wsclient *client, int (*cb)(wsclient *c, wsclient_error *err));
void libwsclient_close(wsclient *client);
#undef errors


#include "cJSON.h"

#include "base64.h"


typedef struct global_s {
    char *server_uri;
    char *source_file;
    char *output_prefix;
    char *build_output_file;
    FILE *build_output_fh;
    enum {
        s_connecting,
        s_options_negotiation,
        s_source_sent,
        s_build_sent,
        s_compiling,
        s_complete
    } state;
    int option_counter;
    int output_ends_in_newline;
    int last_message_was_output;
    int quiet;
    int really_quiet;
    int rc;

    /* Options */
    int timeout;
    int ansitext;
    char *arch;
} global_t;

global_t global;


/**
 * Protocol error.
 */
void protoerror(cJSON *json, char *msg)
{
    char *out;
    fprintf(stderr, "Protocol error: %s\n", msg);

    fprintf(stderr, "Protocol message:\n");
    out = cJSON_Print(json);
    fprintf(stderr, "%s\n", out);
    free(out);
    exit(1);
}

/**
 * General error.
 */
void error(char *msg)
{
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

int onopen(wsclient *c) {
#ifdef DEBUG
    fprintf(stderr, "onopen\n");
#endif
    return 0;
}

int onclose(wsclient *c) {
    if (global.state != s_complete)
    {
        fprintf(stderr, "Connection closed\n");
    }
    return 0;
}

int onerror(wsclient *c, wsclient_error *err) {
    fprintf(stderr, "ERROR: (%d): %s\n", err->code, err->str);
    if(err->extra_code) {
        errno = err->extra_code;
        perror("recv");
    }
    return 0;
}


/* Get a string from a JSON object */
#define GET_JSON_STRING(_json) \
        (((_json) && cJSON_IsString((_json)) && (_json)->valuestring != NULL) \
            ? (_json)->valuestring : NULL)

/* Get an int from a JSON object */
#define GET_JSON_INT(_json, _badvalue) \
        (((_json) && cJSON_IsNumber((_json)) && (_json)->valueint != INT_MAX && (_json)->valueint != INT_MIN) \
            ? (_json)->valueint : (_badvalue))

/* Get an object from a JSON object */
#define GET_JSON_MAP(_json) \
        (((_json) && cJSON_IsObject((_json))) \
            ? (_json) : NULL)

/* Get an array from a JSON object */
#define GET_JSON_ARRAY(_json) \
        (((_json) && cJSON_IsArray((_json))) \
            ? (_json) : NULL)

/* Get a string from a JSON map (requires a 'cJSON *_tmpjson', created by PREPARE_JSON_KEYED) */
#define PREPARE_JSON_KEYED cJSON *_tmpjson = NULL
#define GET_JSON_KEYED_STRING(_json, _key) \
        ((_json) && (_tmpjson=cJSON_GetObjectItemCaseSensitive((_json), (_key))) \
            ? GET_JSON_STRING(_tmpjson) : NULL)

/* Same for an int */
#define GET_JSON_KEYED_INT(_json, _key, _badvalue) \
        ((_json) && (_tmpjson=cJSON_GetObjectItemCaseSensitive((_json), (_key))) \
            ? GET_JSON_INT(_tmpjson, (_badvalue)) : (_badvalue))

/* Get an indexed item from an array */
#define GET_JSON_INDEXED(_json, _index) \
        ((_json) ? cJSON_GetArrayItem((_json), (_index)) : NULL)

/* Get an indexed item from a JSON array (requires a 'cJSON *_tmpjson', created by PREPARE_JSON_KEYED) */
#define PREPARE_JSON_INDEXED cJSON *_tmpjson = NULL
#define GET_JSON_INDEXED_STRING(_json, _index) \
        ((_json) && (_tmpjson=cJSON_GetArrayItem((_json), (_index))) \
            ? GET_JSON_STRING(_tmpjson) : NULL)

/* Same for an int */
#define GET_JSON_INDEXED_INT(_json, _index, _badvalue) \
        ((_json) && (_tmpjson=cJSON_GetArrayIndex((_json), (_index))) \
            ? GET_JSON_INT(_tmpjson, (_badvalue)) : (_badvalue))



/**
 * Send an action+json object combination.
 */
void robuild_send_json(wsclient *c, char *action, cJSON *payload)
{
    cJSON *json = cJSON_CreateArray();
    char *msg;
    cJSON_AddItemToArray(json, cJSON_CreateString(action));
    cJSON_AddItemToArray(json, payload);

    msg = cJSON_Print(json);
#ifdef DEBUG
    printf("Sending: %s\n", msg);
#endif
    libwsclient_send(c, msg);
    free(msg);
    cJSON_Delete(json);
}


/**
 * Send an action+string combination.
 */
void robuild_send_str(wsclient *c, char *action, char *str)
{
    robuild_send_json(c, action, cJSON_CreateString(str));
}


/**
 * Send the source message.
 */
void source_send(wsclient *c)
{
    char buffer[BUFSIZE_INPUT + 1];
    int base64size = 0;
    char *base64;
    FILE *fh;

    /* Now that we've been sent the welcome, we can send the source data */
    base64 = malloc(base64size + 1);
    fh = fopen(global.source_file, "rb");
    if (!fh)
    {
        error("Cannot read source input file");
    }
    *base64 = '\0';

    while (!feof(fh))
    {
        char *resized;
        int encoded;
        int read = fread(buffer, 1, BUFSIZE_INPUT, fh);
        if (read < 0)
        {
            perror("fread");
        }
        if (read == 0)
            break;

        resized = realloc(base64, base64size + BUFSIZE_BASE64 + 1);
        if (resized == NULL)
        {
            error("Not enough memory");
        }
        base64 = resized;
        encoded = base64_encode(buffer, read, &base64[base64size], BUFSIZE_BASE64 + 1);
        if (encoded == -1)
        {
            error("Base64 encode error");
        }
        base64size += encoded;
    }
    fclose(fh);

    robuild_send_str(c, "source", base64);
    free(base64);

    global.state = s_source_sent;
}

/************************** Options handling *******************************/

cJSON *option_timeout_build(void)
{
    if (global.timeout == -1)
        return NULL;
    return cJSON_CreateNumber(global.timeout);
}

void option_timeout_reply(cJSON *payload, int success)
{
    if (!success)
    {
        char *msg = GET_JSON_STRING(payload);
        printf("Warning: Timeout configuration failed: %s\n", msg ? msg : "Server error");
    }
}


cJSON *option_ansitext_build(void)
{
    if (global.ansitext == -1)
        return NULL;
    return cJSON_CreateBool(global.ansitext);
}


void option_ansitext_reply(cJSON *payload, int success)
{
    if (!success)
    {
        char *msg = GET_JSON_STRING(payload);
        printf("Warning: ANSI text configuration failed: %s\n", msg ? msg : "Server error");
    }
}


cJSON *option_arch_build(void)
{
    if (global.arch == NULL)
        return NULL;
    return cJSON_CreateString(global.arch);
}


void option_arch_reply(cJSON *payload, int success)
{
    if (!success)
    {
        char *msg = GET_JSON_STRING(payload);
        printf("Warning: Architecture configuration failed: %s\n", msg ? msg : "Server error");
    }
}

typedef struct option_handler_s {
    char *name;
    cJSON *(*build)(); /* Return NULL if no value for this option */
    void (*reply)(cJSON *payload, int success);
} option_handler_t;


option_handler_t option_handlers[] = {
    {
        "timeout",
        option_timeout_build,
        option_timeout_reply,
    },

    {
        "ansitext",
        option_ansitext_build,
        option_ansitext_reply,
    },

    {
        "arch",
        option_arch_build,
        option_arch_reply,
    },

    /* Terminating entry */
    {
        NULL,
    }
};


/**
 * Send the next options message.
 */
void options_next(wsclient *c)
{
    while (1)
    {
        cJSON *array;
        cJSON *name_str;
        cJSON *value;
        option_handler_t *handler = &option_handlers[global.option_counter];
        if (handler->name == NULL)
        {
            /* We've run out of options to send, so move on to the next stage - sending the source */
            break;
        }

        value = handler->build();
        if (value == NULL)
        {
            /* Nothing to do for this handler, move on */
            global.option_counter++;
            continue;
        }

        name_str = cJSON_CreateString(handler->name);
        array = cJSON_CreateArray();
        cJSON_AddItemToArray(array, name_str);
        cJSON_AddItemToArray(array, value);
        robuild_send_json(c, "option", array);
        return;
    }

    /* We're done; so move on to sending the source */
    source_send(c);
}


/**
 * Deal with a response to the option request.
 */
void options_reply(wsclient *c, cJSON *json, int success)
{
    option_handler_t *handler = &option_handlers[global.option_counter];

    handler->reply(json, success);

    global.option_counter++;
    options_next(c);
}


/**
 * API message: 'welcome' - we connected and they greeted.
 */
void robuild_msg_welcome(wsclient *c, cJSON *json) {
    char *msg = GET_JSON_STRING(json);
    if (!msg)
    {
        protoerror(json, "'welcome' message did not pass a string");
    }

    if (!global.quiet)
        printf("System: %s\n", msg);

    if (global.state != s_connecting)
    {
        protoerror(json, "'welcome' received out of sequence");
    }

    /* Begin the option negotiation */
    global.state = s_options_negotiation;
    options_next(c);
}


/**
 * API message: 'error' - The server reported an error
 */
void robuild_msg_error(wsclient *c, cJSON *json) {
    char *msg = GET_JSON_STRING(json);
    if (!msg)
    {
        protoerror(json, "'error' message did not pass a string");
    }

    if (global.state == s_options_negotiation)
    {
        /* Options negotiation may handle errors */
        options_reply(c, json, 0);
        return;
    }

    printf("Error from server: %s\n", msg);
    /* We immediately exit, because there's generally nothing we can do */
    exit(1);
}


/**
 * API message: 'response' - The server is responding to something that we did.
 */
void robuild_msg_response(wsclient *c, cJSON *json) {
    PREPARE_JSON_INDEXED;
    char *msg = GET_JSON_STRING(json);
    cJSON *json_data = NULL;
    if (!msg)
    {
        /* They didn't give us a string, so check if it's an array */
        cJSON *array = GET_JSON_ARRAY(json);
        if (array == NULL)
        {
            protoerror(json, "'response' message did not pass a string or an array of (string, data)");
        }
        msg = GET_JSON_INDEXED_STRING(array, 0);
        if (msg == NULL)
        {
            protoerror(json, "'response' message array item 0 was not a string");
        }
        json_data = GET_JSON_INDEXED(array, 1);
        if (json_data == NULL)
        {
            protoerror(json, "'response' message array item 1 was not present");
        }
    }

    if (global.state != s_options_negotiation)
    {
        /* Don't print the server messages during the options negotiation; leave it to the option handler. */
        if (!global.quiet)
            printf("Server: %s\n", msg);
    }

    if (global.state == s_source_sent)
    {
        robuild_send_str(c, "build", "");
        global.state = s_build_sent;
    }
    else if (global.state == s_build_sent)
    {
        global.state = s_compiling;
    }
    else if (global.state == s_options_negotiation)
    {
        options_reply(c, json, 1);
    }
    else
    {
        protoerror(json, "'response' received for unknown message");
    }
}


/**
 * API message: 'message' - The server is informing us of how the build is going.
 */
void robuild_msg_message(wsclient *c, cJSON *json) {
    char *msg = GET_JSON_STRING(json);
    if (!msg)
    {
        protoerror(json, "'message' message did not pass a string");
    }

    if (!global.quiet)
        printf("Build: %s\n", msg);

    if (global.state == s_compiling)
    {
        /* Nothing to do; it's just informational */
    }
    else
    {
        protoerror(json, "'message' received for unknown state");
    }
}


/**
 * API message: 'throwback' - The server is informing us of warning/error data
 */
void robuild_msg_throwback(wsclient *c, cJSON *json) {
    cJSON *tbjson = GET_JSON_MAP(json);
    if (!tbjson)
    {
        protoerror(json, "'throwback' message did not pass an object");
    }

    if (global.state == s_compiling)
    {
        PREPARE_JSON_KEYED;

        if (!global.quiet)
        {
            int reason = GET_JSON_KEYED_INT(tbjson, "reason", -1);
            char *reason_name = GET_JSON_KEYED_STRING(tbjson, "reason_name");
            int severity = GET_JSON_KEYED_INT(tbjson, "severity", -1);
            char *severity_name = GET_JSON_KEYED_STRING(tbjson, "severity_name");
            char *filename = GET_JSON_KEYED_STRING(tbjson, "filename");
            int lineno = GET_JSON_KEYED_INT(tbjson, "lineno", -1);
            char *url = GET_JSON_KEYED_STRING(tbjson, "url");
            char *message = GET_JSON_KEYED_STRING(tbjson, "message");

            printf("Throwback:\n");
            printf("  Reason:       %i (%s)\n", reason, reason_name ? reason_name : "<none>");
            if (severity != -1)
                printf("  Severity:     %i (%s)\n", severity, severity_name ? severity_name : "<none>");
            if (filename)
                printf("  Filename:     %s : %i\n", filename, lineno);
            if (message)
                printf("  Message:      %s\n", message);
        }
    }
    else
    {
        protoerror(json, "'throwback' received for unknown state");
    }
}


/**
 * API message: 'clipboard' - The server is informing us of a built file
 */
void robuild_msg_clipboard(wsclient *c, cJSON *json) {
    cJSON *cbjson = GET_JSON_MAP(json);
    if (!cbjson)
    {
        protoerror(json, "'clipboard' message did not pass an object");
    }

    if (global.state == s_compiling)
    {
        PREPARE_JSON_KEYED;

        int filetype = GET_JSON_KEYED_INT(cbjson, "filetype", -1);
        const char *base64 = GET_JSON_KEYED_STRING(cbjson, "data");
        int base64size = strlen(base64);

        char output[BUFSIZE_INPUT];
        FILE *fh;
        char output_filename[1024];

        if (!global.quiet)
            printf("Result file: filetype=&%03x, %lu bytes\n", filetype, strlen(base64) * 3 / 4);

#ifdef __riscos
        strcpy(output_filename, global.output_prefix);
#else
        sprintf(output_filename, "%s,%03x", global.output_prefix, filetype);
#endif
        fh = fopen(output_filename, "wb");
        if (!fh)
        {
            error("Could not write to output file");
        }
        while (base64size)
        {
            int got = base64_decode(&base64, &base64size, output, sizeof(output));
            fwrite(output, 1, got, fh);
        }
        fclose(fh);

#ifdef __riscos
        /* Set the file type */
        _swix(OS_File, _INR(0, 2), 18, output_filename, filetype);
#endif
    }
    else
    {
        protoerror(json, "'throwback' received for unknown state");
    }
}


/**
 * API message: 'rc' - The server is informing us of the return code on completion
 */
void robuild_msg_rc(wsclient *c, cJSON *json) {
    int rc = GET_JSON_INT(json, -1);
    if (rc < 0)
    {
        protoerror(json, "'rc' message must return a valid positive integer");
    }

    if (!global.quiet)
        printf("RC: %i\n", rc);

    if (global.state == s_compiling)
    {
        /* Record the RC for exit */
        global.rc = rc;
    }
    else
    {
        protoerror(json, "'rc' received for unknown state");
    }
}


/**
 * API message: 'complete' - The server is informing us that the build is now complete
 */
void robuild_msg_complete(wsclient *c, cJSON *json) {
    if (!global.quiet)
        printf("Server: Build complete\n");

    if (global.state == s_compiling)
    {
        global.state = s_complete;
    }
    else
    {
        protoerror(json, "'rc' received for unknown state");
    }
}


/**
 * API message: 'output' - The server is reporting VDU output.
 */
void robuild_msg_output(wsclient *c, cJSON *json) {
    char *msg = GET_JSON_STRING(json);
    if (!msg)
    {
        protoerror(json, "'output' message did not pass a string");
    }

    if (global.state != s_compiling)
    {
        protoerror(json, "'output' received when not compiling");
    }

    if (global.build_output_fh)
    {
        fwrite(msg, 1, strlen(msg), global.build_output_fh);
    }

    if (!global.really_quiet)
    {
        if (!global.last_message_was_output)
        {
            if (!global.quiet)
                printf("Output:\n");
            global.output_ends_in_newline = 1;
        }

        while (*msg)
        {
            char *newline = strchr(msg, '\n');

            if (global.output_ends_in_newline)
            {
                if (!global.quiet)
                    printf("  ");
            }

            if (newline == NULL)
            {
                global.output_ends_in_newline = 0;
                printf("%s", msg);
                msg += strlen(msg);
            }
            else
            {
                /* There is a newline in this line */
                global.output_ends_in_newline = 1;
                *newline = '\0';
                printf("%s\n", msg);
                msg = newline + 1;
            }
        }
    }
    global.last_message_was_output = 1;
}


/**
 * Received a message from the websocket.
 */
int onmessage(wsclient *c, wsclient_message *msg) {

#ifdef DEBUG
    fprintf(stderr, "onmessage: %s\n", msg->payload);
#endif

    char *wsmsg = msg->payload;
    cJSON *json = cJSON_Parse(wsmsg);
    cJSON *json_msgtype;
    cJSON *json_msgdata;
    char *msgtype;

    if (!cJSON_IsArray(json))
    {
        protoerror(json, "Bad message from server (not an array for server action)");
    }

    json_msgtype = json->child;
    json_msgdata = json_msgtype != NULL ? json_msgtype->next : NULL;

    msgtype = GET_JSON_STRING(json_msgtype);

    if (global.last_message_was_output && strcmp(msgtype, "output") != 0)
    {
        /* Transition from output to non-output lines */
        if (!global.quiet)
            if (!global.output_ends_in_newline)
                printf("\n");
        global.last_message_was_output = 0;
    }

    if (strcmp(msgtype, "welcome") == 0)
    {
        robuild_msg_welcome(c, json_msgdata);
    }
    else if (strcmp(msgtype, "error") == 0)
    {
        robuild_msg_error(c, json_msgdata);
    }
    else if (strcmp(msgtype, "response") == 0)
    {
        robuild_msg_response(c, json_msgdata);
    }
    else if (strcmp(msgtype, "message") == 0)
    {
        robuild_msg_message(c, json_msgdata);
    }
    else if (strcmp(msgtype, "output") == 0)
    {
        robuild_msg_output(c, json_msgdata);
    }
    else if (strcmp(msgtype, "rc") == 0)
    {
        robuild_msg_rc(c, json_msgdata);
    }
    else if (strcmp(msgtype, "complete") == 0)
    {
        robuild_msg_complete(c, json_msgdata);
    }
    else if (strcmp(msgtype, "throwback") == 0)
    {
        robuild_msg_throwback(c, json_msgdata);
    }
    else if (strcmp(msgtype, "clipboard") == 0)
    {
        robuild_msg_clipboard(c, json_msgdata);
    }
    else
    {
        char msg[256];
        sprintf(msg, "Unrecognised server action '%s'", msgtype);
        protoerror(json, msg);
    }

    /* We're finished with the message, so free it */
    cJSON_Delete(json);

    if (global.state == s_complete)
    {
        libwsclient_close(c);
    }

    return 0;
}

int parse_bool(char *arg, char *context)
{
    char msg[256];
    if (strcmp(arg, "on") == 0)
    {
        return 1;
    }
    else if (strcmp(arg, "off") == 0)
    {
        return 0;
    }

    /* Unrecognised boolean name */
    sprintf(msg, "Expected a boolean ('on' or 'off') %s", context);
    error(msg);

    return 0;
}


int main(int argc, char **argv) {
    int c;
    static char banner[]="RISC OS Build client for build.riscos.online v" Module_FullVersionAndDate "\n";
    static char syntax[]="Syntax: riscos-build-online [-h] [-q|-Q] [-a] [-A <arch>] -i <infile> [-s <server-uri>] [-o <outfile>] [-b <buildoutput>]\n";
    wsclient *client;

    global.server_uri = "ws://jfpatch.riscos.online/ws";
    global.source_file = NULL;
    global.output_prefix = "output";
    global.build_output_file = NULL;
    global.timeout = -1;
    global.state = s_connecting;
    global.rc = -1; /* Something that looks odd */
    global.ansitext = 1; /* server default */
    global.arch = NULL; /* Default */

    if (argc < 2) {
        printf("%s%s", banner, syntax);
        exit(0);
    }

    while ((c = getopt(argc, argv, "hqQs:i:o:b:t:a:A:")) != EOF) {
        switch (c) {
            case 's': /* server URI */
                global.server_uri = optarg;
                break;

            case 'i': /* input file */
                global.source_file = optarg;
                break;

            case 'o':
                global.output_prefix = optarg;
                break;

            case 'q':
                global.quiet = 1;
                break;

            case 'Q':
                global.really_quiet = 1;
                global.quiet = 1;
                break;

            case 'b':
                global.build_output_file = optarg;
                break;

            case 't':
                global.timeout = atoi(optarg);
                break;

            case 'a':
                global.ansitext = parse_bool(optarg, "parsing ansitext switch (-a)");
                break;

            case 'A':
                global.arch = optarg;
                break;

            case 'h':
                printf("\
%s%s\n\
  -h            Display this help message\n\
  -s <uri>      Specify the server URI to connect to (default: %s)\n\
  -i <file>     Specify input file\n\
  -q            Quiet non-output information\n\
  -Q            Quiet all non-failure information\n\
  -o <file>     %s\n\
  -b <file>     Specify build output file\n\
  -t <secs>     Specify the timeout to use (default is the service default)\n\
  -a on|off     Enable or disable ANSI text: 'off', 'on' (default)\n\
  -A <arch>     Architecture to use: 'aarch32' (default), 'aarch64'\n\
", banner, syntax, global.server_uri,
#ifdef __riscos
"Specify output filename (default 'output')"
#else
"Specify output file prefix; will be suffixed with `,xxx` filetype (default 'output')"
#endif
);
                exit(0);
        }
    }

    if (global.source_file == NULL)
    {
        fprintf(stderr, "Must supply an input file with -i <filename>\n");
        exit(1);
    }

    if (global.build_output_file)
    {
        global.build_output_fh = fopen(global.build_output_file, "w");
        if (!global.build_output_fh)
        {
            fprintf(stderr, "Cannot open build output file\n");
            exit(1);
        }
    }

    /* FIXME: configurable websocket server */
    client = libwsclient_new(global.server_uri);
    if(!client) {
        fprintf(stderr, "Unable to initialize new websocket client\n");
        exit(1);
    }
    /* FIXME: atexit to close sockets */

    /* Use line buffering as this is the most sensible output for most CI */
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

    //set callback functions for this client
    libwsclient_onopen(client, &onopen);
    libwsclient_onmessage(client, &onmessage);
    libwsclient_onerror(client, &onerror);
    libwsclient_onclose(client, &onclose);
    //starts run thread.
#ifdef __riscos
    libwsclient_handshake_thread(client);
    libwsclient_run_thread(client);
#else
    libwsclient_run(client);
#endif
    //blocks until run thread for client is done.
    libwsclient_finish(client);

    // Return the final RC we got
    return global.rc;
}
