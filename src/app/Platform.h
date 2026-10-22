// What the app needs from the operating system, kept behind one small interface so the whole interface can also run (and be tested) without Windows.
#pragma once
#include <string>
#include <vector>

namespace ctc {

class Platform {
public:
    virtual ~Platform() = default;
    // filters: "CSV files|*.csv;*.txt|All files|*.*"
    virtual std::vector<std::string> openFiles(const char* title, const char* filters, bool multi) = 0;
    virtual std::string saveFile(const char* title, const char* filters, const char* defaultExt, const std::string& suggested) = 0;
    virtual void reveal(const std::string& path) = 0;
    virtual void openUrl(const std::string& url) = 0;           // also used for mailto: and tel:
    virtual std::string dataDir() = 0;
    virtual std::string documentsDir() = 0;
    virtual void setTitle(const std::string&) {}
};

}  // namespace ctc
