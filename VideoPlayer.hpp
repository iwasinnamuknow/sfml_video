#ifndef VIDEO_PLAYER_HPP
#define VIDEO_PLAYER_HPP

#include "AudioPlayer.hpp"
#include <SFML/Graphics.hpp>
#include <chrono>
#include <deque>
#include <filesystem>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

class VideoPlayer : public sf::Drawable {
public:
    explicit VideoPlayer(const std::filesystem::path& filename);
    ~VideoPlayer() override;

    auto setPosition(int window_width, int window_height) -> void;
    auto setScale(float factor_x, float factor_y) -> void;
    auto isFinished() -> bool;

    auto update() -> void;
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    auto stop() -> void;

private:
    uint8_t* m_io_buffer{nullptr};
    AVIOContext* m_io_ctx{nullptr};
    AVFormatContext* m_format_ctx{nullptr};
    AVCodec* m_video_codec{nullptr};
    AVCodecContext* m_video_codec_ctx{nullptr};
    AVCodec* m_audio_codec{nullptr};
    AVCodecParameters* m_video_codec_params{nullptr};
    int m_video_stream_index{-1};
    int m_audio_stream_index{-1};
    SwsContext* m_video_sampler{nullptr};
    AVFrame* m_rgba_frame{nullptr};
    SwrContext* m_audio_sampler{nullptr};
    AVCodecContext* m_audio_codec_ctx{nullptr};
    uint8_t* m_video_buffer{nullptr};
    sf::Sprite m_video_sprite;
    sf::Texture m_video_texture;
    AVFrame* m_frame{nullptr};
    AVPacket* m_packet{nullptr};
    bool m_stream_eof{false};
    double m_timebase;
    int m_last_pts{0};
    std::chrono::time_point<std::chrono::steady_clock> m_last_frame;
    AudioPlayer m_audio_player;

    struct VideoFrame {
        std::vector<uint8_t> data;
        int width;
        int height;
        int pts;

        VideoFrame(int w, int h, int ptsValue)
            : data(w * h * 4), width(w), height(h), pts(ptsValue) {}
    };

    std::deque<VideoFrame> m_decoded_video{};
    std::deque<VideoFrame> m_inuse_video_frames{};

    auto find_codecs() -> void;
    auto setup_video() -> void;
    auto setup_audio() -> void;
    auto decode_video_packet() -> int;
    auto decode_frame() -> int;
};

#endif // VIDEO_PLAYER_HPP