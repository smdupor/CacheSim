#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "Cache.h"


/* Argument Reminders:
 *
 * sim_cache   <BLOCKSIZE>   <L1_SIZE>  <L1_ASSOC>  <VC_NUM_BLOCKS>   <L2_SIZE>  <L2_ASSOC>   <trace_file>
 *
 */

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
   Cache L1 = Cache(params, 0x01);
    // Print params
   /* printf("  ===== Simulator configuration =====\n"
            "  L1_BLOCKSIZE:                     %lu\n"
            "  L1_SIZE:                          %lu\n"
            "  L1_ASSOC:                         %lu\n"
            "  VC_NUM_BLOCKS:                    %lu\n"
            "  L2_SIZE:                          %lu\n"
            "  L2_ASSOC:                         %lu\n"
            "  trace_file:                       %s\n"
            "  ===================================\n\n", params.block_size, params.l1_size, params.l1_assoc,
            params.vc_num_blocks, params.l2_size, params.l2_assoc, trace_file);*/
            std::string str_tmp="";
   std::string pblk = "===== Simulator configuration =====\n  BLOCKSIZE:    ";

            str_tmp = std::to_string(params.block_size);
            L1.cat_padded(&pblk,&str_tmp);
   pblk += "  L1_SIZE:      ";
   str_tmp = std::to_string(params.l1_size);
   L1.cat_padded(&pblk,&str_tmp);
   pblk += "  L1_ASSOC:     ";
   str_tmp = std::to_string(params.l1_assoc);
   L1.cat_padded(&pblk,&str_tmp);
   pblk += "  VC_NUM_BLOCKS:";
   str_tmp = std::to_string(params.vc_num_blocks);
   L1.cat_padded(&pblk,&str_tmp);
   pblk += "  L2_SIZE:      ";
   str_tmp = std::to_string(params.l2_size);
   L1.cat_padded(&pblk,&str_tmp);
   pblk += "  L2_ASSOC:     ";
   str_tmp = std::to_string(params.l2_assoc);
   L1.cat_padded(&pblk,&str_tmp);
   pblk += "  trace_file:   ";
   str_tmp = trace_file;
   L1.cat_padded(&pblk,&str_tmp);
   pblk += "\n";

   std::cout << pblk;

    char str[2];
   long long count = 0;


    while(fscanf(FP, "%s %lx", str, &addr) != EOF)
    {
        
        rw = str[0];
       /* if (rw == 'r')
            printf("%s %lx\n", "read", addr);           // Print and test if file is read correctly
        else if (rw == 'w')
            printf("%s %lx\n", "write", addr);          // Print and test if file is read correctly
            */
        if (rw == 'r')
           L1.read(addr);

           else if(rw == 'w')
           L1.write(addr);
         //if(++count  ==136)
           // break;
    }

    L1.contents_report();
   L1.statistics_report();

    return EXIT_SUCCESS;
}
