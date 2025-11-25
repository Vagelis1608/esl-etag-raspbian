#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <sys/sysinfo.h>
#include <fstream>

#include <memory>
#include <stdexcept>
#include <array>

extern "C" {
    #include "btferret/btlib.h"
}

// Use btferret's devices.txt 
// Change to use your own.
#define DEVTXT "btferret/devices.txt"

void usage ( const std::string name ) {
    std::cout << "Error! Invalid input." << std::endl << name << " <Local Device Name (18 chars max)> <BLE Tag MAC>" << std::endl;
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
    uint8_t temperature, memunit, localIP[4];
    uint16_t totalram, freeram, load[3];
    uint32_t uptime; // in minutes

    void init() { // Also refreshes the data
        struct sysinfo info;
        if ( sysinfo( &info ) == 0 ) {
            long double totalram = info.totalram * 1.0;
            long double freeram = info.freeram * 1.0;
            this->memunit = 0; // Bytes
            if ( totalram >= 1024 ) {
                this->memunit = 1; // KBs
                totalram /= 1024;
                freeram /= 1024;
            }
            if ( totalram >= 1024 ) {
                this->memunit = 2; // MBs
                totalram /= 1024;
                freeram /= 1024;
            }
            if ( totalram >= 1024 ) {
                this->memunit = 3; // GBs
                totalram /= 1024;
                freeram /= 1024;
            }
            this->totalram = (int)( totalram * 10 );
            this->freeram = (int)( freeram * 10 );
        } else {
            this->totalram = 0;
            this->freeram = 0;
        }

        try {
            std::string load;
            int i = 0;
            std::stringstream loads = (std::stringstream)runCmd( "uptime | awk -F ':' '{ print $NF }' | sed 's!^ !!' | tr -d ' \n'" );
            while( std::getline(loads, load, ',') && i < 3 ) {
                this->load[i] = std::stod(load) * 100;
                i++;
            }
        } catch (const std::exception& e) {
            this->load[0] = 0;
            this->load[1] = 0;
            this->load[2] = 0;
        }

        try {
            this->uptime = std::stoi( runCmd( "cat /proc/uptime | cut -d. -f1 | tr -d '\n'" ) ) / 60;
        } catch (const std::exception& e) {
            this->uptime = 0;
        }

        // Get temperature and round it
        try {
            std::string reportedTemp = runCmd( "cat /sys/class/thermal/thermal_zone0/temp" );
            this->temperature = std::stoi(reportedTemp) / 1000;
        } catch (const std::exception& e) {
            this->temperature = 0;
        }
        
        try {
            std::string ipbit;
            std::stringstream localIP = (std::stringstream)runCmd( "hostname -I | cut -d' ' -f1 | tr -d ' \n'" );
            int i = 0;
            while( std::getline(localIP, ipbit, '.') && i < 4 ) {
                this->localIP[i] = std::stoi(ipbit);
                i++;
            }
        } catch (const std::exception& e) {
            this->localIP[0] = 0;
            this->localIP[1] = 0;
            this->localIP[2] = 0;
            this->localIP[3] = 0;
        }
    }
};

int main ( int argc, const char *argv[] ) {
    if ( argc != 3 || strcmp( argv[1], "-h" ) == 0 || strcmp( argv[1], "--help" ) == 0 ) {
        usage(argv[0]);
        return 1;
    }

    const std::string localName = argv[1], tagMAC = argv[2];

    if( init_blue(DEVTXT) == 0 ) return 2; // Init btferret

    close_all; // Close btferret
    return 0;
}
