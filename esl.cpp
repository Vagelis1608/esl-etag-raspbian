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
#include <unistd.h>

extern "C" {
    #include "btferret/btlib.h"
}

// Use btferret's devices.txt 
// Change to use your own.
#define DEVTXT "btferret/devices.txt"

// L3N@ Write Characteristic
#define TAG_LE_WAIT 750 // in ms, btferret default
#define WCHAR_UUID "1F1F" //(( 0x1F<<8 ) + 0x1F )
#define WCHAR_HANDLE 0x001A
#define WCHAR_PERMIT 0x12 // 18 - wan

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
                this->load[i] = std::stod(load) * 10;
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

int getNodeIndex ( std::stringstream allnodes, const char *eslmac ) {
    int node;
    try {
        std::string buff;
        while( std::getline(allnodes, buff, '\n') ) {
            if ( buff.find( eslmac ) != std::string::npos ) {
                return std::stoi( buff.substr( 0, buff.find_first_of( ' ' ) ) );
            }
        }
    }  catch (const std::exception& e) {
        throw e;
    }
    return -1;
}

int main ( int argc, const char *argv[] ) {
    if ( argc != 3 || strcmp( argv[1], "-h" ) == 0 || strcmp( argv[1], "--help" ) == 0 ) {
        usage(argv[0]);
        return 1;
    }

    const std::string localName = argv[1], tagMAC = argv[2];

    systemData sysData;
    sysData.init();

    if( init_blue(DEVTXT) == 0 ) {
        std::cerr << "btferret failed to init." << std::endl;
        return 2; // Init btferret
    }

    le_scan();
    char *devInfoBuff = new char[1024];
    device_info_ex( BTYPE_LE | BTYPE_DISCONNECTED, devInfoBuff, 1024 );
    std::string devInfo = devInfoBuff;
    delete[] devInfoBuff;
    
    int node = getNodeIndex((std::stringstream)devInfo, tagMAC.c_str());
    if ( node < 0 ) {
        std::cerr << "Failed to find node of device with MAC: " << tagMAC << std::endl;
        close_all;
        return 3;
    }
    set_le_wait( TAG_LE_WAIT );
    connect_node( node, CHANNEL_LE, 0 );

    if ( find_ctics( node ) < 0 ) {
        std::cerr << "Failed to find characteristics of device with MAC: " << tagMAC << std::endl;
        close_all;
        return 4;
    }

    int wcharIndex = find_ctic_index( node, UUID_2, strtohex( WCHAR_UUID, NULL ) );
    if ( wcharIndex < 0 ) {
        std::cerr << "Failed to find characteristic with UUID: " << WCHAR_UUID << std::endl;
        close_all;
        return 5;
    }

    __u8 message[20] = {0};

    message[0] = 0xEA; // Set name and memunit
    message[1] = sysData.memunit;
    for ( int i = 0; i < 18; i++ ) {
        if ( i < localName.length() ) message[i+2] = localName[i];
        else message[i] = 0x00;
    }
    write_ctic( node, wcharIndex, message, 20 );

    message[0] = 0xE2; // Force full screen refresh
    write_ctic( node, wcharIndex, message, 1 );

    try{
        while (1) {
            message[0] = 0xEB; // Send data
            message[1] = sysData.temperature;
            message[2] = sysData.localIP[0];
            message[3] = sysData.localIP[1];
            message[4] = sysData.localIP[2];
            message[5] = sysData.localIP[3];
            message[6] = ( sysData.totalram >> 8) & 0xFF;;
            message[7] = ( sysData.totalram ) & 0xFF;;
            message[8] = ( sysData.freeram >> 8 ) & 0xFF;
            message[9] = ( sysData.freeram ) & 0xFF;
            message[10] = ( sysData.load[0] >> 8 ) & 0xFF;
            message[11] = ( sysData.load[0] ) & 0xFF;
            message[12] = ( sysData.load[1] >> 8 ) & 0xFF;
            message[13] = ( sysData.load[1] ) & 0xFF;
            message[14] = ( sysData.load[2] >> 8 ) & 0xFF;
            message[15] = ( sysData.load[2] ) & 0xFF;
            message[16] = ( sysData.uptime >> 24 ) & 0xFF;
            message[17] = ( sysData.uptime >> 16 ) & 0xFF;
            message[18] = ( sysData.uptime >> 8 ) & 0xFF;
            message[19] = ( sysData.uptime ) & 0xFF;

            write_ctic( node, wcharIndex, message, 20 );
            
            disconnect_node( node );
            sleep( 60 );
            sysData.init();
            connect_node( node, CHANNEL_LE, 0 );
        }
    } catch ( const std::exception &e ) {
        std::cerr << "Exception thrown: " << e.what() << std::endl << "Exiting..." << std::endl;
    }

    close_all; // Close btferret
    return 0;
}
