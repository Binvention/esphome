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

#ifdef USE_ESP_IDF
esp_err_t httpd_send_all(httpd_req_t *r, const char *buffer, size_t len) {
  while (len > 0) {
    const auto sent = httpd_send(r, buffer, len);
    switch (sent) {
      case HTTPD_SOCK_ERR_INVALID:
      case HTTPD_SOCK_ERR_TIMEOUT:
      case HTTPD_SOCK_ERR_FAIL:
        ESP_LOGE(TAG, "httpd_send failed: %s", esp_err_to_name(sent));
        return sent;
    }
    ESP_LOGVV(TAG, "%d bytes written to httpd", sent);
    len -= sent;
    buffer += sent;
  }
  return ESP_OK;
}

esp_err_t httpd_printf(httpd_req_t *r, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  const int length = vsnprintf(nullptr, 0, fmt, args);
  va_end(args);

  std::string str;
  str.resize(length);

  va_start(args, fmt);
  vsnprintf(&str[0], length + 1, fmt, args);
  va_end(args);

  return httpd_send_all(r, str.data(), str.size());
}

esp_err_t httpd_print(httpd_req_t *r, const char *str) { return httpd_send_all(r, str, strlen(str)); }

esp_err_t esp_http_server_dispatch_event(int32_t event_id, const void *event_data, size_t event_data_size) {
  ESP_LOGVV(TAG, "Dispatching event %ld", event_id);
  esp_err_t err = esp_event_post(ESP_HTTP_SERVER_EVENT, event_id, event_data, event_data_size, ULONG_MAX);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to post esp_http_server event: %s", esp_err_to_name(err));
  }
  return err;
}

template<typename T> std::istream &operator>>(std::istream &is, std::optional<T> &obj) {
  if (T result; is >> result)
    obj = result;
  else
    obj = {};
  return is;
}

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
esp_err_t httpd_resp_send_custom_err(httpd_req_t *req, const char *status, const char *msg) {
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
  return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
}
#endif
#endif  // USE_ESP_IDF

FTPServer::FTPServer(web_server_base::WebServerBase *base) : base_(base) {}

esp_err_t get_handler(httpd_req_t *req) {
  /* Send a simple response */
  const char resp[] = "URI GET Response";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

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

void FTPServer::loop() {
#ifdef USE_ESP_IDF
  LockGuard lg(this->downloadResponses_mutex_);

  for (auto i = this->downloadResponses_.begin(); i != this->downloadResponses_.end();) {
    auto &&response = *i;
    if (response.scheduled)
      continue;
    if (response.completed || response.failed) {
      const auto err = httpd_req_async_handler_complete(response.req());
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_req_async_handler_complete failed: %s", esp_err_to_name(err));
      }
      i = downloadResponses_.erase(i);
      ESP_LOGV(TAG, "download response deleted");
    } else {
      ++i;
    }
  }

  storage::FileInfo read_file;
  storage::FileInfo write_file;
  timeval tv = {.tv_sec = 0, .tv_usec = 0};

  for (auto &&response : this->downloadResponses_) {
    if (response.scheduled) {
      ESP_LOGVV(TAG, "Response is scheduled. Skiped");
      continue;
    }

    // write_file = response.file();
    if (!(response.buffer.free() > 0) && !response.read_done) {
      read_file = response.file();
      ESP_LOGVV(TAG, "Add read selector %d", read_file.read_offset);
    } else {
      ESP_LOGVV(TAG, "Buffer full: %i, Read done: %i. Skipp adding read selector.", response.buffer.free(),
                response.read_done);
    }
  }

  if (read_file.path.empty() && write_file.path.empty()) {
    return;
  }
  // schedule transfer
  for (auto &&response : this->downloadResponses_) {
    if (response.scheduled)
      continue;

    const auto to_read = response.buffer.free();
    if (to_read != 0 && !response.read_done) {
      const auto start_time = esp_timer_get_time();
      this->storage_client_.set_file(read_file);
      uint8_t temp_array[to_read];
      const auto read_bytes = this->storage_client_.read_array(temp_array, to_read);
      response.buffer.write_without_replacement(temp_array, read_bytes, pdMS_TO_TICKS(20));
      // read(response.file_fd(), response.buffer.write_ptr(), to_read);

      ESP_LOGVV(TAG, "read %d bytes from %s", read_bytes, read_file.path);
      if (read_bytes == 0) {
        ESP_LOGV(TAG, "reading is done");
        response.read_done = true;
      } else {
        response.bytes_to_send -= read_bytes;
        if (response.bytes_to_send == 0) {
          ESP_LOGV(TAG, "reading part is done");
          response.read_done = true;
        }
      }
      // ESP_LOGVV(TAG, "buffer: read: %p + %d, write: %p + %d", response.buffer.read_ptr(),
      //           response.buffer.bytes_to_read(), response.buffer.write_ptr(), response.buffer.bytes_to_write());
    } else {
      ESP_LOGVV(TAG, "Read skip:  available buffer: %d,  not done: %d, to read: %d", response.buffer.free(),
                !response.read_done, to_read);
    }

    if ((response.read_done || response.buffer.available() > 0)) {
      response.scheduled = true;
      const auto err = httpd_queue_work(
          response.req()->handle,
          [](void *p) {
            auto &response = *reinterpret_cast<DownloadResponse *>(p);
            const auto to_send = response.buffer.available();
            if (to_send) {
              const auto start_time = esp_timer_get_time();
              uint8_t temp_array[to_send];
              int num_ready = response.buffer.read(temp_array, to_send, 0);
              const auto sent = httpd_send(response.req(), (char *) &(temp_array[0]), num_ready);
              // const auto sent = httpd_socket_send(response.req()->handle, response.resp_fd(),
              // response.buffer.read_ptr(), to_send, O_NONBLOCK);

              if (sent < 0) {
                if (sent == -3) {
                  // looks like will block
                  // return; // killed by watchdog
                }
                ESP_LOGE(TAG, "httpd_send failed: %s", esp_err_to_name(sent));
                response.failed = true;
              } else {
                if (sent != num_ready) {
                  ESP_LOGE(TAG, "wasn't able to send all the bytes read from file. Read: %0d\tSent: %0d", num_ready,
                           sent);
                }
                response.bytes_sent += sent;
              }
            }
            // ESP_LOGVV(TAG, "buffer: read: %p + %d, write: %p + %d", response.buffer.read_ptr(),
            //           response.buffer.bytes_to_read(), response.buffer.write_ptr(),
            //           response.buffer.bytes_to_write());
            if (!response.failed && response.read_done && response.buffer.available()) {
              ESP_LOGV(TAG, "Response completed");
              esp_http_server_event_data evt_data = {
                  .fd = response.resp_fd(),
                  .data_len = response.bytes_sent,
              };
              esp_http_server_dispatch_event(HTTP_SERVER_EVENT_SENT_DATA, &evt_data,
                                             sizeof(esp_http_server_event_data));
              response.completed = true;
            }
            response.scheduled = false;
          },
          &response);
      if (err != ESP_OK) {
        response.scheduled = false;
        ESP_LOGE(TAG, "httpd_queue_work failed: %s", esp_err_to_name(err));
      }
    } else {
      // ESP_LOGVV(TAG, "Send skip: is_set: %ld, read done: %d, not empty: %d", FD_ISSET(response.resp_fd(), &wfds),
      //           response.read_done, !response.buffer.available());
    }
  }
#endif  // USE_ESP_IDF
}

