#include "VideoPlayer.hpp"
#include <cstring>
#include <stdexcept>

#ifdef av_err2str
#undef av_err2str
#include <string>
av_always_inline std::string av_err2string(int errnum) {
    char str[AV_ERROR_MAX_STRING_SIZE];
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#define av_err2str(err) av_err2string(err).c_str()
#endif// av_err2str

VideoPlayer::VideoPlayer(const std::filesystem::path& filename) {

    m_format_ctx = avformat_alloc_context();

    if (m_format_ctx == nullptr) {
        throw std::runtime_error("ERROR could not allocate memory for Format Context");
    }

    if (avformat_open_input(&m_format_ctx, filename.c_str(), nullptr, nullptr) != 0) {
        throw std::runtime_error("Could not open file: " + filename.string());
    }

    if (avformat_find_stream_info(m_format_ctx, nullptr) < 0) {
        throw std::runtime_error("Could not get stream info");
    }

    find_codecs();
    setup_video();
    setup_audio();
}

VideoPlayer::~VideoPlayer() {

    stop();

    av_frame_free(&m_frame);
    av_frame_free(&m_rgba_frame);

    av_packet_unref(m_packet);
    av_packet_free(&m_packet);

    avcodec_free_context(&m_video_codec_ctx);
    avcodec_free_context(&m_audio_codec_ctx);

    swr_free(&m_audio_sampler);
    sws_freeContext(m_video_sampler);

    avformat_close_input(&m_format_ctx);

    av_free(m_video_buffer);
    avio_context_free(&m_io_ctx);
}

auto VideoPlayer::update() -> void {
    if (!m_stream_eof) {
        decode_frame();
    }

    if (m_stream_eof && m_decoded_video.empty()) {
        m_audio_player.stopStream();
    }

    if (m_decoded_video.empty()) {
        return;
    }

    auto next_pts = m_decoded_video.front().pts;
    auto pts_diff = next_pts - m_last_pts;
    auto pts_diff_seconds = pts_diff * m_timebase;
    auto time_now = std::chrono::steady_clock::now();
    std::chrono::duration<double> time_diff = time_now - m_last_frame;
    auto time_diff_seconds = time_diff.count();

    if (time_diff_seconds >= pts_diff_seconds) {

        if (!m_inuse_video_frames.empty()) {
            m_inuse_video_frames.clear();
        }

        m_inuse_video_frames.push_back(std::move(m_decoded_video.front()));
        m_decoded_video.pop_front();

        auto& frame = m_inuse_video_frames.back();
        m_video_texture.update(frame.data.data());
        m_last_pts = frame.pts;
        m_last_frame = time_now;
    }
}

void VideoPlayer::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    target.draw(m_video_sprite, states);
}

auto VideoPlayer::isFinished() -> bool {
    return (m_stream_eof && m_decoded_video.empty());
}

auto VideoPlayer::find_codecs() -> void {
    // loop though all the streams and print its main information
    for (uint64_t i = 0; i < m_format_ctx->nb_streams; i++) {
        AVCodecParameters* local_codec_params{nullptr};
        local_codec_params = m_format_ctx->streams[i]->codecpar;

        const AVCodec* local_codec = avcodec_find_decoder(local_codec_params->codec_id);

        if (local_codec == nullptr) {
            continue;
        }

        // when the stream is a video we store its index, codec parameters and codec
        if (local_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (m_video_stream_index == -1) {
                m_video_stream_index = i;
                m_video_codec = const_cast<AVCodec*>(local_codec);
                m_video_codec_params = local_codec_params;
            }
        } else if (local_codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (m_audio_stream_index == -1) {
                m_audio_stream_index = i;
                m_audio_codec = const_cast<AVCodec*>(local_codec);
            }
        }
    }

    if (m_audio_stream_index == -1) {
        throw std::runtime_error("File does not contain an audio stream!");
    }

    if (m_video_stream_index == -1) {
        throw std::runtime_error("File does not contain a video stream!");
    }
}

auto VideoPlayer::setup_video() -> void {
    // https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html
    m_video_codec_ctx = avcodec_alloc_context3(m_video_codec);
    if (m_video_codec_ctx == nullptr) {
        throw std::runtime_error("failed to allocated memory for AVCodecContext");
    }

    // Fill the codec context based on the values from the supplied codec parameters
    // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
    if (avcodec_parameters_to_context(m_video_codec_ctx, m_video_codec_params) < 0) {
        throw std::runtime_error("failed to copy codec params to codec context");
    }

    // Initialize the AVCodecContext to use the given AVCodec.
    // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#ga11f785a188d7d9df71621001465b0f1d
    if (avcodec_open2(m_video_codec_ctx, m_video_codec, NULL) < 0) {
        throw std::runtime_error("failed to open codec through avcodec_open2");
    }

    // Allocate RGBA buffer
    m_rgba_frame = av_frame_alloc();
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_video_codec_ctx->width, m_video_codec_ctx->height, 1);

    m_video_buffer = static_cast<uint8_t*>(av_malloc(numBytes));

    av_image_fill_arrays(m_rgba_frame->data, m_rgba_frame->linesize, m_video_buffer, AV_PIX_FMT_RGBA, m_video_codec_ctx->width, m_video_codec_ctx->height, 1);

    m_video_sampler = sws_getContext(m_video_codec_ctx->width, m_video_codec_ctx->height, m_video_codec_ctx->pix_fmt, m_video_codec_ctx->width, m_video_codec_ctx->height, AV_PIX_FMT_RGBA,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);

    m_video_texture.create(m_video_codec_ctx->width, m_video_codec_ctx->height);
    m_video_texture.setSmooth(false);
    m_video_sprite.setTexture(m_video_texture);

    m_timebase = av_q2d(m_format_ctx->streams[m_video_stream_index]->time_base);

    m_frame = av_frame_alloc();
    if (m_frame == nullptr) {
        throw std::runtime_error("failed to allocate memory for AVFrame");
    }

    m_packet = av_packet_alloc();
    if (m_packet == nullptr) {
        throw std::runtime_error("failed to allocate memory for AVPacket");
    }
}

