# Mastodon Avatar Display <!-- omit in toc -->

The intention of this project is to display the avatar of the last tooter
in your home timeline on a round LCD.  It currently just displays an avatar from a hardcoded URL.

The [TLS client](https://github.com/peterharperuk/pico-examples/tree/add_mbedtls_example/pico_w/tls_client) is based on the example in Peter Harper's branch of pico-examples.

The [jpeg decoder](https://github.com/bitbank2/JPEGDEC) is from Larry Bank, used under the APLv2.

The ST7789 library is my own creation, originally this version was meant for use on the PicoSystem.

You need to be on the develop branch of the pico-sdk to get pico-mbedtls.
