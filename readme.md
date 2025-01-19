# Protocol
## Server
### Errors
'CER'
    response to an invalid command

### Responses
'FLS'|uint32_t number_filenames to be sent
    indicates how many FLN packets will be sent

FLN|uint32_t packet length|uint32_t seq|filename
    filename sent for 'ls' command

FGS|uint32_t number lines to be sent
    indicates how many FGN packets will be sent

FGN|uint32_t packet length|uint32_t seq|file line
    packet containing line of file

### Misc
GDB: acknowledge clients exit

## Client
### Command packets
EXT: notify the server that the client is done

GFL|uint32_t packet_length|filename
    get specific file

LST: get a list of files on server

FPS|uint32_t packet_length|uint32_t file line count|filename
    put a file on the server

FPN|uint32_t packet_length|uint32_t seq|file line
