#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

// Simple thread-safe logger that writes to file
class Logger {
 public:
  enum Level { DEBUG, INFO, WARNING, ERROR };

  static Logger& getInstance() {
    static Logger instance;
    return instance;
  }

  void setEnabled(bool enabled) { this->enabled = enabled; }

  void setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex);
    if (logFile.is_open()) {
      logFile.close();
    }
    logFile.open(filename, std::ios::app);
  }

  void log(Level level, const std::string& message) {
    if (!enabled) return;

    std::lock_guard<std::mutex> lock(mutex);
    std::ostream& out = logFile.is_open() ? logFile : std::cerr;

    const char* levelStr = "";
    switch (level) {
      case DEBUG:
        levelStr = "DEBUG";
        break;
      case INFO:
        levelStr = "INFO";
        break;
      case WARNING:
        levelStr = "WARNING";
        break;
      case ERROR:
        levelStr = "ERROR";
        break;
    }

    out << "[" << levelStr << "] " << message << std::endl;
  }

  // Convenience methods
  void debug(const std::string& msg) { log(DEBUG, msg); }
  void info(const std::string& msg) { log(INFO, msg); }
  void warning(const std::string& msg) { log(WARNING, msg); }
  void error(const std::string& msg) { log(ERROR, msg); }

 private:
  Logger() : enabled(false) {}
  ~Logger() {
    if (logFile.is_open()) {
      logFile.close();
    }
  }

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  bool enabled;
  std::ofstream logFile;
  std::mutex mutex;
};
