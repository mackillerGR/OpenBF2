// [CHOSEN, not recovered] The Bink entry points the movie player imports, on portable hosts, where
// RAD's library does not exist. FFmpeg decodes the files; the ABI is Code/RendDX9Abi/Bink.h.
// Video stays on the wall clock and sound goes straight to an SDL stream, apart from the game's
// OpenAL device. Movies have no seeking beyond the loop back to the first frame.

#include "Bink.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool g_soundEnabled = false;

struct Movie
{
    renddx9abi::BINK      handle;           // first: the handle Video reads is this object
    AVFormatContext*      format  = nullptr;
    AVCodecContext*       video   = nullptr;
    AVCodecContext*       audio   = nullptr;
    int                   videoStream = -1;
    int                   audioStream = -1;
    AVFrame*              frame   = nullptr;
    AVFrame*              samples = nullptr;
    AVPacket*             packet  = nullptr;
    SDL_AudioStream*      sound   = nullptr;
    Uint64                frameNanoseconds = 33333333;
    Uint64                nextFrameAt = 0;
    bool                  paused  = false;
    bool                  flushed = false;
    bool                  frameReady = false;
    std::vector<float>    interleaved;
};

AVCodecContext* openDecoder(AVStream* stream)
{
    const AVCodec* const codec = avcodec_find_decoder(stream->codecpar->codec_id);
    AVCodecContext* context = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (context && (avcodec_parameters_to_context(context, stream->codecpar) < 0
                    || avcodec_open2(context, codec, nullptr) < 0))
        avcodec_free_context(&context);
    return context;
}

void closeMovie(Movie* movie)
{
    if (movie->sound)
        SDL_DestroyAudioStream(movie->sound);
    av_packet_free(&movie->packet);
    av_frame_free(&movie->frame);
    av_frame_free(&movie->samples);
    avcodec_free_context(&movie->video);
    avcodec_free_context(&movie->audio);
    avformat_close_input(&movie->format);
    delete movie;
}

// Queues one decoded block of movie sound, opening the device on the first.
void playSamples(Movie* movie)
{
    const AVFrame* const frame = movie->samples;
    const int channels = frame->ch_layout.nb_channels;
    if (channels <= 0 || frame->nb_samples <= 0)
        return;
    if (!movie->sound)
    {
        const SDL_AudioSpec spec = { SDL_AUDIO_F32, channels, frame->sample_rate };
        if (!SDL_WasInit(SDL_INIT_AUDIO))
            SDL_InitSubSystem(SDL_INIT_AUDIO);
        movie->sound = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!movie->sound)
        {
            std::fprintf(stderr, "Bink: no sound device: %s\n", SDL_GetError());
            return;
        }
        SDL_ResumeAudioStreamDevice(movie->sound);
    }
    const int count = frame->nb_samples * channels;
    if (frame->format == AV_SAMPLE_FMT_FLT)
    {
        SDL_PutAudioStreamData(movie->sound, frame->data[0], count * (int)sizeof(float));
        return;
    }
    if (frame->format != AV_SAMPLE_FMT_FLTP)
        return;
    movie->interleaved.resize((size_t)count);
    for (int channel = 0; channel < channels; ++channel)
    {
        const float* const source = reinterpret_cast<const float*>(frame->extended_data[channel]);
        for (int i = 0; i < frame->nb_samples; ++i)
            movie->interleaved[(size_t)(i * channels + channel)] = source[i];
    }
    SDL_PutAudioStreamData(movie->sound, movie->interleaved.data(), count * (int)sizeof(float));
}

// Reads on until the next video frame is decoded. False at the end of the file.
bool decodeNextFrame(Movie* movie)
{
    for (;;)
    {
        const int received = avcodec_receive_frame(movie->video, movie->frame);
        if (received == 0)
            return true;
        if (received != AVERROR(EAGAIN))
            return false;
        if (movie->flushed)
            return false;
        if (av_read_frame(movie->format, movie->packet) < 0)
        {
            movie->flushed = true;
            avcodec_send_packet(movie->video, nullptr);
            continue;
        }
        if (movie->packet->stream_index == movie->videoStream)
        {
            avcodec_send_packet(movie->video, movie->packet);
        }
        else if (movie->audio && g_soundEnabled && movie->packet->stream_index == movie->audioStream)
        {
            if (avcodec_send_packet(movie->audio, movie->packet) == 0)
                while (avcodec_receive_frame(movie->audio, movie->samples) == 0)
                    playSamples(movie);
        }
        av_packet_unref(movie->packet);
    }
}

void rewindMovie(Movie* movie)
{
    av_seek_frame(movie->format, movie->videoStream, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(movie->video);
    if (movie->audio)
        avcodec_flush_buffers(movie->audio);
    movie->flushed = false;
}

inline unsigned char clampByte(int value)
{
    return (unsigned char)(value < 0 ? 0 : (value > 255 ? 255 : value));
}

} // anonymous namespace

