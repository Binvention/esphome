#include "audio_reader.h"

#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

namespace esphome {
namespace audio {

static const uint32_t READ_WRITE_TIMEOUT_MS = 20;

AudioReader::~AudioReader() {}

esp_err_t AudioReader::add_sink(const std::weak_ptr<RingBuffer> &output_ring_buffer) {
  // we'll leave buffering to the storage and audio components
  this->file_ring_buffer_ = output_ring_buffer.lock();
  return ESP_OK;
}

esp_err_t AudioReader::start(const std::string &uri, AudioFileType &file_type) {
  if (uri.empty()) {
    return ESP_ERR_INVALID_ARG;
  }
  storage_client_.set_file(uri);
  auto file_info = storage_client_.get_file_info(uri);
  ESP_LOGVV("AudioReader", "Starting to play file %s of size %0d", file_info.path.c_str(), file_info.size);
  if (file_type == AudioFileType::NONE) {
    if (uri.find(".wav") > 0) {
      this->audio_file_type_ = AudioFileType::WAV;
      file_type = AudioFileType::WAV;
      ESP_LOGVV("AudioReader", "File Type Detected WAV");
    } else if (uri.find(".flac") > 0) {
      this->audio_file_type_ = AudioFileType::FLAC;
      ESP_LOGVV("AudioReader", "File Type Detected FLAC");
      file_type = AudioFileType::FLAC;

    } else if (uri.find(".mp3") > 0) {
      this->audio_file_type_ = AudioFileType::MP3;
      file_type = AudioFileType::MP3;
      ESP_LOGVV("AudioReader", "File Type Detected MP3");
    } else {
      ESP_LOGE("AudioReader", "Unable to determine file type");
    }
  } else {
    switch (file_type) {
      case AudioFileType::WAV:
        ESP_LOGVV("AudioReader", "Using File Type WAV");
        break;
      case AudioFileType::FLAC:
        ESP_LOGVV("AudioReader", "Using File Type FLAC");
        break;
      case AudioFileType::MP3:
        ESP_LOGVV("AudioReader", "Using File Type MP3");
        break;
    }
    this->audio_file_type_ = file_type;
  }

  return ESP_OK;
}

AudioFileType AudioReader::get_audio_type(const char *content_type) {
#ifdef USE_AUDIO_MP3_SUPPORT
  if (strcasecmp(content_type, "mp3") == 0 || strcasecmp(content_type, "audio/mp3") == 0 ||
      strcasecmp(content_type, "audio/mpeg") == 0) {
    return AudioFileType::MP3;
  }
#endif
  if (strcasecmp(content_type, "audio/wav") == 0) {
    return AudioFileType::WAV;
  }
#ifdef USE_AUDIO_FLAC_SUPPORT
  if (strcasecmp(content_type, "audio/flac") == 0 || strcasecmp(content_type, "audio/x-flac") == 0) {
    return AudioFileType::FLAC;
  }
#endif
  return AudioFileType::NONE;
}

AudioReaderState AudioReader::read() {
  uint8_t temp[4];
  int num_bytes;
  int available = this->file_ring_buffer_->available();
  do {
    num_bytes = this->storage_client_.read_array(&(temp[0]), 4);
    available -= num_bytes;
    if (num_bytes) {
      size_t bytes_written =
          this->file_ring_buffer_->write_without_replacement(temp, num_bytes, pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS));
      if (bytes_written != num_bytes) {
        ESP_LOGE("Audio Reader", "error occurred while writing to file buffer");
      }
    }
  } while (num_bytes != 0 && available >= 4);
  if (num_bytes) {
    return AudioReaderState::READING;
  }
  return AudioReaderState::FINISHED;
}

}  // namespace audio
}  // namespace esphome

#endif
