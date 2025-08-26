#pragma once

#include <array>
#include <cstddef>
#include <list>

#include <fcntl.h>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/ring_buffer.h"
#include "esphome/components/storage/storage.h"

namespace esphome {
namespace ftp_server {

constexpr size_t max_read = 1024;
constexpr size_t max_send = 1024;
constexpr size_t download_buffer = max_read * 4;
constexpr bool add_noblock_file = true;
constexpr bool add_noblock_response = true;

class FTPServer : public Component, public AsyncWebHandler {
 public:
  FTPServer(web_server_base::WebServerBase *);
  void setup() override;
  void dump_config() override;
  void loop() override;

  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override;
  bool isRequestHandlerTrivial() override { return false; }

  void set_url_prefix(std::string const &);
  void set_root_path(std::string const &);
  void set_deletion_enabled(bool);
  void set_download_enabled(bool);
  void set_upload_enabled(bool);

 protected:
  web_server_base::WebServerBase *base_;
  storage::StorageClient storage_client_;

#ifdef USE_ESP_IDF
  struct DownloadResponse {
    RingBuffer buffer;

    DownloadResponse(storage::FileInfo file, httpd_req_t *req, size_t len)
        : file_(file), req_(req), bytes_to_send(len) {}
    httpd_req_t *req() const { return req_; }
    storage::FileInfo &file() { return file_; }

   protected:
    storage::FileInfo file_;
    httpd_req_t *req_;

   public:
    int bytes_sent = 0;
    int bytes_to_send = 0;
    bool scheduled = false;
    bool read_done = false;
    bool completed = false;
    bool failed = false;
  };
  mutable std::list<DownloadResponse> downloadResponses_;
  mutable Mutex downloadResponses_mutex_;
#endif  // USE_ESP_IDF

  std::string url_prefix_;
  std::string root_path_;
  bool deletion_enabled_;
  bool download_enabled_;
  bool upload_enabled_;

  std::string build_prefix() const;
  std::string extract_path_from_url(std::string const &) const;
  std::string build_absolute_path(std::string) const;
  void write_row(AsyncResponseStream *response, storage::FileInfo const &info) const;
  void handle_index(AsyncWebServerRequest *, std::string const &) const;
  void handle_get(AsyncWebServerRequest *) const;
  void handle_delete(AsyncWebServerRequest *);
  void handle_download(AsyncWebServerRequest *, std::string const &) const;
};

}  // namespace ftp_server
}  // namespace esphome
