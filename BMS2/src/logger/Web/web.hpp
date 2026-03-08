#ifndef WEB_HPP
#define WEB_HPP

#include "battery/modes.hpp"
#include "esp_littlefs.h"
#include <string.h>
#include "wchar.h"

namespace web
{
    // cache LogLine from t_logger and pull data from it

#include "logger/Web/index.html" // const char INDEX_HTML[] PROGMEM
    void setupServer();

    bool checkCorsPreflight();

    void handleName();

    void handleFullShutdown();

    void hangleForceDischargeEnable();
    void handleForceDischargeDisable();

    void handleLogDownload();
    void handleLogDelete();

    void handleParameters();
    void handleAcknowledge();

    void handleReadCells();
    void handleData();

    void handleIdle();
    void handleMonitor();
    void handleBalancing();

    void handleCanMode();

    void handleState();
    const char *printState(modes::Mode state);

    bool handleFileRead(char *path);
    char *getContentType(char *filename);

    void saveData();
    char *generateLogLine();
    void writeToFile(FILE &file);

    // Send/stream the HTML file that is embedded in the code
    void sendDefaultHTML();
    // Always send the embedded HTML file
    void handleDefault();
    // Prefer sending the HTML file from SPIFFS, if it exists otherwise send the embedded HTML
    void handleRoot();
    // Upload a new HTML file to SPIFFS
    void handleFrontend();

    void handleFileUpload();

    void handleNotFound();

} // namespace web

#endif // WEB_HPP