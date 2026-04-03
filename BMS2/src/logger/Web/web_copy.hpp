#ifndef WEB_HPP
#define WEB_HPP

#include "battery/modes.hpp"
#include "esp_littlefs.h"
#include <string.h>
#include "wchar.h"
#include "esp_http_server.h"
#include "string"
#include "vector"

namespace web
{
    // cache LogLine from t_logger and pull data from it

#include "logger/Web/index.html" // const char INDEX_HTML[] PROGMEM
    esp_err_t setupServer();

    bool checkCorsPreflight();

    void handleName(httpd_req_t *req);

    void handleFullShutdown(httpd_req_t *req);

    void hangleForceDischargeEnable(httpd_req_t *req);
    void handleForceDischargeDisable(httpd_req_t *req);

    void handleLogDownload(httpd_req_t *req);
    void handleLogDelete(httpd_req_t *req);

    void handleParameters(httpd_req_t *req);
    void handleAcknowledge(httpd_req_t *req);

    void handleReadCells(httpd_req_t *req);
    void handleData(httpd_req_t *req);

    void handleIdle(httpd_req_t *req);
    void handleMonitor(httpd_req_t *req);
    void handleBalancing(httpd_req_t *req);

    void handleCanMode(httpd_req_t *req);

    void handleState(httpd_req_t *req);
    const char *printState(modes::Mode state);

    bool handleFileRead(char *path);
    const char *getContentType(const std::string &filename);

    void saveData();
    char *generateLogLine();
    void writeToFile(FILE &file);

    // Send/stream the HTML file that is embedded in the code
    void sendDefaultHTML();
    // Always send the embedded HTML file
    void handleDefault(httpd_req_t *req);
    // Prefer sending the HTML file from SPIFFS, if it exists otherwise send the embedded HTML
    void handleRoot(httpd_req_t *req);
    // Upload a new HTML file to SPIFFS
    void handleFrontend(httpd_req_t *req);

    void handleFileUpload(httpd_req_t *req);

    void handleNotFound(httpd_req_t *req, httpd_err_code_t err);
    esp_err_t send_text(httpd_req_t *req, const char *status, const char *type, const std::string &body);

    bool stream_file(httpd_req_t *req, const char *path, const char *content_type);
    bool update_parameter(const std::string &legacy_key, const std::string &raw_value, std::string &out_value);
    std::string get_path_arg(httpd_req_t *req, const std::string &prefix, size_t index);
    std::string to_fs_path(const std::string &path);
    bool ends_with(const std::string &value, const std::string &suffix);
    std::string content_type_header(httpd_req_t *req);
    bool parse_multipart_file(const std::vector<uint8_t> &body,
                              const std::string &content_type,
                              const std::string &field_name,
                              std::string &filename,
                              const uint8_t *&file_start,
                              size_t &file_size);
    esp_err_t read_request_body(httpd_req_t *req, std::vector<uint8_t> &out);
} // namespace web

#endif // WEB_HPP