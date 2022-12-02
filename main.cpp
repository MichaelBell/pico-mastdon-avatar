#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "secrets.h"

extern "C" {
#include "st7789_lcd.h"
#include "tls_client.h"
}

static ST7789 st;
uint32_t st_buffer[2][256];

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

int main() {
	stdio_init_all();

	st7789_init(&st, pio0, 0, st_buffer[0], st_buffer[1]);
	st7789_start_pixels_at(&st, 0, 0, DISPLAY_ROWS - 1, DISPLAY_COLS - 1);
	st7789_repeat_pixel(&st, 0xf000, DISPLAY_ROWS * DISPLAY_COLS);
	st7789_trigger_transfer(&st);
	st7789_wait_for_transfer_complete(&st);

	connect_wifi();

    char* content_ptr;
    int rsp_len = https_get("worldtimeapi.org", "/api/ip", https_buffer, HTTPS_BUF_LEN, &content_ptr);
    if (rsp_len > 0) {
        printf("Got HTTPS response:\n%s\n", content_ptr);
    }

	while (1);
}
