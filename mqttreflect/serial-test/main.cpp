#include <iostream>
#include "serial.hpp"

using namespace std;

int main()
{
    SerialIO serial;
    int err = serial.open("/dev/ttyUSB0", B38400);
    if(err != 0) {
        cout << "error opening " << errno << ":" << SerialIO::last_error_string() <<endl;
        return 0;
    }
    try {
    while(1) {
        cout << "r:" << serial.read() << endl;
    }
    } catch(SerialException &e) {
        cout << "exc:" << e.what() << endl;
    }
    return 0;
}
