
#include "Exception.h"

Exception::Exception(const std::string& message):
    m_message(message) {
}

Exception::Exception():
    Exception("Unknown exception") {
}

Exception::~Exception() {}

const char *Exception::what() const noexcept {
    return m_message.c_str();
}
