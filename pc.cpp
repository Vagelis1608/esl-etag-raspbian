#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <array>
#include <unistd.h>
#include <ctime>

#include <cpr/cpr.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Open Hardware Monitor API Point 
#define DATAJ "/data.json"

extern "C" {
    #include "btferret/btlib.h"
}

// Use btferret's devices.txt 
// Change to use your own.
#define DEVTXT "./btferret/devices.txt"

// Write Characteristic
#define TAG_LE_WAIT 750 // in ms, btferret default
#define WCHAR_UUID "1F1F" //(( 0x1F<<8 ) + 0x1F )
#define WCHAR_HANDLE 0x001A
#define WCHAR_PERMIT 0x12 // 18 - wan

void usage ( const std::string name ) {
    std::cout << "Error! Invalid input." << std::endl << name << " <PC ip:port> <Device Name (36 chars max)> <BLE Tag MAC>" << std::endl;
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
    private:
        signed int cpuI = -1, gpuI = -1, ramI = -1;

        void init(json baseJson) {
            try {
                int i = 0;
                do {
                    std::string nodeId = baseJson[i]["NodeId"];

                    if ( nodeId.find( "cpu" ) != std::string::npos ) this->cpuI = i;
                    else if ( nodeId.find( "gpu" ) != std::string::npos ) this->gpuI = i;
                    else if ( nodeId.find( "ram" ) != std::string::npos ) this->cpuI = i;

                    i++;
                } while ( ( this->cpuI < 0 || this->gpuI < 0 || this->ramI < 0 ) && i < baseJson.size() );
            } catch ( std::exception &e ) {};
        }
            
    public:
        uint8_t cpuTemp, gpuTemp;
        uint16_t cpuPower, gpuPower, cpuLoad, ramLoad, gpuLoad, gpuRamLoad;
        uint32_t uptime; // in minutes
        unsigned long startup; // EPOCH

        void reset () {
            this->cpuTemp = 0;
            this->cpuLoad = 0;
            this->cpuPower = 0;
            this->ramLoad = 0;
            this->gpuTemp = 0;
            this->gpuLoad = 0;
            this->gpuPower = 0;
            this->gpuRamLoad = 0;
        }

        void refresh (const std::string *recData) {
            this->reset();

            uptime = ( this->startup > 0 ) ? ( ( ( std::time(0) - this->startup ) / 60 ) + 1 ) : 0;

            json recJson = json::parse(*recData),
                baseJson = recJson["Children"][0]["Children"];

            if ( this->cpuI < 0 || this->gpuI < 0 || this->ramI < 0 ) this->init(baseJson);

            if ( this->cpuI >= 0 ) {
                json cpuJson = baseJson[this->cpuI];
                
                for ( int i = 0; i < cpuJson.size(); i++ ) {
                    std::string test = cpuJson[i]["NodeId"];

                    if ( test.find( "Temperature" ) != std::string::npos ){
                        for ( int j = 0; j < cpuJson[i]["Children"].size(); j++ ) {
                            json temp = cpuJson[i]["Children"][j];

                            if ( ((std::string)(temp["NodeId"])).find( "temperature/0" ) != std::string::npos ) {
                                std::string found = temp["Value"];
                                this->cpuTemp = std::stoi( found.substr( 0, found.find_first_of( ',' ) ) );
                                break;
                            }
                        }
                    } else if ( test.find( "Load" ) != std::string::npos ){
                        for ( int j = 0; j < cpuJson[i]["Children"].size(); j++ ) {
                            json temp = cpuJson[i]["Children"][j];

                            if ( ((std::string)(temp["NodeId"])).find( "load/0" ) != std::string::npos ) {
                                std::string found = temp["Value"];
                                found.erase( found.find_first_of( ',' ) );
                                this->cpuLoad = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                                break;
                            }
                        }
                    } else if ( test.find( "Power" ) != std::string::npos ){
                        for ( int j = 0; j < cpuJson[i]["Children"].size(); j++ ) {
                            json temp = cpuJson[i]["Children"][j];

                            if ( ((std::string)(temp["NodeId"])).find( "power/0" ) != std::string::npos ) {
                                std::string found = temp["Value"];
                                found.erase( found.find_first_of( ',' ) );
                                this->cpuPower = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                                break;
                            }
                        }
                    }
                }
            }

            if ( this->ramI >= 0 ) {
                json ramJson = baseJson[this->ramI];

                for ( int i = 0; i < ramJson.size(); i++ ) {
                    if ( ((std::string)(ramJson["NodeId"])).find( "Load" ) != std::string::npos ) {
                        for ( int j = 0; j < ramJson[i]["Children"].size(); j++ ) {
                            json temp = ramJson[i]["Children"][j];

                            if ( ((std::string)(temp["NodeId"])).find( "load/0" ) != std::string::npos ) {
                                std::string found = temp["Value"];
                                found.erase( found.find_first_of( ',' ) );
                                this->cpuLoad = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            if ( this->gpuI >= 0 ) {
                json gpuJson = baseJson[this->gpuI];
                
                for ( int i = 0; i < gpuJson.size(); i++ ) {
                    std::string test = gpuJson[i]["NodeId"];

                    if ( test.find( "Temperature" ) != std::string::npos ){
                        for ( int j = 0; j < gpuJson[i]["Children"].size(); j++ ) {
                            json temp = gpuJson[i]["Children"][j];

                            if ( ((std::string)(temp["NodeId"])).find( "temperature/0" ) != std::string::npos ) {
                                std::string found = temp["Value"];
                                this->gpuTemp = std::stoi( found.substr( 0, found.find_first_of( ',' ) ) );
                                break;
                            }
                        }
                    } else if ( test.find( "Load" ) != std::string::npos ){
                        for ( int j = 0; j < gpuJson[i]["Children"].size(); j++ ) {
                            json temp = gpuJson[i]["Children"][j];

                            if ( ((std::string)(temp["NodeId"])).find( "load/0" ) != std::string::npos ) {
                                std::string found = temp["Value"];
                                found.erase( found.find_first_of( ',' ) );
                                this->gpuLoad = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                            } else if ( strcmp( ((std::string)(temp["Text"])).c_str(), "GPU Memory" ) == 0 ) {
                                std::string found = temp["Value"];
                                found.erase( found.find_first_of( ',' ) );
                                this->gpuRamLoad = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                            }
                        }
                    } else if ( test.find( "Power" ) != std::string::npos ){
                        for ( int j = 0; j < gpuJson[i]["Children"].size(); j++ ) {
                            json temp = gpuJson[i]["Children"][j];

                            if ( ((std::string)(temp["NodeId"])).find( "power/0" ) != std::string::npos ) {
                                std::string found = temp["Value"];
                                found.erase( found.find_first_of( ',' ) );
                                this->gpuPower = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                                break;
                            }
                        }
                    }
                }
            }
       }
};

int getNodeIndex ( std::stringstream allnodes, const char *eslmac ) {
    try {
        std::string buff;
        while( std::getline(allnodes, buff, '\n') ) {
            if ( buff.find( eslmac ) != std::string::npos ) {
                return std::stoi( buff.substr( 0, buff.find_first_of( ' ' ) ) );
            }
        }
    }  catch (const std::exception& e) {
        std::cerr << "Exception thrown while getting the Node Index: " << e.what() << std::endl;
    }
    return -1;
}

void setName ( std::string pcName, int node, int wcharIndex ) {
    unsigned char message[20] = {0};

    message[0] = 0xEA; // Set name
    message[1] = 0x00;
    for ( int i = 0; i < 18; i++ ) {
        if ( i < pcName.length() ) message[i+2] = pcName[i];
        else message[i+2] = 0x00;
    }
    write_ctic( node, wcharIndex, message, 20 );
    sleep( 3 );

    if ( pcName.length() > 18 ) {
        message[1] = 0x42; // Extend name
        for ( int i = 0; i < 18; i++ ) {
            if ( i < ( pcName.length() - 18 ) ) message[i+2] = pcName[i+18];
            else message[i+2] = 0x00;
        }
        write_ctic( node, wcharIndex, message, 20 );
        sleep( 3 );
    }
}

int main ( int argc, const char *argv[] ) {
    if ( argc != 4 || strcmp( argv[1], "-h" ) == 0 || strcmp( argv[1], "--help" ) == 0 ) {
        usage(argv[0]);
        return 1;
    }

    const std::string apiPoint = ( *argv[1] + DATAJ ), pcName = argv[2], tagMAC = argv[3];

    if( init_blue(DEVTXT) == 0 ) {
        std::cerr << "btferret failed to init." << std::endl;
        return 2; // Init btferret
    }

    le_scan();
    char *devInfoBuff = new char[1024];
    device_info_ex( BTYPE_LE | BTYPE_DISCONNECTED, devInfoBuff, 1024 );
    
    signed int node = getNodeIndex((std::stringstream)devInfoBuff, tagMAC.c_str());
    delete[] devInfoBuff;
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

    signed int wcharIndex = find_ctic_index( node, UUID_2, strtohex( WCHAR_UUID, NULL ) );
    if ( wcharIndex < 0 ) {
        std::cerr << "Failed to find characteristic with UUID: " << WCHAR_UUID << std::endl;
        close_all;
        return 5;
    }

    unsigned char message[20] = {0};

    message[0] = 0xE0; // Set Model
    message[1] = 0x06; // Only 296x156 models supported
    write_ctic( node, wcharIndex, message, 2 );
    sleep( 3 );

    message[0] = 0xE1; // Set Scene
    message[1] = 0x04; // PC Data
    write_ctic( node, wcharIndex, message, 2 );
    sleep( 3 );

    message[0] = 0xED; // Reset the data on the tag first
    write_ctic( node, wcharIndex, message, 1 );
    sleep( 3 );

    setName( pcName, node, wcharIndex );

    try{
        bool mode = true; // true: reset data on connection
        cpr::Response req;
        uint8_t attempts = 0;
        systemData sysData;
        sysData.startup = 0;

        while (1) {
            req = cpr::Get(cpr::Url{ apiPoint }, cpr::Timeout{60000}, cpr::ReserveSize{ 1024 * 1024 });

            if ( req.status_code != 200 ) {
                if ( attempts == 6 ) {
                    mode = true;
                    sysData.startup = 0;

                    message[0] = 0xED; // Reset the data on the tag
                    write_ctic( node, wcharIndex, message, 1 );
                    sleep( 3 );
                }
                if ( attempts <= 6 ) attempts++;
            } else {
                if ( sysData.startup == 0 ) sysData.startup = std::time(0);
                sysData.refresh(&req.text);

                message[0] = 0xEB; // Send data
                message[1] = sysData.cpuTemp;
                message[2] = sysData.gpuTemp;
                message[3] = 0x00;
                message[4] = ( sysData.ramLoad >> 8 ) & 0xFF;
                message[5] = ( sysData.ramLoad ) & 0xFF;
                message[6] = ( sysData.cpuPower >> 8) & 0xFF;
                message[7] = ( sysData.cpuPower ) & 0xFF;
                message[8] = ( sysData.gpuPower >> 8 ) & 0xFF;
                message[9] = ( sysData.gpuPower ) & 0xFF;
                message[10] = ( sysData.cpuLoad >> 8 ) & 0xFF;
                message[11] = ( sysData.cpuLoad ) & 0xFF;
                message[12] = ( sysData.gpuLoad >> 8 ) & 0xFF;
                message[13] = ( sysData.gpuLoad ) & 0xFF;
                message[14] = ( sysData.gpuRamLoad >> 8 ) & 0xFF;
                message[15] = ( sysData.gpuRamLoad ) & 0xFF;
                message[16] = ( sysData.uptime >> 24 ) & 0xFF;
                message[17] = ( sysData.uptime >> 16 ) & 0xFF;
                message[18] = ( sysData.uptime >> 8 ) & 0xFF;
                message[19] = ( sysData.uptime ) & 0xFF;

                write_ctic( node, wcharIndex, message, 20 );
                sleep( 3 );

                if ( mode ) {
                    setName( pcName, node, wcharIndex );

                    message[0] = 0xE2; // Force full screen refresh
                    write_ctic( node, wcharIndex, message, 1 );
                    sleep( 3 );
                    mode = false;
                }
            }

            disconnect_node( node );
            sleep( ( req.elapsed < 60 ) ? ( 60 - req.elapsed ) : 1 );
            connect_node( node, CHANNEL_LE, 0 );
        }
    } catch ( const std::exception &e ) {
        std::cerr << "Exception thrown: " << e.what() << std::endl << "Exiting..." << std::endl;
    }

    close_all; // Close btferret
    return 0;
}
