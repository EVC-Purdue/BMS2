#ifndef WEB_HPP
#define WEB_HPP

#include "battery/modes.hpp"
#include "esp_littlefs.h"
#include <string.h>
#include "wchar.h"
#include "esp_http_server.h"
#include "string"
#include "vector"
#include "battery/battery.hpp"
#include "logger/q_logger.hpp"
#include "battery/q_battery.hpp"

namespace web
{
    // cache LogLine from t_logger and pull data from it

#include "logger/Web/index.html" // const char INDEX_HTML[] PROGMEM
    esp_err_t setupServer();

    bool checkCorsPreflight(httpd_req_t *req);

    esp_err_t handleName(httpd_req_t *req);

    esp_err_t handleFullShutdown(httpd_req_t *req);

    esp_err_t hangleForceDischargeEnable(httpd_req_t *req);
    esp_err_t handleForceDischargeDisable(httpd_req_t *req);

    esp_err_t handleLogDownload(httpd_req_t *req);
    esp_err_t handleLogDelete(httpd_req_t *req);

    esp_err_t handleParameters(httpd_req_t *req);
    esp_err_t handleAcknowledge(httpd_req_t *req);

    esp_err_t handleReadCells(httpd_req_t *req);
    esp_err_t handleData(httpd_req_t *req);

    esp_err_t handleIdle(httpd_req_t *req);
    esp_err_t handleMonitor(httpd_req_t *req);
    esp_err_t handleBalancing(httpd_req_t *req);

    esp_err_t handleCanMode(httpd_req_t *req);

    esp_err_t handleState(httpd_req_t *req);
    const char *printState(modes::Mode state);

    bool handleFileRead(char *path);
    const char *getContentType(const std::string &filename);

    void saveData();
    char *generateLogLine();
    void writeToFile(FILE *file);

    // Send/stream the HTML file that is embedded in the code
    esp_err_t sendDefaultHTML(httpd_req_t *req);
    // Always send the embedded HTML file
    esp_err_t handleDefault(httpd_req_t *req);
    // Prefer sending the HTML file from SPIFFS, if it exists otherwise send the embedded HTML
    esp_err_t handleRoot(httpd_req_t *req);
    // Upload a new HTML file to SPIFFS
    esp_err_t handleFrontend(httpd_req_t *req);

    esp_err_t handleFileUpload(httpd_req_t *req);

    esp_err_t handleNotFound(httpd_req_t *req, httpd_err_code_t err);
    esp_err_t send_text(httpd_req_t *req, const char *status, const char *type, const std::string &body);

    // Helper functions
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
    esp_err_t add_cors_headers(httpd_req_t *req);
    bool set_mode(modes::Mode mode);
    std::string format_float(float value, int precision);
    float pack_voltage_sum(const battery::IcData &ic);
    bool is_fault_set(uint32_t bits, size_t index);
    bool queue_logger_message(const q_logger::Message &msg);
    bool queue_battery_message(const q_battery::Message &msg);
    bool clear_fault_by_index(size_t fault_index);
} // namespace web

#endif // WEB_HPP