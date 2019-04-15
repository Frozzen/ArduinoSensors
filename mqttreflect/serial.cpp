#include <stdio.h>      // standard input / output functions
#include <stdlib.h>
#include <string.h>     // string function definitions
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include <termios.h>    // POSIX terminal control definitions

#include <exception>
#include <string>
using namespace std;

class SerialException : public std::exception
{
    int _errno;
    static string msg;
public:
    SerialException(int e, string m);

    // exception interface
public:
    const char *what() const noexcept;
};
string SerialException::msg;

SerialException::SerialException(int e, string m) :
    _errno(e)
{
    msg = m;
    msg += strerror(_errno);
}


const char *SerialException::what() const noexcept {
    return msg.c_str();
}

class SerialIO
{
    int handle;
public:
    SerialIO() : handle(0) {}
      ~SerialIO() { close(handle); }
    void open(string dev = "/dev/ttyUSB0", int baud = B9600);
    string read();
    void write(string);
};

void SerialIO::open(string dev, int baud)
{
    /* Open File Descriptor */
    handle = ::open( dev.c_str(), O_RDWR| O_NONBLOCK | O_NDELAY );

    /* Error Handling */
    if ( handle < 0 )     {
        throw SerialException(errno, "erroropening "+dev);
    }

    struct termios tty;
    struct termios tty_old;
    memset (&tty, 0, sizeof tty);

    /* Error Handling */
    if ( tcgetattr ( handle, &tty ) != 0 ) {
       throw SerialException(errno, "Error  from tcgetattr:");
    }

    /* Save old tty parameters */
    tty_old = tty;

    /* Set Baud Rate */
    cfsetospeed (&tty, (speed_t)baud);
    cfsetispeed (&tty, (speed_t)baud);

    /* Setting other Port Stuff */
    tty.c_cflag     &=  ~PARENB;            // Make 8n1
    tty.c_cflag     &=  ~CSTOPB;
    tty.c_cflag     &=  ~CSIZE;
    tty.c_cflag     |=  CS8;

    tty.c_cflag     &=  ~CRTSCTS;           // no flow control
    tty.c_cc[VMIN]   =  1;                  // read doesn't block
    tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
    tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

    /* Make raw */
    cfmakeraw(&tty);

    /* Flush Port, then applies attributes */
    tcflush( handle, TCIFLUSH );
    if ( tcsetattr ( handle, TCSANOW, &tty ) != 0) {
       throw SerialException(errno, "Error from tcflush:");
    }
}

void SerialIO::write(string str)
{
    for(auto it = str.begin(); it != str.end();) {
        int n_written = ::write( handle, str.c_str(), str.length() );
        if (n_written < 0)
            throw SerialException(errno, "write:");
        it += n_written;
    }
}

string SerialIO::read()
{
    string response;
    response.reserve(1024);
    char ch;
    do {
        int n = ::read( handle, &ch, 1 );
        if (n < 0) {
            throw exception();
        }
        response += ch;
    } while( ch != '\r');
    return response;
}
