#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "Cache.h"

void print_parameters_block(const char *trace_file, const cache_params &params);

int main (int argc, char* argv[])
{
    FILE *FP;               // filepointer
    char *trace_file;       // Path to trace file
    cache_params params;    // Parameters struct
    char rw;                // 2-wide char array with r/w switch in position [0]
    unsigned long int addr; // address

    if(argc != 8)           // Validate input parameter quantity
    {
        printf("Error: Expected inputs:7 Given inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }

    params.block_size       = strtoul(argv[1], nullptr, 10);
    params.l1_size          = strtoul(argv[2], nullptr, 10);
    params.l1_assoc         = strtoul(argv[3], nullptr, 10);
    params.vc_num_blocks    = strtoul(argv[4], nullptr, 10);
    params.l2_size          = strtoul(argv[5], nullptr, 10);
    params.l2_assoc         = strtoul(argv[6], nullptr, 10);
    trace_file              = argv[7];

    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == nullptr)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }

    //Instantiate cache hierarchy
    Cache L1 = Cache(params, 0x01);

    // Print params
   print_parameters_block(trace_file, params);

   char str[2];
   // Parse tracefile; for each memory action, call read/write to memory hierarchy
    while(fscanf(FP, "%s %lx", str, &addr) != EOF)
    {
        rw = str[0];
        if (rw == 'r')
           L1.read(addr);
        else if(rw == 'w')
           L1.write(addr);
    }

    // Report on simulation results and statistics (recursively calls self up the hierarchy)
    L1.contents_report();
    L1.statistics_report();

    return EXIT_SUCCESS;
}

void print_parameters_block(const char *trace_file, const cache_params &params) {
   std::string temp_string;
   std::string params_string = "===== Simulator configuration =====\n  BLOCKSIZE:    ";
   temp_string = std::to_string(params.block_size);
   Cache::cat_padded(&params_string, &temp_string);

   params_string += "  L1_SIZE:      ";
   temp_string = std::to_string(params.l1_size);
   Cache::cat_padded(&params_string, &temp_string);

   params_string += "  L1_ASSOC:     ";
   temp_string = std::to_string(params.l1_assoc);
   Cache::cat_padded(&params_string, &temp_string);

   params_string += "  VC_NUM_BLOCKS:";
   temp_string = std::to_string(params.vc_num_blocks);
   Cache::cat_padded(&params_string, &temp_string);

   params_string += "  L2_SIZE:      ";
   temp_string = std::to_string(params.l2_size);
   Cache::cat_padded(&params_string, &temp_string);

   params_string += "  L2_ASSOC:     ";
   temp_string = std::to_string(params.l2_assoc);
   Cache::cat_padded(&params_string, &temp_string);

   params_string += "  trace_file:   ";
   temp_string = trace_file;
   Cache::cat_padded(&params_string, &temp_string);

   params_string += "\n";

   std::cout << params_string;
}
