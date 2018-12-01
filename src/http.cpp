#include <ESP8266WebServer.h>
#include <FS.h>
#include <functional>
#include "task.hpp"
#include "http.hpp"

#define KNOWN_MIME_TYPES(_)                     \
  _(".htm", "text/html")                        \
  _(".html", "text/html")                       \
  _(".css", "text/css")                         \
  _(".js", "application/javascript")            \
  _(".png", "image/png")                        \
  _(".gif", "image/gif")                        \
  _(".jpg", "image/jpeg")                       \
  _(".ico", "image/x-icon")                     \
  _(".xml", "text/xml")                         \
  _(".pdf", "application/x-pdf")                \
  _(".zip", "application/x-zip")                \
  _(".gz", "application/x-gzip")

class HTTPServerTask : public Task {
public:
  HTTPServerTask()
    : Task("http-server"),
      server(80),
      fsUploadFile()
  {
    server.on("/test", HTTP_GET, std::bind(&HTTPServerTask::handleTest, this));
    server.on("/list", HTTP_GET, std::bind(&HTTPServerTask::handleFileList, this));
    server.on("/edit", HTTP_GET, std::bind(&HTTPServerTask::handleFileGet, this));
    server.on("/edit", HTTP_POST,
              std::bind(&HTTPServerTask::handleFileUploadPrefix, this),
              std::bind(&HTTPServerTask::handleFileUpload, this));
    server.on("/edit", HTTP_DELETE, std::bind(&HTTPServerTask::handleFileDelete, this));
    server.onNotFound(std::bind(&HTTPServerTask::handleNotFound, this));
    server.begin();
    setActive(true);
  }
  void handleTest() {
    server.send(200, "text/html", "<p><em>Hello</em>, world!</p>\n");
  }
  void handleFileGet() {
    if (!server.hasArg("path")) {
      server.sendHeader("Location", "/edit.html");
      server.send(307);
      return;
    }
    String path = server.arg("path");
    if (SPIFFS.exists(path)) {
      File file = SPIFFS.open(path, "r");
      server.streamFile(file, getContentType(path));
      file.close();
    } else {
      server.send(404, "text/plain", "404: Not Found");
    }
  }
  void handleNotFound() {
    String path = "/www" + server.uri();
    if (path.endsWith("/")) {
      path += "index.html";
    }
    String contentType = getContentType(path);
    String path_gz = path + ".gz";
    if (SPIFFS.exists(path_gz)) {
      File file = SPIFFS.open(path_gz, "r");
      server.streamFile(file, contentType);
      file.close();
    } else if (SPIFFS.exists(path)) {
      File file = SPIFFS.open(path, "r");
      server.streamFile(file, contentType);
      file.close();
    } else {
      server.send(404, "text/plain", "404: Not Found");
    }
  }
  void handleFileUploadPrefix() {
    server.send(200, "text/plain", "");
  }
  void handleFileUpload() {
    if (server.uri() != "/edit") {
      return;
    }
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      String filename = upload.filename;
      if (server.hasArg("prefix")) {
        filename = server.arg("prefix") + "/" + filename;
      }
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      Serial.println("Opening file: " + filename);
      fsUploadFile = SPIFFS.open(filename, "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (fsUploadFile) {
        fsUploadFile.write(upload.buf, upload.currentSize);
      } else {
        server.send(500, "text/plain", "500: couldn't create file");
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (fsUploadFile) {
        fsUploadFile.close();
      } else {
        server.send(500, "text/plain", "500: couldn't create file");
      }
    }
  }
  void handleFileDelete() {
    if (!server.hasArg("path")) {
      server.send(500, "text/plain", "500: bad args");
      return;
    }
    String path = server.arg("path");
    if (path == "/") {
      server.send(500, "text/plain", "500: bad path");
      return;
    }
    if (!SPIFFS.exists(path)) {
      return server.send(404, "text/plain", "FileNotFound");
    }
    SPIFFS.remove(path);
    server.send(200, "text/plain", "");
  }
  void handleFileList() {
    String path = "/";
    if (server.hasArg("dir")) {
      path = server.arg("dir");
    }
    String output = "{\"fs_info\":{";
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    output += "\"totalBytes\":" + String(fs_info.totalBytes);
    output += ",\"usedBytes\":" + String(fs_info.usedBytes);
    output += ",\"blockSize\":" + String(fs_info.blockSize);
    output += ",\"pageSize\":" + String(fs_info.pageSize);
    output += ",\"maxOpenFiles\":" + String(fs_info.maxOpenFiles);
    output += ",\"maxPathLength\":" + String(fs_info.maxPathLength);
    output += "},\"files\":[";
    Dir dir = SPIFFS.openDir(path);
    String comma = "";
    while (dir.next()) {
      File entry = dir.openFile("r");
      output += comma;
      comma = ",";
      output += "{\"type\":\"file\"";
      output += ",\"size\":";
      output += entry.size();
      output += ",\"name\":";
      output += entry.name();
      output += "}";
      entry.close();
    }
    output += "]}";
    server.send(200, "text/json", output);
  }
  void run() override {
    server.handleClient();
  }

  String getContentType(String filename) {
    if (server.hasArg("download")) {
      return "application/octet-stream";
    }
#define HTTP_MIME_HANDLE(ext, mtype) else if(filename.endsWith(ext)) { return mtype; }
    KNOWN_MIME_TYPES(HTTP_MIME_HANDLE)
#undef HTTP_MIME_HANDLE
    else {
      return "text/plain";
    }
  }

private:
  ESP8266WebServer server;
  File fsUploadFile;
};

void initialize_http() {
  new HTTPServerTask();
}
