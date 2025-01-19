#ifndef UFTP_HEADER
#define UFTP_HEADER

// directly from beej.us with no changes
void* get_in_addr(struct sockaddr* sa);

int get_udp_socket(const char* addr, const char* port);

#endif
