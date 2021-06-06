#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>


// definitions
#define TLB_ENTRIES 16 // Max TLB entries.
#define PAGE_SIZE 256 //  in bytes.
#define PAGE_ENTRIES 256 // Max entries.
#define PAGE_BITS 8 // in bits.
#define FRAME_SIZE 256 //n bytes.
//#define FRAME_ENTRIES 128 // Number of frames in physical memory.




// attributes
int virtual_address;
int page_number;
int offset;
int frame_number;
signed char *value;
char line[16];
int physical_address;

int mem_index = 0;
int tlb_index = 0;
int page_table_index = 0;


// methods
int page_look_up(int pg_num, char* where);
void insert_into_tlb(int pg_num, int frm_num);
void insert_into_page_table(int pg_num, int frm_num);
void initialize();
void print_page_table();




// A page consist of a page number and a corresponding frame number
typedef struct 
{
  int page_number;
  int frame_number;
} PAGES;


// data structures
//signed char physical_memory[FRAME_ENTRIES][FRAME_SIZE];   // physical memory
PAGES page_table[PAGE_SIZE];
PAGES tlb[TLB_ENTRIES];


//stats attributes
int address_count = 0;
int page_fault = 0;
int tlb_hit = 0;


int main(int argc, char *argv[]) {

    // dynamic physical memory based on user's command line arguments
    

    int FRAME_ENTRIES = atoi(argv[1]);  
    signed char physical_memory[FRAME_ENTRIES][FRAME_SIZE];  

    // files we read from argv[]
    FILE *addresses;
    FILE *bin;

    // output file
    FILE* output;

    if(strcmp(argv[1], "128") == 0){
        
        output = fopen("output128.csv", "w+");
    }    
    else
    {
        output = fopen("output256.csv", "w+");
        
    }
        
    if(argc != 4){
        printf("error");

    }
    else{
        
        addresses = fopen(argv[3], "r");
        bin = fopen(argv[2], "rb");

        initialize();
        // looping the address file to extract the values
        while(fgets(line, sizeof(line), addresses)){
            address_count++;
            virtual_address = atoi(line); // string to integer conversion
            page_number = (virtual_address >> PAGE_BITS); // MSB 8 bits
            offset = virtual_address & 255; // LSB 8 bits


            frame_number = page_look_up(page_number, "TLB");
            
            // TLB miss
            if(frame_number == -1){
                frame_number = page_look_up(page_number, "PAGE_TABLE");
                

                //page fault
                if(frame_number == -1){
                    page_fault++;
                    
                    //move the cursor inside backing store to the corresponsing page
                     fseek(bin, page_number * PAGE_SIZE, SEEK_SET);

                    //MEMORY NOT FULL
                    if(mem_index < FRAME_ENTRIES){
                        frame_number = mem_index;
                        mem_index++;

                        //read the page bytes into the physical memory
                        fread(physical_memory[frame_number], sizeof(signed char), FRAME_SIZE, bin);
                        insert_into_tlb(page_number, frame_number);
                        insert_into_page_table(page_number, frame_number);

                    }

                    //MEMORY FULL
                    else
                    {
                        frame_number = page_table[0].frame_number;
                        page_table[0].page_number = page_number;

                        //move to page to the bottom of page table to update the LRU
                        int index;
                        for(index = 0; index < (page_table_index - 1); index++){
                            page_table[index].page_number = page_table[index + 1].page_number;
                            page_table[index].frame_number = page_table[index + 1].frame_number;
                        }
                        page_table[page_table_index - 1].page_number = page_number;
                        page_table[page_table_index - 1].frame_number = frame_number;

                        //read the page bytes into the physical memory
                        fread(physical_memory[frame_number], sizeof(signed char), FRAME_SIZE, bin);
                        insert_into_tlb(page_number, frame_number);

                    }
                
                    
                    
                }
                //no page fault i.e page exist in page table
                else{
                    insert_into_tlb(page_number, frame_number);

                    int index, old_page, old_frame;
                    for(index = 0; index < page_table_index; index++){
                        if(page_table[index].frame_number == frame_number){

                            old_page = page_table[index].page_number;
                            old_frame = page_table[index].frame_number;

                            int i = index;
                            while(i < (page_table_index - 1)){
                                page_table[i].page_number = page_table[i + 1].page_number;
                                page_table[i].frame_number = page_table[i + 1].frame_number;
                                i++;
                            }
                            page_table[page_table_index - 1].page_number = old_page;
                            page_table[page_table_index - 1].frame_number = old_frame;

                        }

                    }
                }

            }
            else{
                //printf("Page hit = %d\n", page_number);
                tlb_hit++;

                // move the page entry to the end of the page table as its the most recenlty used
                // and shift all entries to the left

                int index, old_page, old_frame;
                for(index = 0; index < page_table_index; index++){
                    if(page_table[index].frame_number == frame_number){

                        old_page = page_table[index].page_number;
                        old_frame = page_table[index].frame_number;

                        int i = index;
                        while(i < (page_table_index - 1)){
                            page_table[i].page_number = page_table[i + 1].page_number;
                            page_table[i].frame_number = page_table[i + 1].frame_number;
                            i++;
                        }
                        page_table[page_table_index - 1].page_number = old_page;
                        page_table[page_table_index - 1].frame_number = old_frame;

                    }

                }
   
               
            }

            physical_address = ((frame_number << 8) + offset);
            value = physical_memory[frame_number][offset];
            fprintf(output, "%d,%d,%d\n", virtual_address, physical_address, value);
            
        }
    // print_page_table();
    fprintf(output, "Page Faults Rate, %.2f%%,\n", 100*((double) page_fault / (double) address_count));
    fprintf(output, "TLB Hits Rate, %.2f%%,", 100*((double) tlb_hit / (double) address_count));
    fclose(addresses);
    fclose(bin);
    fclose (output);
    return 0;

    }

}

void initialize(){

    int i, j;

    for(i = 0; i < TLB_ENTRIES; i++){
        tlb[i].page_number = -1;
        tlb[i].frame_number = -1;
    }
    for(j = 0; j < PAGE_ENTRIES; j++){
        page_table[j].page_number = -1;
        page_table[j].frame_number = -1;
    }
  
 
}



int page_look_up(int pg_num, char* where){
    int index = 0;

    // consult TLB
    if(strcmp(where, "TLB") == 0){
        for(; index < TLB_ENTRIES; index++){

            if((tlb[index]).page_number == pg_num){
                return (tlb[index]).frame_number;

            }
            
        }

    }
    // consult PAGE TABLE
    if(strcmp(where, "PAGE_TABLE") == 0){

        for(; index < page_table_index; index++){

            if((page_table[index]).page_number == pg_num){
                return (page_table[index]).frame_number;

            }
            
        }

    }   
    
    return -1;
    
}

void insert_into_tlb(int pg_num, int frm_num){

    (tlb[tlb_index]).page_number = pg_num;
    (tlb[tlb_index]).frame_number = frm_num;

    tlb_index++;
    tlb_index = tlb_index % TLB_ENTRIES;


}

void insert_into_page_table(int pg_num, int frm_num){

    (page_table[page_table_index]).page_number = pg_num;
    (page_table[page_table_index]).frame_number = frm_num;

    page_table_index++ ;
    page_table_index = page_table_index % PAGE_ENTRIES;

}

void print_page_table()
{
  int i = 0;
  
  printf("\nPage Table\n");
  
  for(; i < PAGE_ENTRIES; i++)
  {
    printf("[%d] [%d]\n", page_table[i].page_number, page_table[i].frame_number);
  }
  
  printf("\n");
}