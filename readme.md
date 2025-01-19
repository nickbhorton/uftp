# Protocol
## Server
### Responses
'FLS'|uint32_t number_filenames to be sent
    indicates how many FLN packets will be sent

FLN|uint32_t packet length|uint32_t seq|filename
    filename sent for 'ls' command

FGS|uint32_t number lines to be sent
    indicates how many FGN packets will be sent

FGN|uint32_t packet length|uint32_t seq|file line
    packet containing line of file

**Goodbye**
Sent to client on client exit command
```mermaid
packet-beta
0-23: "'GDB'"
```
### Errors
**Invalid Command**
Sent to client if command is not recognized.
```mermaid
packet-beta
0-23: "'CER'"
```

## Client
### Command packets
**Exit**
Notify the server that the client is done
```mermaid
packet-beta
0-23: "'EXT'"
```

GFL|uint32_t packet_length|filename
    get specific file

LST: get a list of files on server

FPS|uint32_t packet_length|uint32_t file line count|filename
    put a file on the server

FPN|uint32_t packet_length|uint32_t seq|file line
