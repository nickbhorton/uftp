# Protocol
## Server
### Errors
LER: command length error, invalid command length
CER: command error, invalid command

### Command packets
FLS|uint32_t number filenames to be sent
    indicates how many FLN packets are to be sent

FLN|uint32_t packet length|uint32_t seq|filename
    filename sent for 'ls' command


### Misc
GDB: acknowledge clients exit


## Client
### Command packets
EXT: notify the server that the client is done
GFL|uint32_t packet_length|filename
    get specific file
LST: get a list of files on server
