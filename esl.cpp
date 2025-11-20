#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/sysinfo.h>
#include <fstream>

void usage ( const std::string name ) {
    std::cout << "Error! Invalid input." << std::endl << name << " <BLE Tag MAC>" << std::endl;
}

std::string runCmd(const std::string& command) {
    char psBuffer[256];
    FILE*   pPipe;
    std::string result;

    if ((pPipe = popen(command.c_str(), "rt")) == NULL) {
        std::cerr << "Cannot start command\n";
        return "";
    }

    while (fgets(psBuffer, sizeof(psBuffer), pPipe) != NULL) 
        result += psBuffer;

    if(!feof(pPipe)) std::cerr << "Error executing command\n";

    pclose(pPipe);
    return result;
}

struct systemData {
    signed temperature;
    long totalram, freeram, load[3];
    std::string localIP, uptime, memunit;

    bool init() { // Also refreshes the data
        struct sysinfo info;
        if ( sysinfo( &info ) != 0 ) return false;

        this->totalram = info.totalram;

        this->freeram = info.freeram;
        
        for ( int i=0; i < 3; i++ ) this->load[i] = info.loads[i]; // 1, 5, 15 minute loads

        this->memunit = ' B';
        if ( info.mem_unit == 1024 ) this->memunit = 'KB';
        if ( info.mem_unit == ( 1024 * 1024 ) ) this->memunit = 'MB';
        if ( info.mem_unit == ( 1024 * 1024 * 1024 ) ) this->memunit = 'GB';
        if ( info.mem_unit == ( 1024 * 1024 * 1024 * 1024 ) ) this->memunit = 'TB'; // How?

        unsigned long rounded = ( info.uptime / 60 ) + 1;
        this->uptime = ( rounded % 60 ) + " mins";
        rounded /= 60;
        if ( rounded > 0 ) {
            this->uptime = ( rounded % 24 ) + " hours, " + this->uptime;
            rounded /= 24;

            if ( rounded > 0 ) this->uptime = rounded + " days, " + this->uptime;
        }

        // Get temperature and round it
        std::string reportedTemp = runCmd( "cat /sys/class/thermal/thermal_zone0/temp" );
        try {
            this->temperature = std::stoi(reportedTemp) / 1000;
        } catch (const std::exception& e) {
            this->temperature = -274; // Just below absolute zero, fairly clear that something went wrong XD
        }
        
        this->localIP = runCmd( "hostname -I | cut -d' ' -f1" );

        return true;
    }
};

int main ( int argc, const char *argv[] ) {
    if ( argc != 2 ) {
        usage(argv[0]);
        return 1;
    }
}
