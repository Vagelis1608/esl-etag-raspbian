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

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/json/src.hpp>
namespace json = boost::json;

extern "C" {
    #include "btferret/btlib.h"
}

// Use btferret's devices.txt 
// Change to use your own.
#define DEVTXT "./btferret/devices.txt"

// Open Hardware Monitor API Point 
#define DATAJ "/data.json"

// Write Characteristic
#define TAG_LE_WAIT 750 // in ms, btferret default
#define WCHAR_UUID "1F1F" //(( 0x1F<<8 ) + 0x1F )
#define WCHAR_HANDLE 0x001A
#define WCHAR_PERMIT 0x12 // 18 - wan

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
    std::string name;
    signed int node, wcharIndex;

    void refresh() {
        try {
            long double totalram = std::stoi( runCmd( "free -b | grep -i 'mem' | tr -s ' ' | cut -d' ' -f2 | tr -d '\n'" ) ) * 1.0;
            long double freeram = std::stoi( runCmd( "free -b | grep -i 'mem' | tr -s ' ' | cut -d' ' -f7 | tr -d '\n'" ) ) * 1.0;
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
        } catch (const std::exception& e) {
            this->totalram = 0;
            this->freeram = 0;
            this->memunit = 0;
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
            this->temperature = std::stoi( runCmd( "cat /sys/class/thermal/thermal_zone0/temp" ) ) / 1000;
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

    void send () {
        unsigned char message[20] = {0};

        message[0] = 0xEB; // Send data
        message[1] = this->temperature;
        message[2] = this->localIP[0];
        message[3] = this->localIP[1];
        message[4] = this->localIP[2];
        message[5] = this->localIP[3];
        message[6] = ( this->totalram >> 8) & 0xFF;
        message[7] = ( this->totalram ) & 0xFF;
        message[8] = ( this->freeram >> 8 ) & 0xFF;
        message[9] = ( this->freeram ) & 0xFF;
        message[10] = ( this->load[0] >> 8 ) & 0xFF;
        message[11] = ( this->load[0] ) & 0xFF;
        message[12] = ( this->load[1] >> 8 ) & 0xFF;
        message[13] = ( this->load[1] ) & 0xFF;
        message[14] = ( this->load[2] >> 8 ) & 0xFF;
        message[15] = ( this->load[2] ) & 0xFF;
        message[16] = ( this->uptime >> 24 ) & 0xFF;
        message[17] = ( this->uptime >> 16 ) & 0xFF;
        message[18] = ( this->uptime >> 8 ) & 0xFF;
        message[19] = ( this->uptime ) & 0xFF;

        write_ctic( this->node, this->wcharIndex, message, 20 );

        if ( this->firstrun ) {
            message[0] = 0xE2; // Force full screen refresh
            write_ctic( this->node, this->wcharIndex, message, 1 );
            sleep( 3 );
            this->firstrun = false;
        }
    }

    void setName () {
        unsigned char message[20] = {0};
    
        message[0] = 0xEA; // Set name
        message[1] = this->memunit;
        for ( int i = 0; i < 18; i++ ) {
            if ( i < this->name.length() ) message[i+2] = this->name[i];
            else message[i+2] = 0x00;
        }
        write_ctic( this->node, this->wcharIndex, message, 20 );
        sleep( 3 );
    
        if ( this->name.length() > 18 ) {
            message[1] = 0x42; // Extend name
            for ( int i = 0; i < 18; i++ ) {
                if ( i < ( this->name.length() - 18 ) ) message[i+2] = this->name[i+18];
                else message[i+2] = 0x00;
            }
            write_ctic( this->node, this->wcharIndex, message, 20 );
            sleep( 3 );
        }
    }

    void prep () {
        unsigned char message[20] = {0};

        message[0] = 0xE1; // Set Scene
        message[1] = 0x03; // Remote Data
        write_ctic( node, wcharIndex, message, 2 );
        sleep( 3 );

        message[0] = 0xED; // Reset the data on the tag
        write_ctic( this->node, this->wcharIndex, message, 1 );
        sleep( 3 );
    }

private:
    bool firstrun = true;
};

struct remoteData {
        uint8_t cpuTemp, gpuTemp;
        uint16_t cpuPower, gpuPower, cpuLoad, ramLoad, gpuLoad, gpuRamLoad;
        uint32_t uptime; // in minutes
        unsigned long startup; // EPOCH
        std::string name;
        signed int node, wcharIndex;
        bool mode = true;

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

            this->uptime = ( this->startup > 0 ) ? ( ( ( std::time(0) - this->startup ) / 60 ) + 1 ) : 0;

            json::value baseJson = json::parse(*recData).at("Children").at(0).at("Children");

            if ( this->cpuI < 0 || this->gpuI < 0 || this->ramI < 0 ) this->init(baseJson);

            if ( this->cpuI >= 0 ) {
                json::array cpuJson = baseJson.at(this->cpuI).at("Children").as_array();
                
                for ( int i = 0; i < cpuJson.size(); i++ ) {
                    json::string test = cpuJson.at(i).at("NodeId").as_string();

                    if ( test.find( "Temperature" ) != json::string::npos ){
                        for ( int j = 0; j < cpuJson.at(i).at("Children").as_array().size(); j++ ) {
                            json::value temp = cpuJson.at(i).at("Children").at(j);

                            if ( ((json::string)(temp.at("NodeId").as_string())).find( "temperature/0" ) != json::string::npos ) {
                                std::string found = (std::string)temp.at("Value").as_string();
                                this->cpuTemp = std::stoi( found.substr( 0, found.find_first_of( ',' ) ) );
                                break;
                            }
                        }
                    } else if ( test.find( "Load" ) != std::string::npos ){
                        for ( int j = 0; j < cpuJson.at(i).at("Children").as_array().size(); j++ ) {
                            json::value temp = cpuJson.at(i).at("Children").at(j);

                            if ( ((json::string)(temp.at("NodeId").as_string())).find( "load/0" ) != std::string::npos ) {
                                std::string found = (std::string)temp.at("Value").as_string();
                                found.erase( found.find_first_of( ',' ) );
                                this->cpuLoad = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                                break;
                            }
                        }
                    } else if ( test.find( "Power" ) != std::string::npos ){
                        for ( int j = 0; j < cpuJson.at(i).at("Children").as_array().size(); j++ ) {
                            json::value temp = cpuJson.at(i).at("Children").at(j);

                            if ( ((json::string)(temp.at("NodeId").as_string())).find( "power/0" ) != std::string::npos ) {
                                std::string found = (std::string)temp.at("Value").as_string();
                                found.erase( found.find_first_of( ',' ) );
                                this->cpuPower = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                                break;
                            }
                        }
                    }
                }
            }

            if ( this->ramI >= 0 ) {
                json::value ramJson = baseJson.at(this->ramI).at("Children");

                for ( int i = 0; i < ramJson.as_array().size(); i++ ) {
                    if ( ((json::string)(ramJson.at("NodeId").as_string())).find( "Load" ) != std::string::npos ) {
                        for ( int j = 0; j < ramJson.at(i).at("Children").as_array().size(); j++ ) {
                            json::value temp = ramJson.at(i).at("Children").at(j);

                            if ( ((std::string)(temp.at("NodeId").as_string())).find( "load/0" ) != std::string::npos ) {
                                std::string found = (std::string)temp.at("Value").as_string();
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
                json::value gpuJson = baseJson.at(this->gpuI).at("Children");
                
                for ( int i = 0; i < gpuJson.as_array().size(); i++ ) {
                    json::string test = gpuJson.at(i).at("NodeId").as_string();

                    if ( test.find( "Temperature" ) != std::string::npos ){
                        for ( int j = 0; j < gpuJson.at(i).at("Children").as_array().size(); j++ ) {
                            json::value temp = gpuJson.at(i).at("Children").at(j);

                            if ( ((std::string)(temp.at("NodeId").as_string())).find( "temperature/0" ) != std::string::npos ) {
                                std::string found = (std::string)temp.at("Value").as_string();
                                this->gpuTemp = std::stoi( found.substr( 0, found.find_first_of( ',' ) ) );
                                break;
                            }
                        }
                    } else if ( test.find( "Load" ) != std::string::npos ){
                        for ( int j = 0; j < gpuJson.at(i).at("Children").as_array().size(); j++ ) {
                            json::value temp = gpuJson.at(i).at("Children").at(j);

                            if ( ((std::string)(temp.at("NodeId").as_string())).find( "load/0" ) != std::string::npos ) {
                                std::string found = (std::string)temp.at("Value").as_string();
                                found.erase( found.find_first_of( ',' ) );
                                this->gpuLoad = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                            } else if ( strcmp( ((std::string)(temp.at("Text").as_string())).c_str(), "GPU Memory" ) == 0 ) {
                                std::string found = (std::string)temp.at("Value").as_string();
                                found.erase( found.find_first_of( ',' ) );
                                this->gpuRamLoad = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                            }
                        }
                    } else if ( test.find( "Power" ) != std::string::npos ){
                        for ( int j = 0; j < gpuJson.at(i).at("Children").as_array().size(); j++ ) {
                            json::value temp = gpuJson.at(i).at("Children").at(j);

                            if ( ((std::string)(temp.at("NodeId").as_string())).find( "power/0" ) != std::string::npos ) {
                                std::string found = (std::string)temp.at("Value").as_string();
                                found.erase( found.find_first_of( ',' ) );
                                this->gpuPower = std::stoi( found.substr( 0, found.find_first_of( ' ' ) ) );
                                break;
                            }
                        }
                    }
                }
            }
        }

        void send () {
            unsigned char message[20] = {0};
            
            message[0] = 0xEB; // Send data
            message[1] = this->cpuTemp;
            message[2] = this->gpuTemp;
            message[3] = 0x00;
            message[4] = ( this->ramLoad >> 8 ) & 0xFF;
            message[5] = ( this->ramLoad ) & 0xFF;
            message[6] = ( this->cpuPower >> 8) & 0xFF;
            message[7] = ( this->cpuPower ) & 0xFF;
            message[8] = ( this->gpuPower >> 8 ) & 0xFF;
            message[9] = ( this->gpuPower ) & 0xFF;
            message[10] = ( this->cpuLoad >> 8 ) & 0xFF;
            message[11] = ( this->cpuLoad ) & 0xFF;
            message[12] = ( this->gpuLoad >> 8 ) & 0xFF;
            message[13] = ( this->gpuLoad ) & 0xFF;
            message[14] = ( this->gpuRamLoad >> 8 ) & 0xFF;
            message[15] = ( this->gpuRamLoad ) & 0xFF;
            message[16] = ( this->uptime >> 24 ) & 0xFF;
            message[17] = ( this->uptime >> 16 ) & 0xFF;
            message[18] = ( this->uptime >> 8 ) & 0xFF;
            message[19] = ( this->uptime ) & 0xFF;

            write_ctic( this->node, this->wcharIndex, message, 20 );
            sleep( 3 );

            if ( this->mode ) {
                this->setName();

                message[0] = 0xE2; // Force full screen refresh
                write_ctic( this->node, this->wcharIndex, message, 1 );
                sleep( 3 );
                this->mode = false;
            }
        }

        void setName () {
            unsigned char message[20] = {0};
        
            message[0] = 0xEA; // Set name
            message[1] = 0x00;
            for ( int i = 0; i < 18; i++ ) {
                if ( i < this->name.length() ) message[i+2] = this->name[i];
                else message[i+2] = 0x00;
            }
            write_ctic( this->node, this->wcharIndex, message, 20 );
            sleep( 3 );
        
            if ( this->name.length() > 18 ) {
                message[1] = 0x42; // Extend name
                for ( int i = 0; i < 18; i++ ) {
                    if ( i < ( this->name.length() - 18 ) ) message[i+2] = this->name[i+18];
                    else message[i+2] = 0x00;
                }
                write_ctic( this->node, this->wcharIndex, message, 20 );
                sleep( 3 );
            }
        }

        void prep () {
            unsigned char message[20] = {0};

            message[0] = 0xE0; // Set Model
            message[1] = 0x06; // Only 296x156 models supported
            write_ctic( this->node, this->wcharIndex, message, 2 );
            sleep( 3 );
        
            message[0] = 0xE1; // Set Scene
            message[1] = 0x04; // PC Data
            write_ctic( this->node, this->wcharIndex, message, 2 );
            sleep( 3 );
            
            message[0] = 0xED; // Reset the data on the tag
            write_ctic( this->node, this->wcharIndex, message, 1 );
            sleep( 3 );
        }
        
    private:
        signed int cpuI = -1, gpuI = -1, ramI = -1;

        void init(json::value baseJson) {
            try {
                int i = 0;
                do {
                    json::string nodeId = baseJson.at(i).at("NodeId").as_string();

                    if ( nodeId.find( "cpu" ) != std::string::npos ) this->cpuI = i;
                    else if ( nodeId.find( "gpu" ) != std::string::npos ) this->gpuI = i;
                    else if ( nodeId.find( "ram" ) != std::string::npos ) this->cpuI = i;

                    i++;
                } while ( ( this->cpuI < 0 || this->gpuI < 0 || this->ramI < 0 ) && i < baseJson.as_array().size() );
            } catch ( std::exception &e ) {};
        }   
};

signed int getNodeIndex ( std::stringstream allnodes, const char *eslmac ) {
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

void usage ( po::options_description *desc ) {
    std::cout << *desc << "\n" 
              << "Only the first 36 chars of the names will be send.\n"
              << "Both local-name and local-mac must be set, or the local mode gets disabled.\n"
              << "All 3 pc-* must be set, or remote mode gets disabled.\n\n"
              << "At least one mode must be enabled, or the program errors out.\n";
}

int main ( const int argc, const char *argv[] ) {
    po::options_description desc("Options");
    desc.add_options()
        ( "help,h", "Print this help message and exit" )
        ( "local-name", po::value<std::string>(), "Name to send to the tag for local device" )
        ( "local-mac", po::value<std::string>(), "ESL Tag's MAC Address for local device" )
        ( "pc-name", po::value<std::string>(), "Name to send to the tag for PC" )
        ( "pc-mac", po::value<std::string>(), "ESL Tag's MAC Address for PC" )
        ( "pc-ip", po::value<std::string>(), "ip:port of PC" )
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    bool doLocal = ( vm.count("local-name") && vm.count("local-mac") ),
         doPC = ( vm.count("pc-name") && vm.count("pc-mac") && vm.count("pc-ip") );

    if ( vm.count("help") ) {
        usage( &desc );
        return 0;
    }

    if ( ( !doLocal && !doPC ) ) {
        std::cerr << "Error: Invalid Inputs!\n\n";
        usage( &desc );
        return 1;
    }
    
    if( init_blue(DEVTXT) == 0 ) { // Init btferret
        std::cerr << "btferret failed to init." << std::endl;
        return 2; 
    }

    systemData sysData; // Local Data
    remoteData remData; // Remote/PC Data

    le_scan();
    set_le_wait( TAG_LE_WAIT );

    char *devInfoBuff = new char[1024];
    device_info_ex( BTYPE_LE | BTYPE_DISCONNECTED, devInfoBuff, 1024 );
    
    if ( doLocal ) {
        sysData.name = vm["local-name"].as<std::string>();
        sysData.node = getNodeIndex((std::stringstream)devInfoBuff, vm["local-mac"].as<std::string>().c_str() );
        if ( sysData.node < 0 ) {
            std::cerr << "Failed to find node of device with MAC: " << vm["local-mac"].as<std::string>() << std::endl;
            close_all();
            return 3;
        }

        connect_node( sysData.node, CHANNEL_LE, 0 );

        if ( find_ctics( sysData.node ) < 0 ) {
            std::cerr << "Failed to find characteristics of device with MAC: " << vm["local-mac"].as<std::string>() << std::endl;
            close_all();
            return 4;
        }
    
        sysData.wcharIndex = find_ctic_index( sysData.node, UUID_2, strtohex( WCHAR_UUID, NULL ) );
        if ( sysData.wcharIndex < 0 ) {
            std::cerr << "Failed to find characteristic with UUID: " << WCHAR_UUID << std::endl;
            close_all();
            return 5;
        }
        
        sysData.prep();
        sysData.refresh();
        sysData.setName();
    }
    
    if ( doPC ) {
        remData.name = vm["pc-name"].as<std::string>();
        remData.node = getNodeIndex( (std::stringstream)devInfoBuff, vm["pc-mac"].as<std::string>().c_str() );
        if ( remData.node < 0 ) {
            std::cerr << "Failed to find node of device with MAC: " << vm["pc-mac"].as<std::string>() << std::endl;
            close_all();
            return 3;
        }

        connect_node( remData.node, CHANNEL_LE, 0 );
        
        if ( find_ctics( remData.node ) < 0 ) {
            std::cerr << "Failed to find characteristics of device with MAC: " << vm["pc-mac"].as<std::string>() << std::endl;
            close_all();
            return 4;
        }
    
        remData.wcharIndex = find_ctic_index( remData.node, UUID_2, strtohex( WCHAR_UUID, NULL ) );
        if ( remData.wcharIndex < 0 ) {
            std::cerr << "Failed to find characteristic with UUID: " << WCHAR_UUID << std::endl;
            close_all();
            return 5;
        }

        remData.prep();
        remData.setName();
        disconnect_node( remData.node );
    }

    delete[] devInfoBuff;

    try {
        unsigned long loopTimer = std::time(0);
        cpr::Response req;
        uint8_t attempts = 0;
        unsigned char message[20];
        std::string apiPoint = (vm["pc-ip"].as<std::string>()) + DATAJ;

        while (true) {
            loopTimer = std::time(0);

            if ( doLocal ) {
                sysData.refresh();
                sysData.send();
                disconnect_node( sysData.node );
            }

            if ( doPC ) {
                req = cpr::Get(cpr::Url{ apiPoint }, cpr::Timeout{60000}, cpr::ReserveSize{ 1024 * 1024 });
        
                if ( req.status_code != 200 ) {
                    if ( attempts == 6 ) {
                        remData.mode = true;
                        remData.startup = 0;

                        connect_node( remData.node, CHANNEL_LE, 0 );

                        message[0] = 0xED; // Reset the data on the tag
                        write_ctic( remData.node, remData.wcharIndex, message, 1 );
                        sleep( 3 );

                        disconnect_node( remData.node );
                    }
                    if ( attempts <= 6 ) attempts++;
                } else {
                    connect_node( remData.node, CHANNEL_LE, 0 );

                    if ( remData.startup == 0 ) remData.startup = std::time(0);
                    remData.refresh(&req.text);
                    remData.send();

                    disconnect_node( remData.node );
                }
            }

            sleep( ( std::time(0) - loopTimer < 60 ) ? ( std::time(0) - loopTimer ) : 1 );
            if ( doLocal ) connect_node( sysData.node, CHANNEL_LE, 0 );
        }
    } catch ( std::exception &e ) {
        std::cerr << "Exception thrown: " << e.what() << '\n' << "Exiting..." << std::endl;
    }

    close_all();
    return 6;
}
