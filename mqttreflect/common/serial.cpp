#include <iostream>

#include <stdio.h>      // standard input / output functions
#include <stdlib.h>
#include <string.h>     // string function definitions
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions

#include <exception>
#include <string>
#include "serial.hpp"

using namespace std;

string SerialException::msg;

SerialException::SerialException(int e, string m) :
    _errno(e)
{
    msg = m + strerror(_errno);
}


const char *SerialException::what() const noexcept {
    return msg.c_str();
}
SerialIO::~SerialIO()
{   if(handle)
        ::close(handle);
}

/**
 * @brief SerialIO::open
 * @param dev
 * @param baud
 * @return
 */
int SerialIO::open(string dev, int baud)
{
    /* Open File Descriptor */
    handle = ::open( dev.c_str(), O_RDWR| O_NONBLOCK /*| O_NDELAY*/ );

    /* Error Handling */
    if ( handle < 0 )     {
        return errno;
        //throw SerialException(errno, "erroropening "+dev);
    }

    struct termios tty;
    struct termios tty_old;
    memset (&tty, 0, sizeof tty);

    /* Error Handling */
    if ( tcgetattr ( handle, &tty ) != 0 ) {
        return errno;
       //throw SerialException(errno, "Error  from tcgetattr:");
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

    tty.c_iflag &= ~INPCK;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;

    tty.c_cflag     &=  ~CRTSCTS;           // no flow control
    tty.c_cc[VMIN]   =  1;                  // read doesn't block
    tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
    tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

    /* Make raw */
    cfmakeraw(&tty);

    /* Flush Port, then applies attributes */
    tcflush( handle, TCIFLUSH );
    if ( tcsetattr ( handle, TCSANOW, &tty ) != 0) {
        return errno;
       //throw SerialException(errno, "Error from tcflush:");
    }
    return 0;
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

string SerialIO::last_error_string()
{
    return strerror(errno);
}
/**
  /usr/lib/python3/dist-packages/serial/serialposix.py", line 509, in read при пропадании устройства

 * @brief SerialIO::read
 * @return
 */
string SerialIO::read()
{
    string response;
    response.reserve(1024);
    char ch;
    do {
        int n = ::read( handle, &ch, 1 );
        cout << "++" << n << "++";
        if (n < 0) {
/*            if(::eof(handle))
                throw SerialException(errno, "device reports readiness to read but returned no data "
                                             "(device disconnected or multiple access on port?)");*/
            if (EAGAIN != errno && errno != EALREADY &&
                     errno != EWOULDBLOCK && errno != EINPROGRESS &&
                     errno != EINTR)
                throw SerialException(errno, "read:");
        } else
            response += ch;
        usleep(10000);
    } while( ch != '\r');
    return response;
}
