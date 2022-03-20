
#ifndef WORLDENGINE_EXCEPTION_H
#define WORLDENGINE_EXCEPTION_H

#include <exception>
#include <string>

class Exception : std::exception {
public:
    Exception(const std::string& message);

    Exception();

    ~Exception();

    virtual const char* what() const noexcept override;

private:
    std::string m_message;
};


#endif //WORLDENGINE_EXCEPTION_H
