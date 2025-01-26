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

# Design decisions
The principal idea is that the server never hangs on a specific read. It is the
clients job to understand what is going on in the server. The server only waits
on poll(), reads socket, responds immediately with 'n' number of packets.

The server will store some information about the client to successfully
implement this idea. For example when the server is receiving a file, a
filename is associated with the clients address so when a FLQ packet is
received it knows what file to write that chunk too. Or another example the
server stores the progress of a file being written so when the last successful
file chunk is sent it knows to also send a packet notifying the client that the
file write was a success.

This idea also allows for multiple clients and one server. If two clients send
very large files at the same time the server understands which packets are
accosted with which clients and thus knows what files to write to.