auto VideoPlayer::setup_audio() -> void {
    const AVChannelLayout output_channels = AV_CHANNEL_LAYOUT_STEREO;

    m_audio_codec_ctx = avcodec_alloc_context3(m_audio_codec);
    avcodec_parameters_to_context(m_audio_codec_ctx, m_format_ctx->streams[m_audio_stream_index]->codecpar);

    if (avcodec_open2(m_audio_codec_ctx, m_audio_codec, nullptr) < 0) {
        throw std::runtime_error("Failed to open audio codec");
    }

    if (swr_alloc_set_opts2(&m_audio_sampler,
                            &output_channels, AV_SAMPLE_FMT_S16, m_audio_codec_ctx->sample_rate,
                            &m_audio_codec_ctx->ch_layout, m_audio_codec_ctx->sample_fmt, m_audio_codec_ctx->sample_rate,
                            0, nullptr) != 0) {
        throw std::runtime_error("Error initialising audio resampler");
    }
    swr_init(m_audio_sampler);

    // Prepare audio stream

    m_audio_player.load(m_audio_codec_ctx);
    m_audio_player.play();
}

auto VideoPlayer::decode_video_packet() -> int {
    // Send the packet to the decoder
    int response = avcodec_send_packet(m_video_codec_ctx, m_packet);
    if (response < 0) {
        throw std::runtime_error("Error while sending a packet to the decoder: " + av_err2string(response));
    }

    // Process all frames returned by the decoder
    while (true) {
        response = avcodec_receive_frame(m_video_codec_ctx, m_frame);

        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            // No more frames available, either need more packets or EOF reached
            break;
        } else if (response < 0) {
            throw std::runtime_error("Error while receiving a frame from the decoder: " + av_err2string(response));
        }

        // If we receive a frame, we will process it
        if (response >= 0) {
            // Perform color space conversion
            sws_scale(m_video_sampler, m_frame->data, m_frame->linesize, 0, m_video_codec_ctx->height,
                      m_rgba_frame->data, m_rgba_frame->linesize);

            // Create and store the decoded frame with proper timestamp
            VideoFrame frame(m_video_codec_ctx->width, m_video_codec_ctx->height, m_frame->pts);

            // Copy data into the frame buffer
            std::memcpy(frame.data.data(), m_rgba_frame->data[0], m_video_codec_ctx->width * m_video_codec_ctx->height * 4);

            // Add to the frame queue
            m_decoded_video.push_back(std::move(frame));
        }
    }

    return 0;
}

auto VideoPlayer::decode_frame() -> int {
    int response = 0;

    response = av_read_frame(m_format_ctx, m_packet);
    if (response == AVERROR_EOF) {
        m_stream_eof = true;
    } else if (response < 0) {
        return response;
    }

    if (m_packet->stream_index == m_video_stream_index) {
        response = decode_video_packet();
        if (response < 0) {
            return response;
        }
        av_packet_unref(m_packet);
    } else if (m_packet->stream_index == m_audio_stream_index) {
        response = avcodec_send_packet(m_audio_codec_ctx, m_packet);

        if (response == 0) {

            while (avcodec_receive_frame(m_audio_codec_ctx, m_frame) == 0) {
                // Convert audio to 16-bit signed int
                uint8_t* buffer = nullptr;
                int buffer_size = av_samples_get_buffer_size(nullptr, 2,
                                                             m_frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
                buffer = static_cast<uint8_t*>(av_malloc(buffer_size));
                swr_convert(m_audio_sampler, &buffer, m_frame->nb_samples,
                            const_cast<const uint8_t**>(m_frame->data), m_frame->nb_samples);

                // Enqueue audio samples
                m_audio_player.enqueueSamples(reinterpret_cast<int16_t*>(buffer), buffer_size / sizeof(int16_t));

                av_free(buffer);
            }
        }

        av_packet_unref(m_packet);
    }

    return 0;
}

auto VideoPlayer::stop() -> void {
    m_audio_player.stopStream();
}

auto VideoPlayer::setPosition(int pos_x, int pos_y) -> void {
    m_video_sprite.setPosition(
            static_cast<float>(pos_x),
            static_cast<float>(pos_y));
}

auto VideoPlayer::setScale(float factor_x, float factor_y) -> void {
    m_video_sprite.setScale(factor_x, factor_y);
}