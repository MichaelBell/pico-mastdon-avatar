#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

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
    if (cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_pass, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
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

    char* content_ptr;
    int rsp_len = https_get("rebel-lion.uk", "/small/system/cache/accounts/avatars/109/449/728/322/785/203/original/2e5db4e6374a0202.jpg", https_buffer, HTTPS_BUF_LEN, &content_ptr);
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

    while (1);
}
