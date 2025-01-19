# Protocol
## Server
### Responses
'FLS'|'uint32_t' number_filenames to be sent
    indicates how many FLN packets will be sent

FLN|'uint32_t' packet_length|'uint32_t' seq|filename_str
    filename sent for 'ls' command

FGS|'uint32_t' number_lines
    indicates how many FGN packets will be sent

FGN|'uint32_t' packet_length|'uint32_t' seq|file line
    packet containing line of file

GDB
    Sent to client on client exit command

### Errors
CER
    Sent to client if command is not recognized.

## Client
### Command packets
EXT
    Notify the server that the client is done

GFL|'uint32_t' packet_length|filename_str
    get specific file

LST: get a list of files on server

FPS|'uint32_t' packet_length|'uint32_t' line_count|filename_str
    put a file on the server

FPN|'uint32_t' packet_length|'uint32_t' seq|line_str
