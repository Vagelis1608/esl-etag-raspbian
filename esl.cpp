#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <fstream>

#include <memory>
#include <stdexcept>
#include <array>

void usage ( const std::string name ) {
    std::cout << "Error! Invalid input." << std::endl << name << " <BLE Tag MAC>" << std::endl;
}

std::string runCmd(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

struct systemData {
    __int16_t temperature;
    __uint16_t totalram, freeram;
    std::string localIP, uptime, memunit, load;

    bool init() { // Also refreshes the data
        struct sysinfo info;
        if ( sysinfo( &info ) != 0 ) return false;

        long double totalram = info.totalram * 1.0;
        long double freeram = info.freeram * 1.0;
        this->memunit = " Bs";
        if ( totalram >= 1024 ) {
            this->memunit = "KBs";
            totalram /= 1024;
            freeram /= 1024;
        }
        if ( totalram >= 1024 ) {
            this->memunit = "MBs";
            totalram /= 1024;
            freeram /= 1024;
        }
        if ( totalram >= 1024 ) {
            this->memunit = "GBs";
            totalram /= 1024;
            freeram /= 1024;
        }
        this->totalram = (int)( totalram * 100 );
        this->freeram = (int)( freeram * 100 );

        this->load = runCmd( "uptime | awk -F ':' '{ print $NF }' | sed 's!^ !!' | tr -d '\n'" );

        this->uptime = runCmd( "uptime -p | tr -d '\n'" );

        // Get temperature and round it
        std::string reportedTemp = runCmd( "cat /sys/class/thermal/thermal_zone0/temp" );
        try {
            this->temperature = std::stoi(reportedTemp) / 1000;
        } catch (const std::exception& e) {
            this->temperature = -274; // Just below absolute zero, fairly clear that something went wrong XD
        }
        
        this->localIP = runCmd( "hostname -I | cut -d' ' -f1 | tr -d '\n'" );

        return true;
    }
};

int main ( int argc, const char *argv[] ) {
    /*if ( argc != 2 || strcmp( argv[1], "-h" ) == 0 || strcmp( argv[1], "--help" ) ) {
        usage(argv[0]);
        return 1;
    }*/
   systemData test;
   test.init();
   std::cout << "Temp: " << test.temperature << std::endl;
   std::cout << "RAM: " << test.totalram / 100.0 << " / " << test.freeram / 100.0 << " " << test.memunit << std::endl;
   std::cout << "Loads: " << test.load << std::endl;
   std::cout << "IP: " << test.localIP << std::endl;
   std::cout << "Uptime: " << test.uptime << std::endl;
}
