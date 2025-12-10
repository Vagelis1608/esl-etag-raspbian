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
make # optional, to build both together
```

```
# Raspbian data only
make esl
sudo ./esl <Local Device Name (36 chars max)> <BLE Tag MAC>
```


To display data from a PC, you need to have [OpenHardwareMonitor](https://github.com/hexagon-oss/openhardwaremonitor) running on it, with the Web Server and Allow Remote Access enabled.

You will probably have to allow the port two-way access in your Firewall, including the Windows one.

**This still needs to run on a Raspberry Pi, as the communication protocols arent supported in Windows**

```
# PC data only
make pc
sudo ./pc <PC ip:port> <Device Name (36 chars max)> <BLE Tag MAC>
```

**Must be run as root.**
