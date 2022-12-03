#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "json11.hpp"
#include "JPEGDEC.h"

#include "secrets.h"

extern "C" {
#include "st7789_lcd.h"
#include "tls_client.h"
}

static ST7789 st;
uint32_t st_buffer[2][256];

static JPEGDEC jpegdec;

#define HTTPS_BUF_LEN 16384
char https_buffer[HTTPS_BUF_LEN];

#define DISPLAY_ROWS 240
#define DISPLAY_COLS 240

bool connect_wifi() {
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
        printf("failed to initialise\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();

    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        return false;
    } else {
        printf("Connected.\n");
        return true;
    }
}

int jpeg_draw_callback(JPEGDRAW* pDraw) {
    printf("Drawing %d pixels at %d-%d, %d-%d\n", 
            pDraw->iWidth * pDraw->iHeight, pDraw->x, pDraw->iWidth, pDraw->y, pDraw->iHeight, pDraw->pPixels);

    st7789_wait_for_transfer_complete(&st);
    st7789_start_pixels_at(&st, pDraw->x, pDraw->y, pDraw->x + pDraw->iWidth - 1, pDraw->y + pDraw->iHeight - 1);
    st7789_dma_pixel_data(&st, (uint32_t*)pDraw->pPixels, pDraw->iWidth * pDraw->iHeight / 2, true);
    st7789_trigger_transfer(&st);

    return 1;
}

#define AUTH_HEADER "Authorization: Bearer " MASTODON_TOKEN "\r\n"

#define AVATAR_URI_MAX_LEN 128
static char avatar_uri_buffer[AVATAR_URI_MAX_LEN] = "/small";

#define TOOT_ID_LEN 20
static char toot_id[TOOT_ID_LEN];

const char* get_last_avatar_uri() {
    char* content_ptr;
    int rsp_len = https_get("rebel-lion.uk", "/api/v1/timelines/home?limit=1", AUTH_HEADER, https_buffer, HTTPS_BUF_LEN, &content_ptr);
    if (rsp_len <= 0) {
        printf("Couldn't fetch latest toot\n");
        return nullptr;
    }
    printf("Got JSON:\n%s\n", content_ptr);

    std::string parse_err;
    const auto json_rsp = json11::Json::parse(content_ptr, parse_err);
    if (json_rsp.is_null()) {
        printf("Failed to parse json: %s\n", parse_err.c_str());
        return nullptr;
    }

    const auto& json_toot_id = json_rsp[0]["id"];
    if (!json_toot_id.is_string()) {
        printf("Failed to find toot id\n");
        return nullptr;
    }

    const auto& toot_id_str = json_toot_id.string_value();
    if (toot_id_str.length() >= TOOT_ID_LEN - 1) {
        printf("Toot ID too long!\n");
        return nullptr;
    }
    strcpy(toot_id, toot_id_str.c_str());

    const auto& avatar_url = json_rsp[0]["account"]["avatar"];
    if (!avatar_url.is_string()) {
        printf("Failed to find avatar url\n");
        return nullptr;
    }

    const auto& avatar_url_str = avatar_url.string_value();
    if (avatar_url_str.length() > AVATAR_URI_MAX_LEN) {
        printf("Avatar URI too long for buffer\n");
        return nullptr;
    }

    size_t uri_start_idx = avatar_url_str.find('/', 8);
    if (uri_start_idx == std::string::npos) {
        printf("Failed to parse avatar url: %s\n", avatar_url_str.c_str());
        return nullptr;
    }

    // Copy in after the /small prefix
    strcpy(&avatar_uri_buffer[6], &avatar_url_str.c_str()[uri_start_idx]);
    return avatar_uri_buffer;
}

void display_avatar(const char* avatar_uri) {
    printf("Fetching avatar at: %s\n", avatar_uri);
    char* content_ptr;
    int rsp_len = https_get("rebel-lion.uk", avatar_uri, nullptr, https_buffer, HTTPS_BUF_LEN, &content_ptr);
    printf("https_get returned %d\n", rsp_len);
    if (rsp_len > 0) {
        printf("Received %d bytes.  Headers: \n%s\n", rsp_len, https_buffer);

        int content_len = rsp_len - (content_ptr - https_buffer);
        if (content_len > 0) {
            printf("Decoding JPEG length %d\n", content_len);
            jpegdec.openRAM((uint8_t*)content_ptr, content_len, &jpeg_draw_callback);
            jpegdec.setPixelType(RGB565_BIG_ENDIAN);
            jpegdec.decode(0, 0, 0);
        }
    }
}

int main() {
    stdio_init_all();

    st7789_init(&st, pio0, pio_claim_unused_sm(pio0, true), st_buffer[0], st_buffer[1]);
    st7789_start_pixels_at(&st, 0, 0, DISPLAY_ROWS - 1, DISPLAY_COLS - 1);
    st7789_repeat_pixel(&st, 0xf000, DISPLAY_ROWS * DISPLAY_COLS);
    st7789_trigger_transfer(&st);
    st7789_wait_for_transfer_complete(&st);

    st7789_start_pixels_at(&st, 100, 100, 139, 139);
    st7789_repeat_pixel(&st, 0x000f, 40*40);
    st7789_trigger_transfer(&st);
    st7789_wait_for_transfer_complete(&st);

    while (!connect_wifi()) {
        sleep_ms(10000);
    }

    char last_toot_id[TOOT_ID_LEN] = "";
    while (true) {
        const char* avatar_uri = get_last_avatar_uri();
        if (!avatar_uri) return 1;

        if (strcmp(last_toot_id, toot_id)) {
            strcpy(last_toot_id, toot_id);

            display_avatar(avatar_uri);
        }

        sleep_ms(15000);
    }
}