extern "C" {

renddx9abi::HBINK BinkOpen(const char* name, unsigned int)
{
    std::string path(name ? name : "");
    std::replace(path.begin(), path.end(), '\\', '/');
    FILE* const probe = std::fopen(path.c_str(), "rb");
    if (!probe)
        return nullptr;
    std::fclose(probe);

    av_log_set_level(AV_LOG_ERROR);
    Movie* const movie = new Movie();
    if (avformat_open_input(&movie->format, path.c_str(), nullptr, nullptr) < 0
        || avformat_find_stream_info(movie->format, nullptr) < 0)
    {
        std::fprintf(stderr, "Bink: cannot read %s\n", path.c_str());
        closeMovie(movie);
        return nullptr;
    }
    movie->videoStream = av_find_best_stream(movie->format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    movie->audioStream = av_find_best_stream(movie->format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (movie->videoStream < 0)
    {
        closeMovie(movie);
        return nullptr;
    }
    AVStream* const stream = movie->format->streams[movie->videoStream];
    movie->video = openDecoder(stream);
    if (movie->audioStream >= 0)
        movie->audio = openDecoder(movie->format->streams[movie->audioStream]);
    movie->frame = av_frame_alloc();
    movie->samples = av_frame_alloc();
    movie->packet = av_packet_alloc();
    if (!movie->video || !movie->frame || !movie->samples || !movie->packet)
    {
        std::fprintf(stderr, "Bink: no decoder for %s\n", path.c_str());
        closeMovie(movie);
        return nullptr;
    }
    const AVRational rate = stream->avg_frame_rate.num ? stream->avg_frame_rate : stream->r_frame_rate;
    if (rate.num > 0 && rate.den > 0)
        movie->frameNanoseconds = (Uint64)1000000000 * (Uint64)rate.den / (Uint64)rate.num;
    movie->handle.Width = (unsigned)movie->video->width;
    movie->handle.Height = (unsigned)movie->video->height;
    movie->handle.Frames = (unsigned)(stream->nb_frames > 0 ? stream->nb_frames : stream->duration);
    movie->handle.FrameNum = 1;
    movie->nextFrameAt = SDL_GetTicksNS();
    return &movie->handle;
}

void BinkClose(renddx9abi::HBINK handle)
{
    if (handle)
        closeMovie(reinterpret_cast<Movie*>(handle));
}

int BinkWait(renddx9abi::HBINK handle)
{
    const Movie* const movie = reinterpret_cast<Movie*>(handle);
    return movie->paused || SDL_GetTicksNS() < movie->nextFrameAt;
}

int BinkDoFrame(renddx9abi::HBINK handle)
{
    Movie* const movie = reinterpret_cast<Movie*>(handle);
    movie->frameReady = decodeNextFrame(movie);
    const Uint64 now = SDL_GetTicksNS();
    movie->nextFrameAt += movie->frameNanoseconds;
    if (movie->nextFrameAt < now)
        movie->nextFrameAt = now;       // a slow frame is not chased
    return 0;
}

void BinkNextFrame(renddx9abi::HBINK handle)
{
    Movie* const movie = reinterpret_cast<Movie*>(handle);
    if (handle->FrameNum < handle->Frames)
    {
        ++handle->FrameNum;
        return;
    }
    rewindMovie(movie);
    handle->FrameNum = 1;
}

void BinkCopyToBuffer(renddx9abi::HBINK handle, void* destination, int pitch, unsigned int destinationHeight,
                      unsigned int, unsigned int, unsigned int)
{
    const Movie* const movie = reinterpret_cast<Movie*>(handle);
    const AVFrame* const frame = movie->frame;
    if (!movie->frameReady || !destination)
        return;
    if (frame->format != AV_PIX_FMT_YUV420P && frame->format != AV_PIX_FMT_YUVA420P)
        return;
    const bool fullRange = frame->color_range == AVCOL_RANGE_JPEG;
    const unsigned int width = std::min((unsigned)frame->width, (unsigned)(pitch / 4));
    const unsigned int height = std::min((unsigned)frame->height, destinationHeight);
    for (unsigned int y = 0; y < height; ++y)
    {
        const unsigned char* const luma = frame->data[0] + y * frame->linesize[0];
        const unsigned char* const blue = frame->data[1] + (y / 2) * frame->linesize[1];
        const unsigned char* const red = frame->data[2] + (y / 2) * frame->linesize[2];
        unsigned char* out = static_cast<unsigned char*>(destination) + y * pitch;
        for (unsigned int x = 0; x < width; ++x, out += 4)
        {
            const int u = blue[x / 2] - 128, v = red[x / 2] - 128;
            const int c = fullRange ? luma[x] * 256 : (luma[x] - 16) * 298;
            out[0] = clampByte((c + (fullRange ? 454 : 516) * u + 128) >> 8);
            out[1] = clampByte((c - (fullRange ? 88 : 100) * u - (fullRange ? 183 : 208) * v + 128) >> 8);
            out[2] = clampByte((c + (fullRange ? 359 : 409) * v + 128) >> 8);
            out[3] = 255;
        }
    }
}

void BinkGetSummary(renddx9abi::HBINK handle, renddx9abi::BINKSUMMARY* summary)
{
    summary->Width = handle->Width;
    summary->Height = handle->Height;
    summary->TotalFrames = handle->Frames;
}

void BinkPause(renddx9abi::HBINK handle, int pause)
{
    Movie* const movie = reinterpret_cast<Movie*>(handle);
    movie->paused = pause != 0;
    if (movie->sound)
    {
        if (pause)
            SDL_PauseAudioStreamDevice(movie->sound);
        else
            SDL_ResumeAudioStreamDevice(movie->sound);
    }
    if (!pause)
        movie->nextFrameAt = SDL_GetTicksNS();
}

void* BinkOpenDirectSound(unsigned int)
{
    return nullptr;
}

int BinkSetSoundSystem(void*, unsigned int)
{
    g_soundEnabled = true;
    return 1;
}

} // extern "C"
