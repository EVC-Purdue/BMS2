#ifndef WEB_IDF_HPP
#define WEB_IDF_HPP

#include <cstdio>
#include <string>

#include "battery/modes.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace web_idf
{

#include "logger/Web/index.html" // const char INDEX_HTML[]

    esp_err_t setupServer();
    void stopServer();

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

    bool handleFileRead(httpd_req_t *req, const std::string &path);
    const char *getContentType(const std::string &filename);

    void saveData();
    std::string generateLogLine();
    void writeToFile(std::FILE *file);

    esp_err_t sendDefaultHTML(httpd_req_t *req);
    esp_err_t handleDefault(httpd_req_t *req);
    esp_err_t handleRoot(httpd_req_t *req);
    esp_err_t handleFrontend(httpd_req_t *req);

    esp_err_t handleFileUpload(httpd_req_t *req);

    esp_err_t handleNotFound(httpd_req_t *req, httpd_err_code_t err);

} // namespace web_idf

#endif // WEB_IDF_HPP
