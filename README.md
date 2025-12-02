# ESL Tag Remote Data Uploader

Pulls system info from the local device to send to an asl tag for display.

The tag must be running my custom firmware from: https://github.com/Vagelis1608/stellar-L3N-etag/tree/main

or dirivative, since the entire format and communication "protocol" are custom.


### Data

Currently sends the following data to the tag, every 60 seconds: Free and Total RAM, CPU Loads and Temperature, Local IP, Uptime

And the name once.


### Download and build

``` 
git clone --recurse-submodules https://github.com/Vagelis1608/esl-etag-raspbian.git
cd esl-etag-raspbian
make
sudo ./esl <Local Device Name (36 chars max)> <BLE Tag MAC>
```

**Must be run as root.**
