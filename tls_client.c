#include <string.h>
#include <time.h>

#include "hardware/structs/rosc.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

#define TLS_CLIENT_HTTP_REQUEST  "GET %s HTTP/1.1\r\n" \
                                 "Host: %s\r\n" \
                                 "%s" \
                                 "Connection: close\r\n" \
                                 "\r\n"
#define TLS_CLIENT_TIMEOUT_SECS  15

typedef struct TLS_CLIENT_T_ {
    struct altcp_pcb *pcb;
    char* req;
    char* rsp;
    int rsp_idx;
    int rsp_buf_len;
    bool complete;
} TLS_CLIENT_T;

static struct altcp_tls_config *tls_config = NULL;


/* Function to feed mbedtls entropy. May be better to move it to pico-sdk */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    /* Code borrowed from pico_lwip_random_byte(), which is static, so we cannot call it directly */
    static uint8_t byte;

    for(int p=0; p<len; p++) {
        for(int i=0;i<32;i++) {
            // picked a fairly arbitrary polynomial of 0x35u - this doesn't have to be crazily uniform.
            byte = ((byte << 1) | rosc_hw->randombit) ^ (byte & 0x80u ? 0x35u : 0);
            // delay a little because the random bit is a little slow
            busy_wait_at_least_cycles(30);
        }
        output[p] = byte;
    }

    *olen = len;
    return 0;
}


static err_t tls_client_close(void *arg) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;
    err_t err = ERR_OK;

    state->complete = true;
    if (state->pcb != NULL) {
        altcp_arg(state->pcb, NULL);
        altcp_poll(state->pcb, NULL, 0);
        altcp_recv(state->pcb, NULL);
        altcp_err(state->pcb, NULL);
        err = altcp_close(state->pcb);
        if (err != ERR_OK) {
            printf("close failed %d, calling abort\n", err);
            altcp_abort(state->pcb);
            err = ERR_ABRT;
        }
        state->pcb = NULL;
    }
    return err;
}

static err_t tls_client_connected(void *arg, struct altcp_pcb *pcb, err_t err) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;
    if (err != ERR_OK) {
        printf("connect failed %d\n", err);
        return tls_client_close(state);
    }

    printf("connected to server, sending request\n");
    err = altcp_write(state->pcb, state->req, strlen(state->req), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("error writing data, err=%d", err);
        return tls_client_close(state);
    }

    return ERR_OK;
}

static err_t tls_client_poll(void *arg, struct altcp_pcb *pcb) {
    printf("timed out");
    return tls_client_close(arg);
}

static void tls_client_err(void *arg, err_t err) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;
    printf("tls_client_err %d\n", err);
    state->pcb = NULL; /* pcb freed by lwip when _err function is called */
}

static err_t tls_client_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;
    if (!p) {
        printf("connection closed\n");
        return tls_client_close(state);
    }

    int recv_len = p->tot_len;
    if (recv_len + state->rsp_idx >= state->rsp_buf_len) {
        printf("HTTPS response exceeded buffer length\n");
        recv_len = state->rsp_buf_len - state->rsp_idx - 1;
    }

    if (p->tot_len > 0) {
        printf("%d bytes new data received from server, %d stored\n", p->tot_len, recv_len);

        if (recv_len > 0) {
            char* buf = state->rsp + state->rsp_idx;
            pbuf_copy_partial(p, buf, recv_len, 0);
            buf[recv_len] = 0;
            state->rsp_idx += recv_len;
        }

        altcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);

    return ERR_OK;
}

static void tls_client_connect_to_server_ip(const ip_addr_t *ipaddr, TLS_CLIENT_T *state)
{
    err_t err;
    u16_t port = 443;

    printf("connecting to server IP %s port %d\n", ipaddr_ntoa(ipaddr), port);
    err = altcp_connect(state->pcb, ipaddr, port, tls_client_connected);
    if (err != ERR_OK)
    {
        fprintf(stderr, "error initiating connect, err=%d\n", err);
        tls_client_close(state);
    }
}

static void tls_client_dns_found(const char* hostname, const ip_addr_t *ipaddr, void *arg)
{
    if (ipaddr)
    {
        printf("DNS resolving complete\n");
        tls_client_connect_to_server_ip(ipaddr, (TLS_CLIENT_T *) arg);
    }
    else
    {
        printf("error resolving hostname %s\n", hostname);
        tls_client_close(arg);
    }
}


