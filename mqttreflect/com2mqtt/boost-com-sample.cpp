
#include <iostream>
#include "SimpleSerial.h"

using namespace std;
using namespace boost;

int main(int argc, char* argv[])
{
    try {

        SimpleSerial serial("/dev/ttyUSB0", 38400);

        //serial.writeString("Hello world\n");
        while(1)
        cout<<"Received : " << serial.readLine() <<endl;

    } catch(boost::system::system_error& e)
    {
        cout<<"Error: "<<e.what()<<endl;
        return 1;
    }
}

