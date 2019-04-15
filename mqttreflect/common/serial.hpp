#ifndef SERIAL_HPP
#define SERIAL_HPP
#include <exception>
#include <string>
#include <termios.h>    // POSIX terminal control definitions


class SerialException : public std::exception
{
    int _errno;
    static std::string msg;
public:
    SerialException(int e, std::string m);

    // exception interface
public:
    const char *what() const noexcept;
};

class SerialIO
{
    int handle;
public:
    SerialIO() : handle(0) {}
    ~SerialIO();
    int open(std::string dev = "/dev/ttyUSB0", int baud = B9600);
    std::string read();
    void write(std::string);
    static std::string last_error_string();
};

#endif // SERIAL_HPP