static bool tls_client_open(const char *hostname, void *arg) {
    err_t err;
    ip_addr_t server_ip;
    TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;

    state->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_ANY);
    if (!state->pcb) {
        printf("failed to create pcb\n");
        return false;
    }

    altcp_arg(state->pcb, state);
    altcp_poll(state->pcb, tls_client_poll, TLS_CLIENT_TIMEOUT_SECS * 2);
    altcp_recv(state->pcb, tls_client_recv);
    altcp_err(state->pcb, tls_client_err);

    /* Set SNI */
    mbedtls_ssl_set_hostname(altcp_tls_context(state->pcb), hostname);

    printf("resolving %s\n", hostname);

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();

    err = dns_gethostbyname(hostname, &server_ip, tls_client_dns_found, state);
    if (err == ERR_OK)
    {
        /* host is in DNS cache */
        tls_client_connect_to_server_ip(&server_ip, state);
    }
    else if (err != ERR_INPROGRESS)
    {
        printf("error initiating DNS resolving, err=%d\n", err);
        tls_client_close(state->pcb);
    }

    cyw43_arch_lwip_end();

    return err == ERR_OK || err == ERR_INPROGRESS;
}

// Perform initialisation
static TLS_CLIENT_T* tls_client_init(void) {
    TLS_CLIENT_T *state = calloc(1, sizeof(TLS_CLIENT_T));
    if (!state) {
        printf("failed to allocate state\n");
        return NULL;
    }

    return state;
}

static int hex_to_int(char c) {
    if (c < '0') return -1;
    if (c <= '9') return c - '0';
    if (c < 'A') return -1;
    if (c <= 'F') return c - 'A' + 10;
    if (c < 'a') return -1;
    if (c <= 'f') return c - 'a' + 10;
    return -1;
}

// Remove chunk encoding from HTTP content
static bool dechunk(char* content_ptr, int* content_len) {
    char* original_start = content_ptr;
    char* move_to = content_ptr;
    while (true) {
        int chunk_len = 0;
        while (*content_ptr != '\r') {
            chunk_len <<= 4;
            int next_val = hex_to_int(*content_ptr++);
            if (next_val < 0) {
                printf("Dechunk failed: non-hex value in chunk size\n");
                return false;
            }
            chunk_len += next_val;
        }

        if (content_ptr[1] != '\n') {
            printf("Dechunk failed: Expected \\r\\n after chunk length\n");
            return false;
        }

        if (chunk_len == 0) break;

        printf("Remove chunk length %d\n", chunk_len);
        content_ptr += 2;
        if ((content_ptr + chunk_len) - original_start > *content_len) {
            printf("Dechunk failed: Chunk exceeded content length\n");
            return false;
        }

        memmove(move_to, content_ptr, chunk_len);
        move_to += chunk_len;
        content_ptr += chunk_len + 2;
    }

    *content_len = move_to - original_start;
    original_start[*content_len] = 0;
    return true;
}

// Supplied buffer must be large enough for the HTTP response including the headers
// On success, returned value indicates the total length of received data,
//             out parameter content_ptr specifies where the content begins.
// On failure, returns a negative value.
int https_get(const char* hostname, const char* uri, const char* headers, char* buffer, int buf_len, char** content_ptr) {
    /* No CA certificate checking */
    tls_config = altcp_tls_create_config_client(NULL, 0);

    printf("Starting HTTPS get for %s\n", uri);

    TLS_CLIENT_T *state = tls_client_init();
    if (!state) {
        return -1;
    }
    state->req = buffer;
    snprintf(state->req, buf_len, TLS_CLIENT_HTTP_REQUEST, uri, hostname, headers ? headers : "");
    state->rsp = buffer;
    state->rsp_buf_len = buf_len;
    if (!tls_client_open(hostname, state)) {
        return -2;
    }
    while (!state->complete) {
        // the following #ifdef is only here so this same example can be used in multiple modes;
        // you do not need it in your code
#if PICO_CYW43_ARCH_POLL
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for WiFi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
#endif
        sleep_ms(1);
    }

    int rsp_len = state->rsp_idx;

    free(state);
    altcp_tls_free_config(tls_config);

    // Check whether we received any data
    if (rsp_len == 0) return -3;

    // Content starts after two \r\ns
    *content_ptr = strstr(buffer, "\r\n\r\n");
    if (!*content_ptr) return -4;
    (*content_ptr)[2] = 0;
    *content_ptr += 4;

    // Check headers for chunked encoding
    int content_len = rsp_len - (*content_ptr - buffer);
    if (strstr(buffer, "Transfer-Encoding: chunked\r\n")) {
        if (!dechunk(*content_ptr, &content_len)) {
            return -5;
        }
        rsp_len = (*content_ptr - buffer) + content_len;
    }

    // Success!
    return rsp_len;
}

