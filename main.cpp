#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "JPEGDEC.h"

#include "secrets.h"

extern "C" {
#include "st7789_lcd.h"
#include "tls_client.h"
#include "mastodon.h"
}

// Display control and command buffers
static ST7789 st;
uint32_t st_buffer[2][32];

static JPEGDEC jpegdec;

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
    //printf("Drawing %d pixels at %d-%d, %d-%d\n", 
    //        pDraw->iWidth * pDraw->iHeight, pDraw->x, pDraw->iWidth, pDraw->y, pDraw->iHeight, pDraw->pPixels);

    st7789_wait_for_transfer_complete(&st);
    st7789_start_pixels_at(&st, pDraw->x, pDraw->y, pDraw->x + pDraw->iWidth - 1, pDraw->y + pDraw->iHeight - 1);
    st7789_dma_pixel_data(&st, (uint32_t*)pDraw->pPixels, pDraw->iWidth * pDraw->iHeight / 2, true);
    st7789_trigger_transfer(&st);

    return 1;
}

#define TOOT_ID_LEN 20
#define TOOT_AVATAR_PATH_LEN 128

void display_avatar(const uint8_t* avatar_jpeg, int jpeg_len, bool booster) {
    if (jpeg_len > 0) {
        printf("Decoding JPEG length %d\n", jpeg_len);
        jpegdec.openRAM(avatar_jpeg, jpeg_len, &jpeg_draw_callback);
        jpegdec.setPixelType(RGB565_BIG_ENDIAN);
        if (!booster) {
            jpegdec.decode(0, 0, 0);
        }
        else {
            jpegdec.decode(170, 170, JPEG_SCALE_QUARTER);
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
    MTOOT last_toot;
    while (true) {
        if (get_latest_home_toot(&last_toot)) {
            if (strcmp(last_toot_id, last_toot.id)) {
                strcpy(last_toot_id, last_toot.id);

                char booster_avatar_path[TOOT_AVATAR_PATH_LEN] = "";
                if (last_toot.booster_avatar_path) strcpy(booster_avatar_path, last_toot.booster_avatar_path);

                int len;
                const uint8_t* jpeg_data = get_avatar_jpeg(last_toot.originator_avatar_path, &len);
                display_avatar(jpeg_data, len, false);

                if (last_toot.booster_avatar_path) {
                    jpeg_data = get_avatar_jpeg(booster_avatar_path, &len);
                    display_avatar(jpeg_data, len, true);
                }
            }
        }

        sleep_ms(15000);
    }
}

