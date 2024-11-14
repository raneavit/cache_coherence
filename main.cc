/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
using namespace std;

#include "cache.h"

int main(int argc, char *argv[])
{
    
    ifstream fin;
    FILE * pFile;
    int busSignal;

    if(argv[1] == NULL){
         printf("input format: ");
         printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
         exit(0);
        }

    ulong cache_size     = atoi(argv[1]);
    ulong cache_assoc    = atoi(argv[2]);
    ulong blk_size       = atoi(argv[3]);
    ulong num_processors = atoi(argv[4]);
    ulong protocol       = atoi(argv[5]); /* 0:MODIFIED_MSI 1:DRAGON*/
    char *fname        = (char *) malloc(20);
    fname              = argv[6];
    string protocolName = protocol ?  "Dragon" : "Modified MSI" ;

    cout << "===== 506 Personal information =====" << endl;
    cout << "Avit Ramakant Rane" << endl;
    cout << "arrane" << endl;
    cout << "ECE406 Students? NO" << endl;

    printf("===== 506 SMP Simulator configuration =====\n");
    // print out simulator configuration here
    cout << "L1_SIZE:	           "<< cache_size << endl;
    cout << "L1_ASSOC:	           "<< cache_assoc << endl;
    cout << "L1_BLOCKSIZE:         "<< blk_size << endl;
    cout << "NUMBER OF PROCESSORS: "<< num_processors << endl;
    cout << "COHERENCE PROTOCOL:   "<< protocolName << endl;
    cout << "TRACE FILE:           "<< fname << endl;

    
    // Using pointers so that we can use inheritance */
    Cache** cacheArray = (Cache **) malloc(num_processors * sizeof(Cache));
    for(ulong i = 0; i < num_processors; i++) {
            cacheArray[i] = new Cache(cache_size, cache_assoc, blk_size, protocol, i);
    }

    pFile = fopen (fname,"r");
    if(pFile == 0)
    {   
        printf("Trace file problem\n");
        exit(0);
    }
    
    ulong proc;
    char op;
    ulong addr;

    int line = 1;
    while(fscanf(pFile, "%lu %c %lx", &proc, &op, &addr) != EOF)
    {
// #ifdef _DEBUG
//         printf("%d\n", line);
// #endif
        // propagate request down through memory hierarchy
        // by calling cachesArray[processor#]->Access(...)

        switch(protocol){
            case MSI:
                busSignal = cacheArray[proc]->Access(addr, op);
                for(ulong i=0; i<num_processors; i++){
                    if(i == proc) continue;
                    else{
                        cacheArray[i]->Snoop(addr, busSignal);
                    }
                }
            break;

            case DRGN:
                cacheLine * line;
                //first find if in other cores
                cacheArray[proc]->Cflag = true;

                for(ulong i=0; i<num_processors; i++){
                    if(i == proc) continue;
                    else{
                        line = cacheArray[i]->findLine(addr);
                        if(line != NULL){cacheArray[proc]->Cflag = false; break;}
                    }
                }
                

                busSignal = cacheArray[proc]->Access(addr, op);

                if(busSignal == BusRdandUpd){
                    for(ulong i=0; i<num_processors; i++){
                        if(i == proc) continue;
                        else{
                            cacheArray[i]->Snoop(addr, BusRd);
                        }
                    }

                    for(ulong i=0; i<num_processors; i++){
                        if(i == proc) continue;
                        else{
                            cacheArray[i]->Snoop(addr, BusUpd);
                        }
                    }
                }
                else{
                    for(ulong i=0; i<num_processors; i++){
                        if(i == proc) continue;
                        else{
                            cacheArray[i]->Snoop(addr, busSignal);
                        }
                    }
                }
                

            break;

            default:
                cout << "Unknown Protocol" << endl;
            break;

        }

        line++;
    }

    fclose(pFile);

    //********************************//
    //print out all caches' statistics //
    //********************************//
    for(ulong i=0; i<num_processors; i++){
        cacheArray[i]->printStats();
    }

    
}