bool FTPServer::canHandle(AsyncWebServerRequest *request) {
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
      default:
        break;
    }
  }
}

void FTPServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                             size_t len, bool final) {
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

  if (!this->storage_client_.get_file_info(path).is_directory) {
    this->handle_download(request, path);
    return;
  }

  this->handle_index(request, path);
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

void FTPServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  ESP_LOGVV(TAG, "handling index");
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F(R"(
  <!DOCTYPE html>
  <html lang=\"en\">
  <head>
    <meta charset=UTF-8>
    <meta name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">
    <title>SD Card Files</title>
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
      <h1>SD Card Files</h1>
      <button onclick="window.location.href='/'">Go to web server</button>
    </div>
    <div class="breadcrumb">
      <a href="/">Home</a>)"));

  std::string current_path = "/";
  std::string relative_path = Path::join(this->url_prefix_, Path::remove_root_path(path, this->root_path_));
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

  auto entries = this->storage_client_.list_directory(path);
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

void FTPServer::handle_download(AsyncWebServerRequest *request, std::string const &path) {
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

#ifdef USE_ESP_IDF
  const auto size_start_time = esp_timer_get_time();
  const auto data_len = file.size;

  std::optional<size_t> range_begin;
  std::optional<size_t> range_end;
  if (const auto range_header = request->get_header("Range")) {
    ESP_LOGV(TAG, "Range header: %s", range_header->c_str());
    std::istringstream stream(*range_header);
    const std::string prefix = "bytes=";
    for (auto &&c : prefix) {
      if (stream.get() != c) {
        httpd_resp_send_err(*request, HTTPD_400_BAD_REQUEST, "Only bytes range supported");
        return;
      }
    }

    for (; !stream.eof();) {
      ESP_LOGVV(TAG, "processing range part");
      std::optional<size_t> first;
      std::optional<size_t> last;
      stream >> std::ws;
      const auto first_byte = stream.peek();
      if (first_byte == '-') {
        stream.get();
        stream >> std::ws >> last >> std::ws;
        if (last) {
          ESP_LOGVV(TAG, "range part from end: %d", *last);
        } else {
          ESP_LOGVV(TAG, "range part from end: none");
        }

        if (data_len < *last) {
          httpd_resp_send_custom_err(*request, "416 Range Not Satisfiable", "reverse range too long");
          return;
        }
        first = data_len - *last;
        last.reset();
      } else {
        stream >> first >> std::ws;
        if (first) {
          ESP_LOGVV(TAG, "range part first: %d", *first);
        } else {
          ESP_LOGVV(TAG, "range part first: none");
        }
        const auto c = stream.get();
        switch (c) {
          case '-':
            stream >> std::ws >> last >> std::ws;
            if (last) {
              ESP_LOGVV(TAG, "range part last: %d", *last);
            } else {
              ESP_LOGVV(TAG, "range part last: none");
            }
            break;
          case ',':
            ESP_LOGVV(TAG, "range part: ,");
            break;
          case EOF:
            ESP_LOGVV(TAG, "range part: EOF");
            break;
          default: {
            std::string err = "Invalid Range header: '-' or ',' expected, got ";
            err += c;
            httpd_resp_send_err(*request, HTTPD_400_BAD_REQUEST, err.c_str());
            return;
          }
        }
      }

      if (!first && !last) {
        httpd_resp_send_err(*request, HTTPD_400_BAD_REQUEST, "Invalid Range header: at least one number expected");
        return;
      }
      const auto end = last ? *last + 1 : data_len;
      if (*first > end) {
        httpd_resp_send_err(*request, HTTPD_400_BAD_REQUEST, "Invalid Range header: last greater then first");
        return;
      }
      range_begin = range_begin ? std::min(*first, *range_begin) : *first;
      range_end = range_end ? std::max(*range_end, end) : end;

      if (stream.eof())
        break;

      if (stream.get() != ',') {
        httpd_resp_send_err(*request, HTTPD_400_BAD_REQUEST, "Invalid Range header: comma expected");
        return;
      }
    }

    if (!range_begin)
      range_begin = 0;
    if (!range_end)
      range_end = data_len;

    if (*range_begin == 0 && *range_end == data_len) {
      range_begin.reset();
      range_end.reset();
    }
  }

  ESP_LOGV(TAG, "file_size(%s) (%llu us)", path.c_str(), esp_timer_get_time() - size_start_time);
  const auto header_start_time = esp_timer_get_time();
  if (range_begin && range_end && data_len > 0) {
    if (*range_end > data_len) {
      httpd_resp_send_custom_err(*request, "416 Range Not Satisfiable", "file is shorter then requested");
      return;
    }
    if (range_begin && *range_begin != 0) {
      file.read_offset = *range_begin;
      storage_client_.set_file(file);
    }
    httpd_print(*request, "HTTP/1.1 206 Partial Content\r\n");
    httpd_printf(*request, "Content-Type: %s\r\n", Path::mime_type(path).c_str());
    httpd_printf(*request,
                 "Content-Length: %d\r\n"
                 "Content-Range: bytes %d-%d/%d\r\n",
                 *range_end - *range_begin, *range_begin, *range_end - 1, data_len);
  } else {
    httpd_printf(*request,
                 "HTTP/1.1 %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %d\r\n",
                 HTTPD_200, Path::mime_type(path).c_str(), data_len);
  }

  // struct stat file_stat;
  // if (-1 == fstat(file.fd(), &file_stat) && file_stat.st_mtime) {
  //   ESP_LOGE(TAG, "fstat call failed: %s", strerror(errno));
  // } else if (file_stat.st_mtime == 0) {
  //   ESP_LOGI(TAG, "st_mtime is 0. Not sending.");
  // } else {
  //   time_t mtime = file_stat.st_mtime;
  //   struct tm *tm_info = gmtime(&mtime);
  //   char buffer[128];
  //   if (0 != strftime(buffer, sizeof(buffer), "Last-Modified: %a, %d %b %Y %H:%M:%S GMT\r\n", tm_info))
  //     httpd_print(*request, buffer);
  // }

  httpd_print(*request, "Cache-Control: no-cache\r\n");
  httpd_print(*request, "Accept-Ranges: bytes\r\n");
  if (download) {
    httpd_printf(*request, "Content-Disposition: attachment; filename=\"%s\";\r\n", Path::file_name(path).c_str());
  }
  httpd_print(*request, "\r\n");
  ESP_LOGV(TAG, "header sent (%llu us)", esp_timer_get_time() - header_start_time);

  auto fd = httpd_req_to_sockfd(*request);
  esp_http_server_dispatch_event(HTTP_SERVER_EVENT_HEADERS_SENT, &fd, sizeof(int));

  httpd_req_t *async_req;
  const auto err = httpd_req_async_handler_begin(*request, &async_req);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "httpd_req_async_handler_begin failed: %s", esp_err_to_name(err));
  } else {
    // if (add_noblock_file) {
    //   int file_flags = fcntl(file.fd(), F_GETFL, 0);
    //   fcntl(file.fd(), F_SETFL, file_flags | O_NONBLOCK);
    // }

    if (add_noblock_response) {
      auto resp_fd = httpd_req_to_sockfd(*request);
      int resp_flags = fcntl(resp_fd, F_GETFL, 0);
      fcntl(resp_fd, F_SETFL, resp_flags | O_NONBLOCK);
    }

    LockGuard lg(this->downloadResponses_mutex_);
    const auto to_send = range_end ? (*range_end - *range_begin) : data_len;
    this->downloadResponses_.emplace_back(std::move(file), async_req, to_send);
  }
#else   // USE_ESP_IDF
  request->send(request->beginResponse(file, path, download));
#endif  // USE_ESP_IDF
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
