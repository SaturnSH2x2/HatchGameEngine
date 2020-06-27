#if INTERFACE
#include <Engine/Includes/Standard.h>

class MediaPlayerState {
public:
    static Uint32 InitFlags;
    static Uint32 ThreadCount;
    static Uint32 FontHinting;
    static Uint32 VideoBufFrames;
    static Uint32 AudioBufFrames;
    static Uint32 SubtitleBufFrames;
    static void*  LibassHandle;
    static void*  AssSharedObjectHandle;
};
#endif

#include <Engine/Media/Utils/MediaPlayerState.h>
#include <Engine/Media/Includes/AVFormat.h>
#include <Engine/Media/Includes/AVUtils.h>

Uint32 MediaPlayerState::InitFlags = 0;
Uint32 MediaPlayerState::ThreadCount = 1;
Uint32 MediaPlayerState::FontHinting = 0;
Uint32 MediaPlayerState::VideoBufFrames = 24 * 2;
Uint32 MediaPlayerState::AudioBufFrames = 20 * 3;
Uint32 MediaPlayerState::SubtitleBufFrames = 64;
void*  MediaPlayerState::LibassHandle = NULL;
void*  MediaPlayerState::AssSharedObjectHandle = NULL;

const char* const font_mime[] = {
    "application/x-font-ttf",
    "application/x-font-truetype",
    "application/x-truetype-font",
    "application/x-font-opentype",
    "application/vnd.ms-opentype",
    "application/font-sfnt",
    NULL
};

#ifdef USING_LIBAV

PUBLIC STATIC double MediaPlayerState::GetSystemTime() {
    return (double)av_gettime() / 1000000.0;
}
PUBLIC STATIC bool   MediaPlayerState::AttachmentIsFont(void* p) {
    AVStream* stream = (AVStream*)p;
    AVDictionaryEntry *tag = av_dict_get(stream->metadata, "mimetype", NULL, AV_DICT_MATCH_CASE);
    if (tag) {
        for (int n = 0; font_mime[n]; n++) {
            if (av_strcasecmp(font_mime[n], tag->value) == 0) {
                return true;
            }
        }
    }
    return false;
}

#endif