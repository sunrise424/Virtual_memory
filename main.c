#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>

#define FRAME_SIZE 256        
#define TOTAL_NUMBER_OF_FRAMES 256  
#define ADDRESS_MASK  0xFFFF  
#define OFFSET_MASK  0xFF 
#define TLB_SIZE 16       
#define PAGE_TABLE_SIZE 256  
#define BUFFER_SIZE      10     // number of characters to read for each line from input file
#define NUMBER_BYTE_READ 256   // number of bytes to read

int pageTableNumbers[PAGE_TABLE_SIZE];  
int pageTableFrames[PAGE_TABLE_SIZE];   

int TLBPageNumber[TLB_SIZE];  
int TLBFrameNumber[TLB_SIZE]; 

int physicalMemory[TOTAL_NUMBER_OF_FRAMES][FRAME_SIZE]; 

//counter to track
int pageFaults = 0;   
int TLBHits = 0;      
int firstAvailableFrame = 0;  
int firstAvailablePTN = 0;  // firstAvailablePageTableNumber
int TLBEntries = 0; // number Of TLB Entries            


// input file and backing store
FILE    *address_file;
FILE    *backing_store;

// how we store reads from input file
char    address[BUFFER_SIZE];
int     logical_address;

signed char buffer[NUMBER_BYTE_READ];
signed char value;   // value in memory

void getPage(int address);
void readFromStore(int pageNmber);
void insertIntoTLB(int pageNumber, int frameNumber);

// function to take the logical address and obtain the physical address and value stored at that address
void getPage(int logical_address){
    
	int PN = ((logical_address & ADDRESS_MASK)>>8);  //get page number
    int offset = (logical_address & OFFSET_MASK);    // get offset
    
    // first try to get page from TLB
    int FN = -1; 
    
    int i;  // check TLB is match
    for(i = 0; i < TLB_SIZE; i++){
        if(TLBPageNumber[i] == PN){   
            FN = TLBFrameNumber[i];  
                TLBHits++;                
        }
    }
    
    // if the frameNumber was not found
    if(FN == -1){
        int i;   
        for(i = 0; i < firstAvailablePTN; i++){
            if(pageTableNumbers[i] == PN){         
                FN = pageTableFrames[i];          
            }
        }
        if(FN == -1){                   
            readFromStore(PN);             
            pageFaults++;                         
            FN = firstAvailableFrame - 1;  
        }
    }
    
    insertIntoTLB(PN, FN);  //  insert the page number and frame number into the TLB
    value = physicalMemory[FN][offset];  
	
  // printf("frame number: %d\n", FN);
 	printf("logical address: %5u (page:%3u, offset:%3u) ---> physical address: %5u Value: %d-- passed\n", logical_address, PN, offset, (FN << 8) | offset, value);
    if (FN % 5 == 0) { printf("\n"); }
}

// insert a page number and frame number into the TLB with a FIFO replacement
void insertIntoTLB(int PN, int FN){
    
     int i; // if is in the TLB, break
    for(i = 0; i < TLBEntries; i++){
        if(TLBPageNumber[i] == PN){
            break;
        }
    }
    
    
    if(i == TLBEntries){
        if(TLBEntries < TLB_SIZE){  // if TLB still has room in it
            TLBPageNumber[TLBEntries] = PN;    // insert the page and frame on the end
            TLBFrameNumber[TLBEntries] = FN;
        }
        else{                                            // otherwise move everything over
            for(i = 0; i < TLB_SIZE - 1; i++){
                TLBPageNumber[i] = TLBPageNumber[i + 1];
                TLBFrameNumber[i] = TLBFrameNumber[i + 1];
            }
            TLBPageNumber[TLBEntries-1] = PN;  // and insert the page and frame on the end
            TLBFrameNumber[TLBEntries-1] = FN;
        }        
    }
    
   
    else{
        for(i = i; i < TLBEntries - 1; i++){      // iterate 
            TLBPageNumber[i] = TLBPageNumber[i + 1];      // move everything over 
            TLBFrameNumber[i] = TLBFrameNumber[i + 1];
        }
        if(TLBEntries < TLB_SIZE){                // if has room , put the page and frame on the end
            TLBPageNumber[TLBEntries] = PN;
            TLBFrameNumber[TLBEntries] = FN;
        }
        else{                                      // otherwise put the page and frame on the number of entries - 1
            TLBPageNumber[TLBEntries-1] = PN;
            TLBFrameNumber[TLBEntries-1] = FN;
        }
    }
    if(TLBEntries < TLB_SIZE){       // has room, increment 
        TLBEntries++;
    }    
}

// read backing store and bring the frame into physical memory and the page table
void readFromStore(int PN){
    
    if (fseek(backing_store, PN * NUMBER_BYTE_READ, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store\n");
    }
    
    // read from the backing store to the buffer
    if (fread(buffer, sizeof(signed char), NUMBER_BYTE_READ, backing_store) == 0) {
        fprintf(stderr, "Error reading from backing store\n");        
    }
    
    // load the bits into the first available frame in the physical memory 
    int i;
    for(i = 0; i < NUMBER_BYTE_READ; i++){
        physicalMemory[firstAvailableFrame][i] = buffer[i];
    }
    
    // and then load the frame number into the page table in the first available frame
    pageTableNumbers[firstAvailablePTN] = PN;
    pageTableFrames[firstAvailablePTN] = firstAvailableFrame;
       
    firstAvailableFrame++;
    firstAvailablePTN++;
}

int main(int argc, const char *argv[])
{
	printf("\t\t\t====================================\n" );
    printf("\t\t\t||    Virtual Memory Manager      ||\n" );
    printf("\t\t\t====================================\n" );

    printf("\t\t\t Translating 1000 Logical Addresses: \n\n");
	
    //  basic error checking
    if (argc != 2) {
        fprintf(stderr,"Usage: ./a.out [input file]\n");
        return -1;
    }
    
    // open the file containing the backing store
    backing_store = fopen("BACKING_STORE.bin", "rb");
    
    if (backing_store == NULL) {
        fprintf(stderr, "Error opening BACKING_STORE.bin %s\n","BACKING_STORE.bin");
        return -1;
    }
    
    // open the file containing the logical addresses
    address_file = fopen(argv[1], "r");
    
    if (address_file == NULL) {
        fprintf(stderr, "Error opening addresses.txt %s\n",argv[1]);
        return -1;
    }
    
    int n = 0;  // number Of Translated Addresses
	
    // read through the input file and output each logical address
    while ( fgets(address, BUFFER_SIZE, address_file) != NULL) {
        logical_address = atoi(address);
        
        // get the physical address and value stored at that address
        getPage(logical_address);
        n++;         
    }
    
	printf("ALL logical ---> physical assertions PASSED!\n\n");
   
    printf("Number of translated addresses = %d\n", n);
    double PFRate = pageFaults / (double)n;
    double TLBRate = TLBHits / (double)n;
    
    printf("Page Faults = %d\n", pageFaults);
    printf("Page Fault Rate = %.4f\n",PFRate);
    printf("TLB Hits = %d\n", TLBHits);
    printf("TLB Hit Rate = %.4f\n", TLBRate);
       
    fclose(address_file);
    fclose(backing_store);
    
    return 0;
}
