#ifndef WEB_HPP
#define WEB_HPP

#include "battery/modes.hpp"
#include "esp_littlefs.h"

namespace web
{

#include "logger/Web/index.html" // const char INDEX_HTML[] PROGMEM

    char *getIndexHtml();

    char *getName();

    char *fullShutdown();

    char *forceDischargeEnable();
    char *forceDischargeDisable();

    char *logDownload();
    char *logDelete();

    char *parameters();
    char *acknowledge();

    char *readCells();
    char *data();

    char *idle();
    char *monitor();
    char *balancing();

    char *canMode();

    char *state();
    const char *printState(modes::Mode state);

    char *fileRead(char *path);
    char *getContentType(char *filename);

    char *saveData();
    char *generateLogLine();
    void writeToFile(FILE *file);

    // Send/stream the HTML file that is embedded in the code
    char *sendDefaultHTML();
    // Always send the embedded HTML file
    char *handleDefault();
    // Prefer sending the HTML file from SPIFFS, if it exists otherwise send the embedded HTML
    char *handleRoot();
    // Upload a new HTML file to SPIFFS
    char *handleFrontend();

    char *handleFileUpload();

    char *handleNotFound();

} // namespace web

#endif // WEB_HPP