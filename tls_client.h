#pragma once

// Supplied buffer must be large enough for the HTTP response including the headers
// On success, returned value indicates the total length of received data,
//             out parameter content_ptr specifies where the content begins.
// On failure, returns a negative value.
int https_get(const char* hostname, const char* uri, char* buffer, int buf_len, char** content_ptr);
