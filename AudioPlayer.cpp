#include "AudioPlayer.hpp"

const size_t AudioPlayer::m_buffer_size = 4096;

void AudioPlayer::load(AVCodecContext* codec_ctx) {
    sf::SoundStream::initialize(2, codec_ctx->sample_rate);
}

void AudioPlayer::enqueueSamples(const int16_t* samples, std::size_t sample_count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_audio_queue.insert(m_audio_queue.end(), samples, samples + sample_count);
    m_cv.notify_one();
}

bool AudioPlayer::onGetData(Chunk& data) {
    std::unique_lock<std::mutex> lock(m_mutex);

    // Wait until there's audio data available
    m_cv.wait(lock,
              [this]() -> bool {
                  return !m_audio_queue.empty() || m_stopping;
              });

    // Check if the audio queue is empty and the stream has ended
    if (m_audio_queue.empty() || m_stopping) {
        return false; // Indicate that the playback should stop
    }

    // Provide audio samples to the sound stream
    m_buffer.assign(m_audio_queue.begin(), m_audio_queue.begin() + std::min(m_audio_queue.size(), m_buffer_size));
    m_audio_queue.erase(m_audio_queue.begin(), m_audio_queue.begin() + m_buffer.size());

    data.samples     = m_buffer.data();
    data.sampleCount = m_buffer.size();

    return true;
}

void AudioPlayer::stopStream() {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_cv.notify_all();
    stop();
}