#include "ftp_server.h"
#include <sys/select.h>
#include "path.h"

#include <esp_timer.h>
#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <optional>

#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP_IDF
#include "esp_idf_version.h"
#endif

namespace esphome {
namespace ftp_server {

static const char *TAG = "ftp_server";

FTPServer::FTPServer(web_server_base::WebServerBase *base) : base_(base) {}

void FTPServer::setup() { this->base_->add_handler(this); }

void FTPServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletation Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
}

void FTPServer::loop() {}

bool FTPServer::canHandle(AsyncWebServerRequest *request) const {
  ESP_LOGD(TAG, "can handle %s %u", request->url().c_str(),
           str_startswith(std::string(request->url().c_str()), this->build_prefix()));
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void FTPServer::handleRequest(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "%s", request->url().c_str());
  if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
    switch (request->method()) {
      case HTTP_GET:
        this->handle_get(request);
        break;
      // case HTTP_HEAD:
      //   this->handle_get(request, true);
      //   break;
      case HTTP_DELETE:
        this->handle_delete(request);
        break;
      case HTTP_POST:
        this->handle_upload(request);

        break;
      default:
        break;
    }
  }
}

void FTPServer::handleUpload(AsyncWebServerRequest *request) {
  if (!this->upload_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  ESP_LOGV(TAG, "Upload requested for url %s, path is %s", request->url().c_str(), path.c_str());

  if (index == 0 && !this->storage_client_.get_file_info(path).is_directory) {
    ESP_LOGV(TAG, "It's not a folder");
    auto response = request->beginResponse(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }
  std::string file_name(filename.c_str());
  if (index == 0) {
    ESP_LOGD(TAG, "uploading file %s to %s", file_name.c_str(), path.c_str());
    this->storage_client_.set_file(Path::join(path, file_name).c_str());
    this->storage_client_.write_array(data, len);
    return;
  }
  this->storage_client_.append_array(data, len);
  if (final) {
    auto response = request->beginResponse(201, "text/html", "upload success");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }
}

void FTPServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }

void FTPServer::set_root_path(std::string const &path) { this->root_path_ = path; }

void FTPServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }

void FTPServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }

void FTPServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

void FTPServer::handle_get(AsyncWebServerRequest *request) {
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  storage::FileInfo path_info = this->storage_client_.get_file_info(path);
  if (path_info.is_directory) {
    this->handle_index(request, path_info);
  } else {
    if (path_info.size > 0) {
      this->handle_download(request, path_info);
    }
  }
}

void FTPServer::write_row(AsyncResponseStream *response, storage::FileInfo const &info) const {
  std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
  std::string file_name = Path::file_name(info.path);
  response->print("<tr><td>");
  response->print("<a href=\"");
  response->print(uri.c_str());
  response->print("\">");
  response->print(file_name.c_str());
  response->print("</a>");
  response->print("</td><td>");

  if (info.is_directory) {
    response->print("Folder");
  } else {
    response->print("<span class=\"file-type\">");
    response->print(Path::file_type(file_name).c_str());
    response->print("</span>");
  }
  response->print("</td><td>");
  if (!info.is_directory) {
    response->print(std::to_string(info.size));
  }
  response->print("</td><td><div class=\"file-actions\">");
  if (!info.is_directory) {
    if (this->download_enabled_) {
      response->printf(
          R"(<form method="get" action="%s"><input type="hidden" name="download" value="true" /> <button type="submit">Download</button></form>)",
          uri.c_str());
    }
    if (this->deletion_enabled_) {
      response->print("<button onClick=\"delete_file('");
      response->print(uri.c_str());
      response->print("')\">Delete</button>");
    }
  }
  response->print("<div></td></tr>");
}

