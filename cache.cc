/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s,int a,int b, int protocol, int id)
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));

   myProtocol = protocol;   
   myId = id;
  
   //*******************//
   //initialize your counters here//
   //*******************//

   writeBackswithFlush = 0;
   memTraffic = 0;
   noOfInvalidations = 0;
   noOfInterventions = 0;
   noOfFlushes = 0;
   BusRdXTransactions = 0;
   BusUpdTransactions = 0;

 
   tagMask =0;
   for(i=0;i<log2Sets;i++)
   {
      tagMask <<= 1;
      tagMask |= 1;
   }
   
   /**create a two dimentional cache, sized as cache[sets][assoc]**/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
         cache[i][j].invalidate();
      }
   }      
   
}

/**you might add other parameters to Access()
since this function is an entry point 
to the memory hierarchy (i.e. caches)**/
int Cache::Access(ulong addr,uchar op)
{
   currentCycle++;/*per cache global counter to maintain LRU order 
                    among cache ways, updated on every cache access*/
         
   if(op == 'w') writes++;
   else          reads++;
   
   cacheLine * line = findLine(addr);
   if(line == NULL)/*miss*/
   {
      if(op == 'w') writeMisses++;
      else readMisses++;

      memTraffic++;

      cacheLine *newline = fillLine(addr);
      if(op == 'w'){
         switch(myProtocol){
            case MSI:
               newline->setFlags(MSI_M);
               //Going from I to M
               //send invalidation bus signal - busrdx
               BusRdXTransactions++;
               return BusRdX;
               break;

            case DRGN:
               if(Cflag == true){
                  newline->setFlags(DRGN_M);
                  //send BusRd signal
                  return BusRd;
               } 
               else{
                  newline->setFlags(DRGN_Sm);
                  //send BusUpd signal
                  BusUpdTransactions++;
                  return BusRdandUpd;
               }
               break;

            default:
               newline->setFlags(DIRTY);
               break;

         }
         
      }

      else if(op == 'r'){
         switch(myProtocol){
            case MSI:
               newline->setFlags(MSI_C);
               //Going From I to C
               //send BusRd signal
               return BusRd;
               break;

            case DRGN:
               if(Cflag == true){
                  newline->setFlags(DRGN_E);
                  //send BusRd signal
                  return BusRd;
               } 
               else{
                  newline->setFlags(DRGN_Sc);
                  //send BusRd signal
                  return BusRd;
               }
               break;

            default:
               break;
         }
      }       
      
   }
   else
   {
      /**since it's a hit, update LRU and update dirty flag**/
      updateLRU(line);
      if(op == 'w'){
         switch(myProtocol){
            case MSI:
               //Going from C/M to M
               line->setFlags(MSI_M);
               break;

            case DRGN:
               if(Cflag == true){
                  //send BusUpd signal
                  if(line->getFlags() == DRGN_Sc || line->getFlags() == DRGN_Sm) {line->setFlags(DRGN_M); BusUpdTransactions++; return BusUpd;}
                  else {line->setFlags(DRGN_M); return 99;}
               } 
               else{
                  //send BusUpd signal
                  {line->setFlags(DRGN_Sm);BusUpdTransactions++; return BusUpd;}
               }
               break;
      
            default:
               line->setFlags(DIRTY);
               break;
         }
      }
   }

   return 99;
}

int Cache::Snoop(ulong addr, int signal){

   cacheLine * line = findLine(addr);

   if(line != NULL){
      switch (myProtocol)
      {
      case MSI:
         
         if(signal == BusRd || signal == BusRdX){
            switch(line->getFlags()){
               case MSI_M:
                  line->invalidate();
                  noOfInvalidations++;
                  noOfFlushes++;
                  memTraffic++;
                  break;

               case MSI_C:
                  line->invalidate();
                  noOfInvalidations++;
                  break;
            }
         }
         else if(signal == 99) ; // No bus signal
         else cout << "Invalid MSI signal" << endl;
         break;
      
      case DRGN:

         switch(signal){
            case BusRd:
               switch(line->getFlags()){
                  case DRGN_E:
                     line->setFlags(DRGN_Sc);
                     noOfInterventions++;
                     break;
                  case DRGN_Sc:
                     break;
                  case DRGN_M:
                     line->setFlags(DRGN_Sm);
                     noOfFlushes++;
                     memTraffic++;
                     noOfInterventions++;
                     break;
                  case DRGN_Sm:
                     noOfFlushes++;
                     memTraffic++;
                     break;
                  default:
                     break;
               }
               break;
            
            case BusUpd:
               switch(line->getFlags()){
                  case DRGN_Sm:
                     line->setFlags(DRGN_Sc);
                     break;
         
                  default:
                     break;
               }
               break;
            

         }
      
      default:
         break;
      }
   }
   

}

/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
   if(cache[i][j].isValid()) {
      if(cache[i][j].getTag() == tag)
      {
         pos = j; 
         break; 
      }
   }
   if(pos == assoc) {
      return NULL;
   }
   else {
      return &(cache[i][pos]); 
   }
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) { 
         return &(cache[i][j]); 
      }   
   }

   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].getSeq() <= min) { 
         victim = j; 
         min = cache[i][j].getSeq();}
   } 

   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   
   if(victim->getFlags() == DIRTY || victim->getFlags() == MSI_M || victim->getFlags() == DRGN_M  || victim->getFlags() == DRGN_Sm) {
      writeBack(addr);
   }
      
   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(VALID);    
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

void Cache::printStats()
{
   switch(myProtocol){
      case MSI:
         cout << "===== Simulation results (Cache " << myId << ") =====\n";
         /****print out the rest of statistics here.****/
         /****follow the ouput file format**************/

         cout << "01. number of reads:                            " << reads << endl;
         cout << "02. number of read misses:                      " << readMisses << endl;
         cout << "03. number of writes:                           " << writes << endl;
         cout << "04. number of write misses:                     " << writeMisses<< endl;
         cout << "05. total miss rate:                            " << fixed << setprecision(2) << 100*(float(readMisses+writeMisses)/(reads+writes)) << "%" << endl;
         writeBackswithFlush = writeBacks + noOfFlushes;
         cout << "06. number of writebacks:                       " << writeBackswithFlush<< endl;
         cout << "07. number of memory transactions:              " << memTraffic<< endl;
         cout << "08. number of invalidations:                    " << noOfInvalidations<< endl;
         cout << "09. number of flushes:                          " << noOfFlushes<< endl;
         cout << "10. number of BusRdX:                           " << BusRdXTransactions<< endl;
      break;

      case DRGN:
         cout << "===== Simulation results (Cache " << myId << ") =====\n";
         /****print out the rest of statistics here.****/
         /****follow the ouput file format**************/

         cout << "01. number of reads:                            " << reads << endl;
         cout << "02. number of read misses:                      " << readMisses << endl;
         cout << "03. number of writes:                           " << writes << endl;
         cout << "04. number of write misses:                     " << writeMisses<< endl;
         cout << "05. total miss rate:                            " << fixed << setprecision(2) << 100*(float(readMisses+writeMisses)/(reads+writes)) << "%" << endl;
         writeBackswithFlush = writeBacks + noOfFlushes;
         cout << "06. number of writebacks:                       " << writeBackswithFlush<< endl;
         cout << "07. number of memory transactions:              " << memTraffic<< endl;
         cout << "08. number of interventions:                    " << noOfInterventions<< endl;
         cout << "09. number of flushes:                          " << noOfFlushes<< endl;
         cout << "10. number of Bus Transactions(BusUpd):         " << BusUpdTransactions<< endl;
      break;

   } 


}
