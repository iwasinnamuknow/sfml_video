#ifndef AUDIO_PLAYER_HPP
#define AUDIO_PLAYER_HPP

#include <SFML/Audio.hpp>
#include <condition_variable>
#include <mutex>
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
}
#endif

class AudioPlayer : public sf::SoundStream {
public:
    void load(AVCodecContext* codec_ctx);

    void enqueueSamples(const int16_t* samples, std::size_t sample_count);

    void stopStream();

protected:
    auto onGetData(Chunk& data) -> bool override;

    void onSeek(sf::Time /*time_offset*/) override {}

    static const size_t m_buffer_size;

private:
    std::vector<int16_t>    m_buffer;
    std::vector<int16_t>    m_audio_queue;
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    bool                    m_stopping{false};
};

#endif // AUDIO_PLAYER_HPP