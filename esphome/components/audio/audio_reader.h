#pragma once

#ifdef USE_ESP_IDF

#include "audio.h"
#include "audio_transfer_buffer.h"

#include "esphome/core/ring_buffer.h"
#include "esphome/components/storage/storage.h"

#include "esp_err.h"

#include <esp_http_client.h>

namespace esphome {
namespace audio {

enum class AudioReaderState : uint8_t {
  READING = 0,  // More data is available to read
  FINISHED,     // All data has been read and transferred
  FAILED,       // Encountered an error
};

class AudioReader {
  /*
   * @brief Class that facilitates reading a raw audio file.
   * Files can be read from flash (stored in a AudioFile struct) or from an http source.
   * The file data is sent to a ring buffer sink.
   */
 public:
  /// @brief Constructs an AudioReader object.
  /// The transfer buffer isn't allocated here, but only if necessary (an http source) in the start function.
  /// @param buffer_size Transfer buffer size in bytes.
  AudioReader(size_t buffer_size) : buffer_size_(buffer_size) {}
  ~AudioReader();

  /// @brief Adds a sink ring buffer for audio data. Takes ownership of the ring buffer in a shared_ptr
  /// @param output_ring_buffer weak_ptr of a shared_ptr of the sink ring buffer to transfer ownership
  /// @return  ESP_OK if successful, ESP_ERR_INVALID_STATE otherwise
  esp_err_t add_sink(const std::weak_ptr<RingBuffer> &output_ring_buffer);

  /// @brief Starts reading an audio file from any storage source.
  /// @param uri the storage path to the file
  /// @param file_type AudioFileType variable passed-by-reference indicating the type of file being read.
  /// @return ESP_OK if successful, an ESP_ERR* code otherwise.
  esp_err_t start(const std::string &uri, AudioFileType &file_type);

  /// @brief Reads new file data from the source and sends to the ring buffer sink.
  /// @return AudioReaderState
  AudioReaderState read();

 protected:
  storage::StorageClient storage_client_;

  /// @brief Determines the audio file type from the http header's Content-Type key
  /// @param content_type string with the Content-Type key
  /// @return AudioFileType of the url, if it can be determined. If not, return AudioFileType::NONE.
  static AudioFileType get_audio_type(const char *content_type);

  std::shared_ptr<RingBuffer> file_ring_buffer_;

  size_t buffer_size_;

  AudioFileType audio_file_type_{AudioFileType::NONE};
};
}  // namespace audio
}  // namespace esphome

#endif
