# Mastodon Avatar Display <!-- omit in toc -->

This displays the avatar of the last tooter in your home timeline on a round LCD, inlaying the booster's avatar for boosted toots.

You need to be on the develop branch of the pico-sdk to get pico-mbedtls, which is used to make HTTPS requests to Mastodon.

This is not currently easily usable, because it relies on being able to fetch a <38kB 240x240 jpeg version of avatar from the Mastodon server by tweaking the normal avatar URL.  It would be fairly straightforward to make a relay that does this locally - I'll add instructions to do that in future.

## Acknowledgments

The [TLS client](https://github.com/peterharperuk/pico-examples/tree/add_mbedtls_example/pico_w/tls_client) is based on the example in Peter Harper's branch of pico-examples.

The [jpeg decoder](https://github.com/bitbank2/JPEGDEC) is from Larry Bank, used under the APLv2.

[cJSON](https://github.com/DaveGamble/cJSON) is a light weight C JSON parser, used under the MIT license.

I've added the files directly rather than using submodules as it is simpler to make small tweaks this way, and they are only a couple of files from larger repos.

The ST7789 library is my own creation, originally this version was meant for use on the PicoSystem.

