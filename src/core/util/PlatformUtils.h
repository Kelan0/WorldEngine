#ifndef WORLDENGINE_PLATFORMUTILS_H
#define WORLDENGINE_PLATFORMUTILS_H

#include <string>

namespace PlatformUtils {

    std::string getFileDirectory(std::string filePath);

    std::string formatFilePath(std::string filePath);

    std::string getAbsoluteFilePath(const std::string& filePath);

    char getFilePathSeparator();

    std::string findExecutionDirectory();
};


#endif //WORLDENGINE_PLATFORMUTILS_H
