#ifndef WORLDENGINE_CONFIGMANAGER_H
#define WORLDENGINE_CONFIGMANAGER_H

#include "core/core.h"


class ConfigManager {
    template<typename T>
    struct ConfigValueMap {
        std::unordered_map<std::string, T> values;
    };
public:
    ConfigManager();

    ~ConfigManager();

    template<typename T>
    const T& get(const std::string& name);

private:
    std::unordered_map<std::type_index, void*> m_valueMaps;
    size_t m_nextId;
};


#endif //WORLDENGINE_CONFIGMANAGER_H
