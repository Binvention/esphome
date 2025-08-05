#pragma once
#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/storage/storage.h"

namespace esphome {
namespace ftp_server {

class FTPServer : public Component, public AsyncWebHandler {
 public:
  FTPServer(web_server_base::WebServerBase *);
  void setup() override;
  void dump_config() override;
  bool canHandle(AsyncWebServerRequest *request);
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override;
  bool isRequestHandlerTrivial() { return false; }

  void set_deletion_enabled(bool);
  void set_download_enabled(bool);
  void set_upload_enabled(bool);
  void set_url_prefix(const std::string &prefix);
  void set_root_path(const std::string &root);

 protected:
  web_server_base::WebServerBase *base_;
  storage::StorageClient storage_client_;

  bool deletion_enabled_;
  bool download_enabled_;
  bool upload_enabled_;
  std::string prefix_;
  std::string root_;

  std::string build_prefix() const;
  std::string extract_path_from_url(std::string const &) const;
  std::string build_absolute_path(std::string) const;
  void write_row(AsyncResponseStream *response, storage::FileInfo const &info) const;
  void handle_index(AsyncWebServerRequest *, std::string const &) const;
  void handle_get(AsyncWebServerRequest *) const;
  void handle_delete(AsyncWebServerRequest *);
  void handle_download(AsyncWebServerRequest *, std::string const &) const;
};

struct Path {
  static constexpr char separator = '/';

  /* Return the name of the file */
  static std::string file_name(std::string const &);

  /* Is the path an absolute path? */
  static bool is_absolute(std::string const &);

  /* Does the path have a trailing slash? */
  static bool trailing_slash(std::string const &);

  /* Join two path */
  static std::string join(std::string const &, std::string const &);

  static std::string remove_root_path(std::string path, std::string const &root);

  static std::vector<std::string> split_path(std::string path);

  static std::string extension(std::string const &);

  static std::string file_type(std::string const &);

  static std::string mime_type(std::string const &);
};

}  // namespace ftp_server
}  // namespace esphome
