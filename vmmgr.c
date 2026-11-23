#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// TLB replacement policy options
#define TLB_LRU 0
#define TLB_RANDOM 1

// page replacement policy options
#define PAGE_FIFO 0
#define PAGE_LRU 1
#define PAGE_RANDOM 2

// define page table with 256 entries
int pageTable[256];

// define physical memory with 256 frames
signed char physicalMemory[256][256];

// define TLB with 16 entries
int tlbPage[16];
int tlbFrame[16];

// keeps track of when each TLB entry was last used
int tlbLastUsed[16];

// stores the number of frames in physical memory (defaults to 256)
int numFrames = 256;

// keeps track of which page is stored in each frame
int frameToPage[256];

// keeps track of when each frame was last used
int frameLastUsed[256];

// keeps track of the next frame to replace for FIFO
int fifoNext = 0;

// finds the least recently used TLB entry
int findLRU(int tlbLastUsed[]);

// finds a random TLB entry
int findRandom();

// finds the oldest frame using FIFO
int findFIFO();

// finds the least recently used physical memory frame
int findLRUFrame(int frameLastUsed[]);

// finds a random physical memory frame
int findRandomFrame();

int main(int argc, char *argv[]) {

    // file pointer
    FILE *file;

    // backing store pointer
    FILE *backingStore;

    // csv file pointer
    FILE *csvFile;

    // stores the csv file name (defaults to results.csv)
    char *csvName = "results.csv";

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
    int byteValue;

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
    // keeps track of the current address access
    int accessCounter = 0;

    // stores page fault rate
    double pageFaultRate;
    // stores TLB hit rate
    double tlbHitRate;

    // stores the TLB replacement policy (defaults to LRU)
    int tlbPolicy = TLB_LRU;

    // stores the page replacement policy (defaults to FIFO)
    int pagePolicy = PAGE_FIFO;

    // stores the number of arguments entered
    int nargs = argc - 1;

    // check to see if input file is provided
    if (nargs % 2 == 1 && nargs <= 9) {
        // read each option provided after the input file
        for (int i = 2; i < argc; i += 2) {
            if (strcmp(argv[i], "-tlb") == 0) {
                // set the TLB policy
                if (strcmp(argv[i + 1], "lru") == 0) {
                    tlbPolicy = TLB_LRU;
                } else if (strcmp(argv[i + 1], "random") == 0) {
                    tlbPolicy = TLB_RANDOM;
                } else {
                    fprintf(stderr, "Unknown TLB policy: %s\n", argv[i + 1]);
                    return 1;
                }
            } else if (strcmp(argv[i], "-page") == 0) {
                // set the page replacement policy
                if (strcmp(argv[i + 1], "fifo") == 0) {
                    pagePolicy = PAGE_FIFO;
                } else if (strcmp(argv[i + 1], "lru") == 0) {
                    pagePolicy = PAGE_LRU;
                } else if (strcmp(argv[i + 1], "random") == 0) {
                    pagePolicy = PAGE_RANDOM;
                } else {
                    fprintf(stderr, "Unknown page policy: %s\n", argv[i + 1]);
                    return 1;
                }
            } else if (strcmp(argv[i], "-csv") == 0) {
                // set the csv file name
                csvName = argv[i + 1];
            } else if (strcmp(argv[i], "-frames") == 0) {
                // set the number of frames
                char *end;
                long frames = strtol(argv[i + 1], &end, 10);

                // number of frames must be a number from 1 to 256
                if (*end != '\0' || frames < 1 || frames > 256) {
                    fprintf(stderr, "Invalid number of frames: %s (must be 1-256)\n", argv[i + 1]);
                    return 1;
                }

                numFrames = frames;
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                return 1;
            }
        }

        // if file is provided then it is read
        file = fopen(argv[1], "r");

        // if file does not exist it prints error message
        if (file == NULL) {
            fprintf(stderr, "Could not open file.\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Usage: %s <input file> [-tlb lru|random] [-page fifo|lru|random] [-frames N] [-csv file]\n", argv[0]);
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

    // open csv file in write mode
    csvFile = fopen(csvName, "w");

    // check if csv file could be created
    if (csvFile == NULL) {
        fprintf(stderr, "Could not create csv file.\n");
        fclose(file);
        fclose(backingStore);
        return 1;
    }

    // write csv header
    fprintf(csvFile, "logical_address,page_number,offset,tlb,page_fault,frame_number,physical_address,value,replaced_page,replaced_frame\n");

    // seed the random number generator for the random TLB policy
    srand(time(NULL));

    // initialize all entries in page table to -1
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
        // gives address an access timestamp
        accessCounter++;

        // mask the logical address to the rightmost 16 bits
        logicalAddress = logicalAddress & 0xFFFF;

        // extract offset
        offset = logicalAddress & 0xFF;

        // extract page number
        pageNum = logicalAddress >> 8;

        // search TLB for page number
        int tlbHit = 0;

        // keeps track of whether this address caused a page fault
        int pageFault = 0;

        // keeps track of the page replaced by this address (-1 if none)
        int replacedPage = -1;

        for (int i = 0; i < 16; i++) {
            if (tlbPage[i] == pageNum) {
                frameNum = tlbFrame[i];
                tlbLastUsed[i] = accessCounter;
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
                if (nextFrame < numFrames) {
                    // use the next free frame
                    frameNum = nextFrame;
                    nextFrame++;
                } else {
                    // if memory is full, replace a frame based on the page policy
                    if (pagePolicy == PAGE_FIFO) {
                        frameNum = findFIFO();
                    } else if (pagePolicy == PAGE_LRU) {
                        frameNum = findLRUFrame(frameLastUsed);
                    } else {
                        frameNum = findRandomFrame();
                    }

                    // remove the replaced page from the page table
                    replacedPage = frameToPage[frameNum];
                    pageTable[replacedPage] = -1;

                    // remove the replaced page from the TLB
                    for (int i = 0; i < 16; i++) {
                        if (tlbPage[i] == replacedPage) {
                            tlbPage[i] = -1;
                            tlbFrame[i] = -1;
                        }
                    }
                }

                // locate page number
                if (fseek(backingStore, pageNum * 256, SEEK_SET) != 0) {
                    fprintf(stderr, "Error locating page in backing store.\n");
                    fclose(file);
                    fclose(backingStore);
                    fclose(csvFile);
                    return 1;
                }

                // load page number in physical memory
                if (fread(physicalMemory[frameNum], sizeof(signed char), 256, backingStore) != 256) {
                    fprintf(stderr, "Error reading page from backing store.\n");
                    fclose(file);
                    fclose(backingStore);
                    fclose(csvFile);
                    return 1;
                }

                // update page number in page table
                pageTable[pageNum] = frameNum;
                frameToPage[frameNum] = pageNum;
                pageFaults++;
                pageFault = 1;
            }

            // add page number and frame number to TLB
            int emptyIndex = -1;

            for (int i = 0; i < 16; i++) {
                if (tlbPage[i] == -1) {
                    emptyIndex = i;
                    break;
                }
            }

            // if there is an empty entry, use it
            if (emptyIndex != -1) {
                tlbPage[emptyIndex] = pageNum;
                tlbFrame[emptyIndex] = frameNum;
                tlbLastUsed[emptyIndex] = accessCounter;
            } else {
                // if TLB is full, replace an entry based on the TLB policy
                int replaceIndex;

                if (tlbPolicy == TLB_LRU) {
                    // replace least recently used entry
                    replaceIndex = findLRU(tlbLastUsed);
                } else {
                    // replace random entry
                    replaceIndex = findRandom();
                }

                tlbPage[replaceIndex] = pageNum;
                tlbFrame[replaceIndex] = frameNum;
                tlbLastUsed[replaceIndex] = accessCounter;
            }
        }

        // calculate physical address
        physicalAddress = (frameNum * 256) + offset;

        // retrieve byte value
        byteValue = physicalMemory[frameNum][offset];

        // update the last used timestamp for this frame
        frameLastUsed[frameNum] = accessCounter;

        // write address translation to csv file
        fprintf(csvFile, "%u,%d,%d,%s,%s,%d,%u,%d,", logicalAddress, pageNum, offset, tlbHit == 1 ? "hit" : "miss", pageFault == 1 ? "yes" : "no", frameNum, physicalAddress, byteValue);

        // if a page was replaced, record the replaced page and frame
        if (replacedPage != -1) {
            fprintf(csvFile, "%d,%d\n", replacedPage, frameNum);
        } else {
            fprintf(csvFile, ",\n");
        }

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

    // debug messages
    // if (tlbMisses + tlbHits == totalAddresses) {
    //     printf("TLB Misses + TLB Hits = total addresses\n");
    // } else {
    //     printf("TLB Misses + TLB Hits != total addresses\n");
    // }

    // print stats
    printf("Page Faults: %d\n", pageFaults);
    printf("Total Addresses: %d\n", totalAddresses);
    printf("Page Fault Rate: %.3f\n", pageFaultRate);

    printf("TLB Hits: %d\n", tlbHits);
    printf("TLB Hit Rate: %.3f\n", tlbHitRate);

    // close input file
    fclose(file);

    // close backing store file
    fclose(backingStore);

    // close csv file
    fclose(csvFile);

    return 0;
}

// finds the least recently used TLB entry
int findLRU(int tlbLastUsed[]) {
    int replaceIndex = 0;

    for (int i = 1; i < 16; i++) {
        if (tlbLastUsed[i] < tlbLastUsed[replaceIndex]) {
            replaceIndex = i;
        }
    }

    return replaceIndex;
}

// finds a random TLB entry
int findRandom() {
    // pick a random index between 0 and 15
    return rand() % 16;
}

// finds the oldest frame using FIFO
int findFIFO() {
    // the oldest frame is the next one in line
    int replaceFrame = fifoNext;

    // move to the next oldest frame, wrapping around
    fifoNext = (fifoNext + 1) % numFrames;

    return replaceFrame;
}

// finds the least recently used physical memory frame
int findLRUFrame(int frameLastUsed[]) {
    int replaceFrame = 0;

    for (int i = 1; i < numFrames; i++) {
        if (frameLastUsed[i] < frameLastUsed[replaceFrame]) {
            replaceFrame = i;
        }
    }

    return replaceFrame;
}

// finds a random physical memory frame
int findRandomFrame() {
    // pick a random frame between 0 and numFrames - 1
    return rand() % numFrames;
}