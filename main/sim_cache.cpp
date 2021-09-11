#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <cmath>
#include <fstream>

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
   std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
    params.block_size       = strtoul(argv[1], nullptr, 10);
    params.l1_size          = strtoul(argv[2], nullptr, 10);
    params.l1_assoc         = strtoul(argv[3], nullptr, 10);
    params.vc_num_blocks    = strtoul(argv[4], nullptr, 10);
    params.l2_size          = strtoul(argv[5], nullptr, 10);
    params.l2_assoc         = strtoul(argv[6], nullptr, 10);
    trace_file              = argv[7];

    //TODO REMOVE FOR HERE FOR SUBMIT
   std::ofstream fp;
   fp.open("../logs.csv", std::ofstream::app);

  params.l2_size = 0;
   params.l2_assoc = 0;
    for(size_t i=pow(2,10);i < pow(2,16);i*=2){
      for(size_t j = 0; j < 4; j++) {
         params.l1_size = i;
         params.l1_assoc = 4;
         //j == 16 ? params.l1_assoc = params.l1_size / params.block_size : params.l1_assoc = j;
         params.block_size = pow(2,j)*16;
         // Open trace_file in read mode
         FP = fopen(trace_file, "r");
         if (FP == nullptr) {
            // Throw error and exit if fopen() failed
            printf("Error: Unable to open file %s\n", trace_file);
            exit(EXIT_FAILURE);
         }
         Cache L1(params, 0x01);
         // Print params
         std::string str_tmp = "";
         std::string pblk = "===== Simulator configuration =====\n  BLOCKSIZE:    ";
/*
         str_tmp = std::to_string(params.block_size);
         L1.cat_padded(&pblk, &str_tmp);
         pblk += "  L1_SIZE:      ";
         str_tmp = std::to_string(params.l1_size);
         L1.cat_padded(&pblk, &str_tmp);
         pblk += "  L1_ASSOC:     ";
         str_tmp = std::to_string(params.l1_assoc);
         L1.cat_padded(&pblk, &str_tmp);
         pblk += "  VC_NUM_BLOCKS:";
         str_tmp = std::to_string(params.vc_num_blocks);
         L1.cat_padded(&pblk, &str_tmp);
         pblk += "  L2_SIZE:      ";
         str_tmp = std::to_string(params.l2_size);
         L1.cat_padded(&pblk, &str_tmp);
         pblk += "  L2_ASSOC:     ";
         str_tmp = std::to_string(params.l2_assoc);
         L1.cat_padded(&pblk, &str_tmp);
         pblk += "  trace_file:   ";
         str_tmp = trace_file;
         L1.cat_padded(&pblk, &str_tmp);
         pblk += "\n";

         std::cout << pblk;
*/
         char str[2];
         while (fscanf(FP, "%s %lx", str, &addr) != EOF) {
            rw = str[0];
            if (rw == 'r')
               L1.read(addr);
            else if (rw == 'w')
               L1.write(addr);
         }
         std::cout<< i << " " << params.block_size << std::endl;

        // L1.contents_report();

         //TODO: REmove CSV Imp
         std::string csv = "" + std::to_string(params.block_size) + ", " +
                           std::to_string(params.l1_size) + ", " +
                           std::to_string(params.l1_assoc) + ", " +
                           std::to_string(params.vc_num_blocks) + ", " +
                           std::to_string(params.l2_size) + ", " +
                           std::to_string(params.l2_assoc) + ", ";


         L1.statistics_report(&csv);

         fp << csv << std::endl;

         fclose(FP);
         //delete (&L1);
         //delete (&fp);
         //delete (&csv);
      }
      }
   fp.close();
   std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
    std::cout << "Sim Time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << "s\n";
    return EXIT_SUCCESS;
}
