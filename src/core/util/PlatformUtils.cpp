#include "core/util/PlatformUtils.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>

#include "core/util/Util.h"

std::string PlatformUtils::getFileDirectory(std::string filePath) {
    std::filesystem::path path = std::filesystem::path(filePath);
    if (!std::filesystem::is_directory(filePath)) {
        path = path.parent_path();
    }
    return formatFilePath(path.string());
}

std::string PlatformUtils::formatFilePath(std::string filePath) {
    Util::trim(filePath);
    for (char& i : filePath) {
        if (i == '\\' || i == '/')
            i = getFilePathSeparator();
    }
    if (!filePath.empty() && !filePath.ends_with(getFilePathSeparator())) {
        filePath += getFilePathSeparator();
    }
    return filePath;
}

std::string PlatformUtils::getAbsoluteFilePath(const std::string& filePath) {
    return formatFilePath(std::filesystem::absolute(filePath).string());
}

char PlatformUtils::getFilePathSeparator() {
    return std::filesystem::path::preferred_separator;
}

std::string PlatformUtils::findExecutionDirectory() {
#ifdef _WIN32
            char buffer[MAX_PATH] = { 0 };
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            return getFileDirectory(buffer);
#endif
}