void FTPServer::handle_index(AsyncWebServerRequest *request, storage::FileInfo const &path) const {
  ESP_LOGVV(TAG, "handling index");
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F(R"(
  <!DOCTYPE html>
  <html lang=\"en\">
  <head>
    <meta charset=UTF-8>
    <meta name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">
    <title>ESP Home Files</title>
    <style>
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      margin: 0;
      padding: 2rem;
      background: #f5f5f7;
      color: #1d1d1f;
    }
    h1 {
      color: #0066cc;
      margin-bottom: 1.5rem;
      display: flex;
      align-items: center;
      gap: 1rem;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      background: white;
      border-radius: 12px;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
      padding: 2rem;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 1.5rem;
    }
    th, td {
      padding: 12px;
      text-align: left;
      border-bottom: 1px solid #e0e0e0;
    }
    th {
      background: #f8f9fa;
      font-weight: 500;
    }
    .file-actions {
      display: flex;
      gap: 8px;
    }
    button {
      padding: 6px 12px;
      border: none;
      border-radius: 6px;
      background: #0066cc;
      color: white;
      cursor: pointer;
      transition: background 0.2s;
    }
    button:hover {
      background: #0052a3;
    }
    .upload-form {
      margin-bottom: 2rem;
      padding: 1rem;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .upload-form input[type="file"] {
      margin-right: 1rem;
    }
    .breadcrumb {
      margin-bottom: 1.5rem;
      font-size: 0.9rem;
      color: #666;
    }
    .breadcrumb a {
      color: #0066cc;
      text-decoration: none;
    }
    .breadcrumb a:hover {
      text-decoration: underline;
    }
    .breadcrumb a:not(:last-child)::after {
      display: inline-block;
      margin: 0 .25rem;
      content: ">";
    }
    .folder {
      color: #0066cc;
      font-weight: 500;
    }
    .file-type {
      color: #666;
      font-size: 0.9rem;
    }
    .folder-icon {
      width: 20px;
      height: 20px;
      margin-right: 8px;
      vertical-align: middle;
    }
    .header-actions {
      display: flex;
      align-items: center;
      gap: 1rem;
    }
    .header-actions button {
      background: #4CAF50;
    }
    .header-actions button:hover {
      background: #45a049;
    }
  </style>
  </head>
  <body>
  <div class="container">
    <div class="header-actions">
      <h1>ESP Home Files</h1>
      <button onclick="window.location.href='/'">Go to web server</button>
    </div>
    <div class="breadcrumb">
      <a href="/">Home</a>)"));

  std::string current_path = "/";
  std::string relative_path = Path::join(this->url_prefix_, Path::remove_root_path(path.path, this->root_path_));
  std::vector<std::string> parts = Path::split_path(relative_path);
  for (std::string const &part : parts) {
    if (!part.empty()) {
      current_path = Path::join(current_path, part);
      response->print("<a href=\"");
      response->print(current_path.c_str());
      response->print("\">");
      response->print(part.c_str());
      response->print("</a>");
    }
  }
  response->print(F("</div>"));

  if (this->upload_enabled_)
    response->print(F("<div class=\"upload-form\"><form method=\"POST\" enctype=\""
#ifdef USE_ESP_IDF
                      "application/x-www-form-urlencoded"
#else
                      "multipart/form-data"
#endif
                      "\">"
                      "<input type=\"file\" name=\"file\"><input type=\"submit\" value=\"upload\"></form></div>"));

  response->print(F("<table><thead><tr>"
                    "<th>Name</th>"
                    "<th>Type</th>"
                    "<th>Size</th>"
                    "<th>Actions</th>"
                    "</tr></thead><tbody>"));

  auto entries = this->storage_client_.list_directory(path.path);
  for (auto const &entry : entries)
    write_row(response, entry);

  response->print(F("</tbody></table>"
                    "<script>"
                    "function delete_file(path) {fetch(path, {method: \"DELETE\"});}"
                    "function download_file(path, filename) {"
                    "fetch(path).then(response => response.blob())"
                    ".then(blob => {"
                    "const link = document.createElement('a');"
                    "link.href = URL.createObjectURL(blob);"
                    "link.download = filename;"
                    "link.click();"
                    "}).catch(console.error);"
                    "} "
                    "</script>"
                    "</body></html>"));

  request->send(response);
}

void FTPServer::handle_download(AsyncWebServerRequest *request, storage::FileInfo const &path) {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  const auto open_start_time = esp_timer_get_time();
  auto file = storage_client_.get_file_info(path);
  ESP_LOGV(TAG, "open(%s) (%llu us)", path.c_str(), esp_timer_get_time() - open_start_time);
  if (!(file.size)) {
    request->send(401, "application/json", "{ \"error\": \"failed to open file\" }");
    return;
  }
  storage_client_.set_file(file);

  const auto download = [&] {
    const auto param = request->getParam("download");
    return param && param->value() == "true";
  }();

  request->send(request->beginResponse(file, path, download));
}

void FTPServer::handle_delete(AsyncWebServerRequest *request) {
  if (!this->deletion_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file deletion is disabled\" }");
    return;
  }
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  if (this->storage_client_.get_file_info(path).is_directory) {
    request->send(401, "application/json", "{ \"error\": \"cannot delete a directory\" }");
    return;
  }
  this->storage_client_.set_file(path);
  this->storage_client_.delete_current_file();
  request->send(204, "application/json", "{}");
  return;

  // request->send(401, "application/json", "{ \"error\": \"failed to delete file\" }");
}

std::string FTPServer::build_prefix() const {
  if (this->url_prefix_.length() == 0 || this->url_prefix_.at(0) != '/')
    return "/" + this->url_prefix_;
  return this->url_prefix_;
}

std::string FTPServer::extract_path_from_url(std::string const &url) const {
  std::string prefix = this->build_prefix();
  return url.substr(prefix.size(), url.size() - prefix.size());
}

std::string FTPServer::build_absolute_path(std::string relative_path) const {
  if (relative_path.size() == 0)
    return this->root_path_;

  std::string absolute = Path::join(this->root_path_, relative_path);
  return absolute;
}

}  // namespace ftp_server
}  // namespace esphome
