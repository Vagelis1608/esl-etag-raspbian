# ESL Tag Remote Data Uploader

Pulls system info from the local device to send to an esl tag for display.

The tag must be running my custom firmware from: https://github.com/Vagelis1608/stellar-L3N-etag/tree/main

or dirivative, since the entire format and communication "protocol" are custom.


### Data

Currently sends the following data to the tag, every 60 seconds (raspbian): Free and Total RAM, CPU Loads and Temperature, Local IP, Uptime

And the name once.


### Download and build

``` 
git clone --recurse-submodules https://github.com/Vagelis1608/esl-etag-raspbian.git
cd esl-etag-raspbian
cmake . # v3.14 minimum
make -j$( nproc )
```
```
# Usage:
$ ./uploader -h
Options:
  -h [ --help ]         Print this help message and exit
  --devices-txt arg     devices.txt to use for btferret. Defaults to btferret/devices.txt
  --local-name arg      Name to send to the tag for local device
  --local-mac arg       ESL Tag's MAC Address for local device
  --pc-name arg         Name to send to the tag for PC
  --pc-mac arg          ESL Tag's MAC Address for PC
  --pc-ip arg           ip:port of PC

Only the first 36 chars of the names will be send.
Both local-name and local-mac must be set, or the local mode gets disabled.
All 3 pc-* must be set, or remote mode gets disabled.

At least one mode must be enabled, or the program errors out.
```

To display data from a PC, you need to have [OpenHardwareMonitor](https://github.com/hexagon-oss/openhardwaremonitor) running on it, with the Web Server and Allow Remote Access enabled.

You will probably have to allow the port two-way access in your Firewall, including the Windows one.

**This still needs to run on a Raspberry Pi, as the communication protocols arent supported in Windows**

**Must be run as root.**
