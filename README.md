# Mastodon Avatar Display <!-- omit in toc -->

The intention of this project is to display the avatar of the last tooter
in your home timeline on a round LCD.  It currently displays the avatar of the last
toot on the public (federated) timeline.

The [TLS client](https://github.com/peterharperuk/pico-examples/tree/add_mbedtls_example/pico_w/tls_client) is based on the example in Peter Harper's branch of pico-examples.

The [jpeg decoder](https://github.com/bitbank2/JPEGDEC) is from Larry Bank, used under the APLv2.

[json11](https://github.com/dropbox/json11) is from Dropbox apparently - MIT license.  Think this is doing a crazy amount of heap allocation so planning to replace with cJSON.

The ST7789 library is my own creation, originally this version was meant for use on the PicoSystem.

You need to be on the develop branch of the pico-sdk to get pico-mbedtls.
