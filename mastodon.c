#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

#include "mastodon.h"
#include "secrets.h"
#include "tls_client.h"

#include "cJSON.h"

// This buffer is used for everything:
// - HTTPS request/response building
// - JSON parsing
#define BUFFER_LEN 40000
static char buffer[BUFFER_LEN];
static char* alloc_ptr;

#define ALIGN_PTR(X) (void*)((uintptr_t)(X + 3) & ~3u)


static void* allocate(size_t sz) {
    if (alloc_ptr + sz > buffer + BUFFER_LEN) {
        printf("Alloc req for %d failed\n", sz);
        return NULL;
    }

    void* ptr = alloc_ptr;
    alloc_ptr = ALIGN_PTR(alloc_ptr + sz);
    return ptr;
}

static void deallocate(void* ptr) {
}

cJSON_Hooks hooks_for_cjson = { &allocate, &deallocate };

#define AUTH_HEADER "Authorization: Bearer " MASTODON_TOKEN "\r\n"

static void parse_account_json(MTOOT* toot, cJSON* account_json)
{
    cJSON* acct_json = cJSON_GetObjectItemCaseSensitive(account_json, "acct");
    if (cJSON_IsString(acct_json)) toot->originator_acct = acct_json->valuestring;

    cJSON* avatar_json = cJSON_GetObjectItemCaseSensitive(account_json, "avatar");
    if (cJSON_IsString(avatar_json) && avatar_json->valuestring != NULL) {
        toot->originator_avatar_path = strchr(avatar_json->valuestring + 8, '/');
    }
}

bool get_latest_home_toot(MTOOT* toot, const char* last_toot_id)
{
    char* content_ptr;
    char req[128] = "/api/v1/timelines/home?limit=1";
    if (last_toot_id) {
        strcat(req, "&since_id=");
        strcat(req, last_toot_id);
    }
    int rsp_len = https_get(MASTODON_HOST, req, AUTH_HEADER, buffer, BUFFER_LEN, &content_ptr);
    if (rsp_len <= 0) {
        printf("Couldn't fetch latest toot\n");
        return false;
    }
    printf("Got JSON:\n%s\n", content_ptr);

    alloc_ptr = ALIGN_PTR(buffer + rsp_len);

    memset(toot, 0, sizeof(MTOOT));

    cJSON_InitHooks(&hooks_for_cjson);
    cJSON* json = cJSON_Parse(content_ptr);
    if (!json) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            printf("Json parsing error at: %s\n", error_ptr);
        }
        return false;
    }
    printf("Buffer used: %d/%d\n", alloc_ptr - buffer, BUFFER_LEN);

    cJSON* toot_json = cJSON_GetArrayItem(json, 0);
    if (!toot_json) {
        printf("No toot\n");
        return false;
    }
    
    cJSON* id_json = cJSON_GetObjectItemCaseSensitive(toot_json, "id");
    if (cJSON_IsString(id_json)) toot->id = id_json->valuestring;

    cJSON* content_json = cJSON_GetObjectItemCaseSensitive(toot_json, "content");
    if (cJSON_IsString(content_json)) toot->content = content_json->valuestring;

    cJSON* account_json = cJSON_GetObjectItemCaseSensitive(toot_json, "account");
    parse_account_json(toot, account_json);

    cJSON* reblog_json = cJSON_GetObjectItemCaseSensitive(toot_json, "reblog");
    if (cJSON_IsObject(reblog_json)) {
        toot->booster_acct = toot->originator_acct;
        toot->booster_avatar_path = toot->originator_avatar_path;
        toot->originator_acct = NULL;
        toot->originator_avatar_path = NULL;
        
        account_json = cJSON_GetObjectItemCaseSensitive(reblog_json, "account");
        parse_account_json(toot, account_json);
    }

    return true;
}

uint8_t* get_avatar_jpeg(const char* avatar_path, int* len_out)
{
    char* content_ptr;
    char avatar_uri[128] = "/small";
    strcpy(avatar_uri + 6, avatar_path);

    int rsp_len = https_get(MASTODON_HOST, avatar_uri, AUTH_HEADER, buffer, BUFFER_LEN, &content_ptr);
    if (rsp_len <= 0) return NULL;

    *len_out = rsp_len - (content_ptr - buffer);
    return (uint8_t*)content_ptr;
}
