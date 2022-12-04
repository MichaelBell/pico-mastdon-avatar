#pragma once

// Functions on this interface fill this struct with pointers into an internal buffer
// The buffer is overwritten on each call, so for any data that must persist over calls
// the data must be copied.
typedef struct mtoot_tag {
    const char* id;
    const char* originator_acct;
    const char* originator_avatar_path;
    const char* booster_acct;
    const char* booster_avatar_path;
    const char* content;
} MTOOT;

// Fetch the latest toot on the home timeline
// Returns true on success, false on failure
// If last_toot_id is non-NULL returns false if there is no newer toot
bool get_latest_home_toot(MTOOT* toot, const char* last_toot_id);

// Get the image for an avatar.  This overwrites the buffer used for toots, but it
// is safe to provide a pointer from a toot as the path.
uint8_t* get_avatar_jpeg(const char* avatar_path, int* len_out);
