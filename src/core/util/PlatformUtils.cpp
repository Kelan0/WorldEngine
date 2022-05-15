#include "core/util/PlatformUtils.h"

#ifdef _WIN32
#include <windows.h>
#endif

std::string PlatformUtils::findExecutionDirectory() {
#ifdef _WIN32
            char buffer[MAX_PATH] = { 0 };
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            size_t end = 0;
            for (size_t i = 0; i < MAX_PATH && buffer[i] != '\0'; ++i) {
                if (buffer[i] == '\\') buffer[i] = '/';
                if (buffer[i] == '/') end = i;
            }
            buffer[end] = '\0';
            std::string t(buffer);
            return t;
#endif
}
