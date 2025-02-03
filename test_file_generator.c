#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * this is not tested and should not be used outside of the testing suit
 */
int main(int argv, char** argc)
{
    char* filename = argc[1];
    size_t size = atoi(argc[2]);
    size_t byte_count = 0;
    FILE* fptr = fopen(filename, "w");
    if (fptr == NULL) {
        fprintf(stderr, "fopen failed\n");
        return -1;
    }
    while (byte_count < size) {
        if (rand() % 100 == 0) {
            fputc('\n', fptr);
        } else {
            fputc((rand() % sizeof(char)), fptr);
        }
        byte_count++;
    }
    int rv = fclose(fptr);
    return rv;
}
