#include <stdio.h>
#include <stdlib.h>

// define page table with 256 entries
int pageTable[256];

// define physical memory with 256 frames
signed char physicalMemory[256][256];

int main(int argc, char *argv[]) {

    // file pointer
    FILE *file;

    // stores logical address
    unsigned int logicalAddress;

    // stores page number
    int pageNum;
    // stores offset
    int offset;
    // stores frame number
    int frameNum;
    // keeps track of next frame
    int nextFrame = 0;

    // stores the number of arguments entered
    int nargs = argc - 1;

    // check to see if input file is provided
    if (nargs == 1) {

        // if file is provided then it is read
        file = fopen(argv[1], "r");

        // if file does not exist it prints error message
        if (file == NULL) {
            fprintf(stderr, "Could not open file.\n");
            return 1;
        }

    } else {
        fprintf(stderr, "Please provide an input filename.\n");
        return 1;
    }

    // intialize all entries in page table to -1
    for (int i = 0; i < 256; i++){
        pageTable[i] = -1;
    }


    // read each logical address from input file
    while (fscanf(file, "%u", &logicalAddress) == 1) {
       // mask the logical address to the rightmost 16 bits
        logicalAddress = logicalAddress & 0xFFFF;

        // extract offset
        offset = logicalAddress & 0xFF;

        // extract page number
        pageNum = logicalAddress >> 8;

        if (pageTable[pageNum] != -1){
            frameNum = pageTable[pageNum];
        } else {
            // load page into physical memory when page fault occurs
            frameNum = nextFrame;
            pageTable[pageNum] = frameNum;
            nextFrame++;
        }
    }

    // close input file
    fclose(file);

    return 0;
}