/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include "includes.h"

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/
enum States {
   INVALID = 0,
   VALID,
   DIRTY,
   MSI_C,
   MSI_M,
   DRGN_M,
   DRGN_Sm,
   DRGN_Sc,
   DRGN_E
};

enum Protocols{ //protocols
   MSI,
   DRGN
};


enum Signals{ //Bus Signals
   BusRd,
   BusRdX,
   BusUpd,
   BusRdandUpd
};

class cacheLine 
{
protected:
   ulong tag;
   ulong Flags;   // 0:invalid, 1:valid, 2:dirty 
   ulong seq; 
 
public:
   cacheLine()                { tag = 0; Flags = 0; }
   ulong getTag()             { return tag; }
   ulong getFlags()           { return Flags;}
   ulong getSeq()             { return seq; }
   void setSeq(ulong Seq)     { seq = Seq;}
   void setFlags(ulong flags) {  Flags = flags;}
   void setTag(ulong a)       { tag = a; }
   void invalidate()          { tag = 0; Flags = INVALID; } //useful function
   bool isValid()             { return ((Flags) != INVALID); }
};

class Cache
{
protected:
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
   ulong reads,readMisses,writes,writeMisses,writeBacks;
   int myProtocol;
   int myId;

   //******///
   //add coherence counters here///
   //******///

   ulong writeBackswithFlush;
   ulong memTraffic;
   ulong noOfInvalidations;
   ulong noOfFlushes;
   ulong BusRdXTransactions;

   ulong noOfInterventions;
   ulong BusUpdTransactions;

   cacheLine **cache;
   ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
   ulong calcIndex(ulong addr)   { return ((addr >> log2Blk) & tagMask);}
   ulong calcAddr4Tag(ulong tag) { return (tag << (log2Blk));}
   
public:
    ulong currentCycle;
    bool Cflag;  
     
    Cache(int,int,int,int,int);
   ~Cache() { delete cache;}
   
   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine * findLine(ulong addr);
   cacheLine * getLRU(ulong);
   
   ulong getRM()     {return readMisses;} 
   ulong getWM()     {return writeMisses;} 
   ulong getReads()  {return reads;}       
   ulong getWrites() {return writes;}
   ulong getWB()     {return writeBacks;}
   
   void writeBack(ulong) {writeBacks++; memTraffic++;}
   int Access(ulong,uchar);
   int Snoop(ulong addr, int signal);
   void printStats();
   void updateLRU(cacheLine *);

   //******///
   //add other functions to handle bus transactions///
   //******///

};

#endif
