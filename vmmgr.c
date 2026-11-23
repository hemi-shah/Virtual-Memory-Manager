
#include <stdio.h>
#include <stdlib.h>

// define page table with 256 entries
int pageTable[256];

// define physical memory with 256 frames
signed char physicalMemory[256][256];

// define TLB with 16 entries
int tlbPage[16];
int tlbFrame[16];

int main(int argc, char *argv[]) {

    // file pointer
    FILE *file;

    // backing store pointer
    FILE *backingStore;

    // stores logical address
    unsigned int logicalAddress;
    // stores physical address
    unsigned int physicalAddress;

    // stores page number
    int pageNum;
    // stores offset
    int offset;
    // stores frame number
    int frameNum;
    // stores byte value
    signed int byteValue;

    // keeps track of next frame
    int nextFrame = 0;
    // keeps track of total address updates
    int totalAddresses = 0;
    // keeps track of number of page faults
    int pageFaults = 0;
    // keeps track of number of TLB hits
    int tlbHits = 0;
    // keeps track of number of TLB misses
    int tlbMisses = 0;

    // stores page fault rate
    double pageFaultRate;
    // stores TLB hit rate
    double tlbHitRate;

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

    // open backing store in read-binary mode
    backingStore = fopen("BACKING_STORE.bin", "rb");

    // check if backing store exists
    if (backingStore == NULL) {
        fprintf(stderr, "Could not open backing store.\n");
        fclose(file);
        return 1;
    }

    // intialize all entries in page table to -1
    for (int i = 0; i < 256; i++) {
        pageTable[i] = -1;
    }

    // initialize all TLB entries to -1
    for (int i = 0; i < 16; i++) {
        tlbPage[i] = -1;
        tlbFrame[i] = -1;
    }

    // read each logical address from input file
    while (fscanf(file, "%u", &logicalAddress) == 1) {
        // increment total number of addresses processed
        totalAddresses++;

        // mask the logical address to the rightmost 16 bits
        logicalAddress = logicalAddress & 0xFFFF;

        // extract offset
        offset = logicalAddress & 0xFF;

        // extract page number
        pageNum = logicalAddress >> 8;

        // search TLB for page number
        int tlbHit = 0;

        for (int i = 0; i < 16; i++) {
            if (tlbPage[i] == pageNum) {
                frameNum = tlbFrame[i];
                tlbHit = 1;
                break;
            }
        }

        // if TLB hit, increment TLB hits
        if (tlbHit == 1) {
            tlbHits++;
        }

        // if TLB miss, check page table
        if (tlbHit == 0) {
            tlbMisses++;

            if (pageTable[pageNum] != -1) {
                frameNum = pageTable[pageNum];
            } else {
                // load page into physical memory when page fault occurs
                frameNum = nextFrame;

                // locate page number
                if (fseek(backingStore, pageNum * 256, SEEK_SET) != 0) {
                    fprintf(stderr, "Error locating page in backing store.\n");
                    fclose(file);
                    fclose(backingStore);
                    return 1;
                }

                // load page number in physical memory
                if (fread(physicalMemory[frameNum], sizeof(signed char), 256, backingStore) != 256) {
                    fprintf(stderr, "Error reading page from backing store.\n");
                    fclose(file);
                    fclose(backingStore);
                    return 1;
                }

                // update page number in page table
                pageTable[pageNum] = frameNum;
                nextFrame++;
                pageFaults++;
            }

            // add page number and frame number to TLB
            for (int i = 0; i < 16; i++) {
                if (tlbPage[i] == -1) {
                    tlbPage[i] = pageNum;
                    tlbFrame[i] = frameNum;
                    break;
                }
            }
        }

        // calculate physical address
        physicalAddress = (frameNum * 256) + offset;

        // retrieve byte value
        byteValue = physicalMemory[frameNum][offset];

        // print address translation
        printf("Logical Address: %u Physical Address: %u Value: %d\n", logicalAddress, physicalAddress, byteValue);
    }

    // calculate page fault rate
    if (totalAddresses > 0) {
        pageFaultRate = (double) pageFaults / totalAddresses;
        tlbHitRate = (double) tlbHits / totalAddresses;
    } else {
        pageFaultRate = 0.0;
        tlbHitRate = 0.0;
    }

    if (tlbMisses + tlbHits == totalAddresses){
        printf("TLB Misses + TLB Hits = total addresses\n");
    } else {
        printf("TLB Misses + TLB Hits != total addresses\n");
    }

    printf("Page Faults: %d\n", pageFaults);
    printf("Total Addresses: %d\n", totalAddresses);
    printf("Page Fault Rate: %.3f\n", pageFaultRate);

    printf("TLB Hits: %d\n", tlbHits);
    printf("TLB Hit Rate: %.3f\n", tlbHitRate);

    // close input file
    fclose(file);

    // close backing store file
    fclose(backingStore);

    return 0;
}