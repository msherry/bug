//Genes for breedingAge/Energy
//Use a separate GC for each colour for a speed gain (but test it)

//Test data________________________________________________________
//generations <= 500000
//srand(44)
//no upper gene restriction
//binary data.out md5sum: 77851a70cea783b2e05233e09c60f3e4

//Includes__________________________________________________________
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#if XCOMMON
#include <X11/Xlib.h>
#include <math.h>
#endif //XCOMMON
#if __POWERPC__
#include <profiler.h>
#endif


//Konstants_________________________________________________________
int kHorizTiles = 590;
int kVertTiles = 400;
int kFoodValue = 40;
int kFoodParticles;
int kBreedingEnergy = 250;
int kBreedingAge = 100;
int kMaxEnergy = 400;
int kDeathLevel = -10;
int kMaxInitGene = 6;         //actually one more than initMax
int kMutateChildren = 1;
int kEden = 0;

int kHScale;
int kVScale;

#define __MEMMANAGE__ 0
#define BINARY_OUT 1

#define kUpdateDelay 10000

#if RAND_MAX == 32767
   #define kRandBitshift 15
#elif RAND_MAX == 65535
   #define kRandBitshift 16
#else
   #undef kRandBitShift
#endif

int kRandomBugs;

//Macros____________________________________________________________
#define Restrict(x) if(x<0) x=0;   \
  //                  else if(x>100) x=100

#ifdef kRandBitshift
   #define rInt(x) (rand()*x)>>kRandBitshift
   monkey
#else 
   #define rInt(x) (random()%x)
#endif

#ifndef __POWERPC__
   #define nil 0
#endif

//When tileState is an array, we need *tileState,
//but now that it's a pointer, this will do
#define tileAddress(y,x) ((lookupStyle == array) ? \
(Tile *)(tileState+horizOffsets[y]+x) : (Tile *)HashLookup(x, y))

//Typedefs__________________________________________________________

//The size of one tile, which directly affects how much memory is
//used. It seems that using a char is also almost twice as fast as a
//long, for some reason.
typedef char Tile;

typedef struct GeneRecord
{
   long forward;
   long lright, mright, hright;
   long backward;
   long hleft, mleft, lleft;
   long total;
   long cannibalism;
} GeneRecord, *GeneRecordPtr;

typedef struct FloatyGeneRecord
{
   float forward;
   float lright, mright, hright;
   float backward;
   float hleft, mleft, lleft;
   float cannibalism;
} FloatyGeneRecord, *FloatyGeneRecordPtr;

typedef struct BugRecord
{
   unsigned long totalAge, tempAge;
   long color, energy;
   unsigned long direction;
   unsigned long x,y;
   unsigned long oldX,oldY;
   struct BugRecord *next, *previous;
   GeneRecord genes;
} BugRecord, *BugRecordPtr;

typedef struct Direction
{
   long h;
   long v;
} Direction;

typedef struct HashEntry {
  int x;
  int y;
  Tile tileData;
  struct HashEntry *next;
} HashEntry, *HashEntryPtr;

enum {food=-1, empty=0, bug};
enum {array, hash};

//Prototypes________________________________________________________
void Initialize(void);
void OpenDataFile(void);
void Life(void);
void AddFood(void);
void Birth(unsigned long, unsigned long, long, GeneRecordPtr, long);
void Death(BugRecordPtr);
BugRecordPtr NewObject(void);
void DisposeObject(BugRecordPtr);
void MoveBug(BugRecordPtr);
void TotalGenes(BugRecordPtr);
inline long CanBreed(BugRecordPtr);
void Reproduction(BugRecordPtr);
void Creation(long);
void Evolution(void);
void Mutation(BugRecordPtr, unsigned long, long);
void RandomGenes(GeneRecordPtr);
void PrintGenes(FloatyGeneRecordPtr, long, long);
void AverageGenes(FloatyGeneRecordPtr);
void GardenOfEden(void);
void GraphData(FloatyGeneRecordPtr);

void ParseOptions(int, char**);
void ShowHelp(void);
Tile *HashLookup(int, int);
HashEntryPtr SearchList(unsigned int, int, int);
HashEntryPtr InsertIntoHash(unsigned int, int, int);
void RemoveHashEntry(int, int);
void FreeMemory(void);

#if XCOMMON
void InitializeGraphics(void);
void SetupColors(void);
#endif

//Globals__________________________________________________________
unsigned long numBugs=0;
unsigned long generations=0;
Tile *tileState;
BugRecordPtr gBugList;
Direction directions[8] = {{0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, \
   {-1,1}, {-1,0}, {-1,-1}};
   
int *horizOffsets;
int lookupStyle=array;

long recurMutate=0;

#if __MEMMANAGE__
BugRecordPtr gFreeBugRecs = nil;
#endif

#if BINARY_OUT
int dataFile;
#else
FILE *dataFile;
#endif //BINARY_OUT
FILE *outFile;
   
#if XCOMMON
Display *display;
Window win;
GC gc;

int greenVal;
int colorVals[256];
#if XFAST
XPoint *points;
XPoint newPoints[256][10000];
#define kFastDrawThresh 1200
#endif //XFAST
#endif //XCOMMON

#define kHashTableSize 8192
//This algorithm is heaps better than the original - speeds are much
//higher. 

//This value is .5 * (sqrt(3)-1) * 2^32
#define HASH_S (unsigned long)1572067122
//Extract the log2(kHashTableSize) bits of the following function
#define HASH(x,y) ((((x<<4) + y) * HASH_S) >> 19)
HashEntryPtr hashArray[kHashTableSize];
HashEntryPtr freeHashes=nil;

char displayName[255] = "\0";
int testMode = 0;
int testGenerations = 500000;

//Code_____________________________________________________________
int main(int argc, char *argv[])
{
   long exitFlag=0;
   unsigned long begin, end;

   ParseOptions(argc, argv);
   Initialize();
   OpenDataFile();

   outFile = fopen("./bug.out","w");

   Creation(kRandomBugs);
   
   time(&begin);
   while(!exitFlag)
   {
      Life();
      if(random()>(const int)(.95779*RAND_MAX)/*31384*/)   //(30000)
         AddFood();
      if (kEden)
         if(random()>(const int)(.95779*RAND_MAX)/*31384*/)
            GardenOfEden();
      if(numBugs == 0)
         Creation(kRandomBugs);

      if(!(generations%500))
      {
	  FloatyGeneRecord genesAvg = {0,0,0,0,0,0,0,0,0};
	  
          Evolution();
	  AverageGenes(&genesAvg);
          GraphData(&genesAvg);
          if(!(generations%kUpdateDelay))
          {
             time(&end);
	     PrintGenes(&genesAvg, begin, end);
          }

	  if(lookupStyle == hash)
	    FreeMemory();
      }
      generations++;	//6/20/01 - Moved to bottom of loop
      if(testMode && generations > testGenerations)
	exitFlag = 1;
   }
#if XCOMMON
   XCloseDisplay(display);
#endif //XCOMMON
   return 0;
}

void FreeMemory(void)
{
  HashEntryPtr q;

  q=freeHashes;
  while(q)
  {
    freeHashes=freeHashes->next;
    free(q);
    q=freeHashes;
  }
}

Tile *HashLookup(int x, int y)
{
  unsigned long hashValue = HASH(x,y);
  HashEntryPtr p;

  p = SearchList(hashValue, x, y);
  if(!p)
    p = InsertIntoHash(hashValue, x, y);

  return &(p->tileData);
}

HashEntryPtr SearchList(unsigned int hashValue, int x, int y)
{
  HashEntryPtr p;

  p = hashArray[hashValue];
  if(p == nil)
    return nil;

  do  //since we've already checked for nil, minor optimization
  {
    if (p->x == x && p->y == y)
       return p;
    p = p->next;
  } while(p != nil); 
  return nil;
}

HashEntryPtr InsertIntoHash(unsigned int hashValue, int x, int y)
{
  HashEntryPtr p;

  p=freeHashes;
  if(!p)
  {
    p = malloc(sizeof(HashEntry));
    if(p == nil)
    {
      printf("Couldn't allocate memory for HashEntry\n");
      exit(-1);
    }
  }
  else
  {
    freeHashes = freeHashes->next;
  }

  p->x = x;
  p->y = y;
  p->tileData = empty;

  p->next = hashArray[hashValue];
  hashArray[hashValue] = p;

  return p;
}

void RemoveHashEntry(int x, int y)
{
   HashEntryPtr p,q=nil;
   unsigned int hashValue = HASH(x,y);
   
   p = hashArray[hashValue];
   while(p)
     {
       if(p->x == x && p->y == y)
	 break;
       q=p;
       p=p->next;
     }
   if (p == nil)
     return;
   if(q)
     q->next = p->next;
   if (p == hashArray[hashValue])
     hashArray[hashValue] = p->next;
   
   //Add this entry to the free hashes list, for later use or eventual
   //garbage collection
   p->next = freeHashes;
   freeHashes = p;
}

void ParseOptions(int argc, char *argv[])
{
   int c, hScaleSet = 0, vScaleSet = 0;
   int numTiles;
   float tempFoodParticles;

   static struct option long_options[] = {
     {"eden", 0, &kEden, 1},
     {"hscale", 1, 0, 0xFF},
     {"vscale", 1, 0, 0xFE},
     {"genemax", 1, 0, 0xFD},
     {"lookupstyle", 1, 0, 0xFC},
     {"display", 1, 0, 0xFB},
     {"mutate", 1, 0, 0xFA},
     {"test", 0, &testMode, 1},

     {"help", 0, 0, 'h'},
   };

   while((c = getopt_long(argc, argv, "x:y:v:e:a:m:d:h", long_options, 0))!=-1)
   {
      switch (c)
      {
         case 'x':
	   kHorizTiles = atoi(optarg);
	   break;
         case 'y':
	   kVertTiles = atoi(optarg);
	   break;
         case 'v':
	   kFoodValue = atoi(optarg);
	   break;
         case 'e':
	   kBreedingEnergy = atoi(optarg);
	   break;
         case 'a':
	   kBreedingAge = atoi(optarg);
	   break;
         case 'm':
	   kMaxEnergy = atoi(optarg);
	   break;
         case 'd':
	   kDeathLevel = atoi(optarg);
	   break;
         case 0xFF:
	   kHScale = atoi(optarg);
	   hScaleSet=1;
	   break;
         case 0xFE:
	   kVScale = atoi(optarg);
	   vScaleSet=1;
	   break;
         case 0xFD:
	   kMaxInitGene = atoi(optarg);
	   break;
         case 0xFC:
	   {
	     if(!strcmp(optarg, "array"))
	       lookupStyle = array;
	     else if(!strcmp(optarg, "hash"))
	       lookupStyle = hash;
	     else
	       {
		 printf("Invalid lookup style specified\n");
		 exit(-1);
	       }
	     break;
	   }
         case 0xFB:
	   {
	     strncpy(displayName, optarg, 255);
	     break;
	   }
         case 0xFA:
	   {
	     kMutateChildren = atoi(optarg);
	     break;
	   }
         case '?':
         case 'h':
	   ShowHelp();
	   exit(0);
	   break;
      }
   }
   
   if(kHorizTiles < 0 || kVertTiles < 0) 
   {
     printf("Must have a positive number of tiles\n");
     exit(-1);
   }
#if XCOMMON
   //Set H-andVScale to the number of multiples of 700
   if (kHorizTiles >= 700 && !hScaleSet)
     kHScale = rint(log((float)kHorizTiles / 700)/log(2));
   if(kVertTiles >= 700 && !vScaleSet)
     kVScale = rint(log((float)kVertTiles / 700)/log(2));
#endif //XCOMMON

   numTiles = kHorizTiles * kVertTiles;
   if(numTiles > 78667)
     kRandomBugs = 5;
   else
     kRandomBugs = 1;
   
   //Used to cast kFoodParticles to a float, but now gcc
   //complains. This prevents ugly rounding errors (like getting 119
   //instead of 120)
   tempFoodParticles = (.6*numTiles)/1180;
   kFoodParticles=tempFoodParticles;
}

void ShowHelp(void)
{
   printf("usage: bug [options]\n");
   printf("  -x\t\tNumber of horizontal tiles\t590\n");
   printf("  -y\t\tNumber of vertical tiles\t400\n");
   printf("  -v\t\tFood value\t\t\t40\n");
   printf("  -e\t\tBreeding energy\t\t\t250\n");
   printf("  -a\t\tBreeding age\t\t\t100\n");
   printf("  -m\t\tMax energy\t\t\t400\n");
   printf("  -d\t\tDeath level\t\t\t-10\n");
   printf("  --eden\tGarden of Eden\n");
   printf("  --mutate\tWhether to mutate children\n");
   printf("  --genemax\tMaximum initial gene value\n");
   printf("  --hscale\tHorizontal scale factor (X only)\n");
   printf("  --vscale\tVertical scale factor (X only)\n");
   printf("  --lookupstyle\tArray or hash table lookup\n");
   printf("  --display\tName of the X server to connect to\n");
   printf("  --test\tTest mode - 500000 generations, srandom(44)\n");   
   printf("\n");
   printf("  -h\t\tThis help\n");
}

void GraphData(FloatyGeneRecordPtr genes)
{
   static unsigned long i=0;

   i++;

#if BINARY_OUT
   //Binary data is stored in NATIVE byte order, so won't work
   //across platforms
   char newLine=0xA;

   write(dataFile, &genes->forward, sizeof(float));
   write(dataFile, &genes->lright, sizeof(float));
   write(dataFile, &genes->mright, sizeof(float));
   write(dataFile, &genes->hright, sizeof(float));
   write(dataFile, &genes->backward, sizeof(float));
   write(dataFile, &genes->hleft, sizeof(float));
   write(dataFile, &genes->mleft, sizeof(float));
   write(dataFile, &genes->lleft, sizeof(float));
   write(dataFile, &genes->cannibalism, sizeof(float));
   
   write(dataFile, &i, sizeof(unsigned long));
   write(dataFile, &newLine, sizeof(char));
#else
   //NEWGENES 
   monkey
   fprintf(dataFile, "%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%li\n", 
	   genes->forward, genes->lright, genes->mright, genes->hright,
	   genes->backward, genes->hleft, genes->mleft, genes->lleft,
	   genes->cannibalism, i);
#endif //BINARY_OUT
}

void AverageGenes(FloatyGeneRecordPtr genes)
{
   FloatyGeneRecord tgenes = {0,0,0,0,0,0,0,0,0};
   BugRecordPtr theBug = gBugList;
   float numBugsRecip = (float)1/numBugs;

   while(theBug != nil)
   {
      tgenes.forward += theBug->genes.forward;
      tgenes.lright += theBug->genes.lright;
      tgenes.mright += theBug->genes.mright;
      tgenes.hright += theBug->genes.hright;
      tgenes.backward += theBug->genes.backward;
      tgenes.hleft += theBug->genes.hleft;
      tgenes.mleft += theBug->genes.mleft;
      tgenes.lleft += theBug->genes.lleft;
      tgenes.cannibalism += theBug->genes.cannibalism;
      //tgenes.cannibalism += theBug->totalAge;
      theBug = theBug->next;
   }
   tgenes.forward *= numBugsRecip;
   tgenes.lright *= numBugsRecip;
   tgenes.mright *= numBugsRecip;
   tgenes.hright *= numBugsRecip;
   tgenes.backward *= numBugsRecip;
   tgenes.hleft *= numBugsRecip;
   tgenes.mleft *= numBugsRecip;
   tgenes.lleft *= numBugsRecip;
   tgenes.cannibalism *= numBugsRecip;

   //We get weird rounding errors without this
   memcpy(genes, &tgenes, sizeof(FloatyGeneRecord));
}

void PrintGenes (FloatyGeneRecordPtr genes, long begin, long end)
{
   double minutes, hours;
   minutes = difftime(end, begin)/60;
   hours = minutes/60;

   fprintf(outFile, "Forward:  %f\n", genes->forward);
   fprintf(outFile, "L Right:  %f\n", genes->lright);
   fprintf(outFile, "M Right:  %f\n", genes->mright);
   fprintf(outFile, "H Right:  %f\n", genes->hright);
   fprintf(outFile, "Backward: %f\n", genes->backward);
   fprintf(outFile, "H Left:   %f\n", genes->hleft);
   fprintf(outFile, "M Left:   %f\n", genes->mleft);
   fprintf(outFile, "L Left:   %f\n", genes->lleft);
   fprintf(outFile, "Cannibalism: %f\n\n", genes->cannibalism);
   
   fprintf(outFile, "RecursiveMutations: %li\n", recurMutate);
   fprintf(outFile, "Generations: %lu\nBugs: %lu\n\n", generations, numBugs);
   
   fprintf(outFile, "Minutes: %.2f\n", minutes);
   fprintf(outFile, "Hours: %.2f\n", hours);
   if(hours >= 24)
       fprintf(outFile, "Days: %.2f\n\n", hours/24);
   else
       fprintf(outFile, "\n");

   fprintf(outFile, "Generations/min: \t%.2f\n", generations/minutes);
   fprintf(outFile, "Generations/hour: \t%.2f\n\n", generations/hours);
   
   fprintf(outFile, "Start Time: %s", ctime(&begin));
   fprintf(outFile, "End Time:   %s", ctime(&end));

   //Do this at the end for its synchronizing side effect
   rewind(outFile);
}

void RandomGenes(GeneRecord *g)
{
   g->forward = random()%kMaxInitGene;
   g->lright = random()%kMaxInitGene;
   g->mright = random()%kMaxInitGene;
   g->hright = random()%kMaxInitGene;
   g->backward = random()%kMaxInitGene;
   g->hleft = random()%kMaxInitGene;
   g->mleft = random()%kMaxInitGene;
   g->lleft = random()%kMaxInitGene;
   
   g->cannibalism = random()%kMaxInitGene;
}

void AddFood(void)
{
   long i;

#if XCOMMON
   XSetForeground(display, gc, greenVal);
#if XFAST
   long j=0;
#endif //XFAST
#endif //XCOMMON

   for(i=0; i<kFoodParticles; i++)
   {
      long x,y;
      
      x = rInt(kHorizTiles);
      y = rInt(kVertTiles);

      if(*tileAddress(y,x) == empty)
      {
#if X
	XDrawPoint(display, win, gc, x>>kHScale, y>>kVScale);
#elif XFAST
	points[j].x=x>>kHScale;
	points[j].y=y>>kVScale;
	j++;
#endif //X
         *tileAddress(y,x) = food;
      }
   }
#if XFAST
   XDrawPoints(display, win, gc, points, j, CoordModeOrigin);
#endif //XFAST
}

void GardenOfEden(void)
{
   long i;

#if XCOMMON
   XSetForeground(display, gc, greenVal);
#endif //XCOMMON
   for(i=0; i<kFoodParticles/8; i++)
   {
      long x,y;
      
      x = (long)rInt(kHorizTiles>>2);
      y = (long)rInt(kVertTiles>>2);

      if(*tileAddress(y,x) == empty)
      {
#if XCOMMON
         XDrawPoint(display, win, gc, x>>kHScale, y>>kVScale);
#endif //XCOMMON
         *tileAddress(y,x) = food;
      }
   }
}

void Life(void)
{
   BugRecordPtr theBug;
   long h,v;
   unsigned long d;
   unsigned long hypX, hypY, oldX, oldY;
#if XFAST
   long tempNumBugs = numBugs;  //in case numBugs drops below threshhold
   long i=0;
   static unsigned long drawables[256];
   
   if(generations == 0)
     {
       for(i=0; i<256; i++)
	 drawables[i] = 0;

       i=0;
     }
#endif //XFAST

   theBug = gBugList;
   while(theBug != nil)
   {
      theBug->totalAge++;
      theBug->tempAge++;

      if(theBug->energy-- > 0)
      {
         Tile *tile;
         Tile tileContents;

         MoveBug(theBug);

         d = theBug->direction;
         h = directions[d].h;
         v = directions[d].v;

         oldX = theBug->x;
         oldY = theBug->y;
         theBug->oldX = oldX;
         theBug->oldY = oldY;

         hypX = theBug->x + h;
         hypY = theBug->y + v;

         if(hypX == -1L)
            hypX = 0;
         else if(hypX == kHorizTiles)
            hypX = kHorizTiles-1;

         if(hypY == -1L)
            hypY = 0;
         else if(hypY == kVertTiles)
            hypY = kVertTiles-1;

         tile = tileAddress(hypY,hypX);
         tileContents = *tile;

         if(tileContents != bug)
         {
#if XFAST
	   if(tempNumBugs > kFastDrawThresh)
	   {
	     points[i].x=oldX>>kHScale;
	     points[i].y=oldY>>kVScale;
	     i++;
	   }
	   else
#endif //XFAST
#if XCOMMON
	   {
	     XSetForeground(display, gc, 0);
	     XDrawPoint(display, win, gc, oldX>>kHScale, oldY>>kVScale);
	   }
#endif //X
	    (lookupStyle == array) ? 
	      *tileAddress(oldY,oldX) = empty : \
	      RemoveHashEntry(oldX, oldY);
            theBug->x = hypX;
            theBug->y = hypY;
            if(tileContents == food)
            {
               if(theBug->energy < kMaxEnergy)
               {
                  theBug->energy += kFoodValue;
               }
            }
#if XFAST
	    //If using fast drawing and numBugs > threshhold for using
	    //it, add each bug to a list to be drawn later
	    if(tempNumBugs > kFastDrawThresh)
	    {
	    //This seems to only be faster for large(!) numbers of bugs
	    newPoints[theBug->color][drawables[theBug->color]].x=hypX>>kHScale;
	    newPoints[theBug->color][drawables[theBug->color]].y=hypY>>kVScale;
	    drawables[theBug->color]++;
	    }
	    else
#endif //XFAST
#if XCOMMON
	    {
	      //Draw each bug individually
	      
	      //Having a separate colorVal for each color not only makes
	      //more sense, but is faster as well. Nice.
	      XSetForeground(display, gc, colorVals[theBug->color]);
	      XDrawPoint(display, win, gc, hypX>>kHScale, hypY>>kVScale);
	    }
#endif //XCOMMON
            *tile = bug;
         }

         if(CanBreed(theBug))
            Reproduction(theBug);

         theBug = theBug->next;
      }   //theBug->energy-- > 0
      else if(theBug->energy > kDeathLevel)
      {
         theBug = theBug->next;
      }
      else
      {
         BugRecordPtr tempBug = theBug;
         theBug = theBug->next;
         Death(tempBug);
      }
   }//while theBug != nil
#if XFAST
   //Draw over the old positions
   XSetForeground(display, gc, 0);
   XDrawPoints(display, win, gc, points, i, CoordModeOrigin);

   if(tempNumBugs > kFastDrawThresh)
   {
   //Draw the new positions
   for(i=0; i<256; i++)
     {
       if(drawables[i])
	 {
	   XSetForeground(display, gc, colorVals[i]);
	   XDrawPoints(display, win, gc, newPoints[i], drawables[i], CoordModeOrigin);
	   drawables[i] = 0;
	 }
     }
   }
#endif //XFAST
}

inline long CanBreed(BugRecordPtr theBug)
{
   if(theBug->energy >= kBreedingEnergy)
   {
      if(theBug->tempAge >= kBreedingAge)
      {
         return 1L;
      }
   }
   return 0;
}

void Reproduction (BugRecordPtr theBug)
{
   long x, y, directionValue, energy;
   long h,v;
   Tile *tile;

   directionValue = random()&7;
   h = directions[directionValue].h;
   v = directions[directionValue].v;
   x = theBug->x + h;
   y = theBug->y + v;
   
   if(x < 0)
      x = 0;
   if(x >= kHorizTiles)
      x = kHorizTiles-1;
   if(y < 0)
      y = 0;
   if(y >= kVertTiles)
      y = kVertTiles-1;
   
   tile = tileAddress(y,x);
   
   if(*tile != bug)
   {
      energy = theBug->energy >> 1;
      Birth(x, y, energy, &theBug->genes, theBug->color);
      theBug->energy = energy;
      theBug->tempAge = 0;
      *tile = bug;
   }
}

void MoveBug(BugRecordPtr theBug)
{
   long i;
   long turnValue = rInt(theBug->genes.total);

   //forward
   turnValue -= theBug->genes.forward;
   if(turnValue < 0)
   {
      i=0;
      goto END;
   }
   //lright
   turnValue -= theBug->genes.lright;
   if(turnValue < 0)
   {
      i=1;
      goto END;
   }
   //mright
   turnValue -= theBug->genes.mright;
   if(turnValue < 0)
   {
      i=2;
      goto END;
   }
   //hright
   turnValue -= theBug->genes.hright;
   if(turnValue < 0)
   {
      i=3;
      goto END;
   }
   //backward
   turnValue -= theBug->genes.backward;
   if(turnValue < 0)
   {
      i=4;
      goto END;
   }
   //hleft
   turnValue -= theBug->genes.hleft;
   if(turnValue < 0)
   {
      i=5;
      goto END;
   }
   //mleft
   turnValue -= theBug->genes.mleft;
   if(turnValue < 0)
   {
      i=6;
      goto END;
   }
   //lleft
   {
      i=7;  //if we get here, we should assume we are moving lleft
   }
   
END:
   i += theBug->direction;
   if(i >= 8)
   {
      i -= 8;
   }
   theBug->direction = i;
}

void Birth(unsigned long x, unsigned long y, long energy, GeneRecordPtr genes, long color)
{
   BugRecordPtr newBug = NewObject();

   newBug->x = x;
   newBug->y = y;
   newBug->direction = random()&7;
   newBug->energy = energy;
   newBug->genes = *genes;
   newBug->color = color;
   if (kMutateChildren)
     Mutation(newBug, 1, 0);
   else
     TotalGenes(newBug);
}

void Death(BugRecordPtr theBug)
{
#if XCOMMON
   XSetForeground(display, gc, 0);
   XDrawPoint(display, win, gc, theBug->x>>kHScale, theBug->y>>kVScale);
#endif //XCOMMON
   (lookupStyle == array) ? 
     *tileAddress(theBug->y,theBug->x) = empty : \
     RemoveHashEntry(theBug->x, theBug->y);
   DisposeObject(theBug);
}

void TotalGenes(BugRecordPtr theBug)
{
   theBug->genes.total = theBug->genes.forward+theBug->genes.lright +
   theBug->genes.mright + theBug->genes.hright + theBug->genes.backward +
   theBug->genes.hleft + theBug->genes.mleft + theBug->genes.lleft;
   
   if(theBug->genes.total == 0)
   {
      Mutation(theBug, (random() & 7)+1, 1);
      recurMutate++;
   }
}

BugRecordPtr NewObject(void)
{
   BugRecordPtr   newObject;
   
#if __MEMMANAGE__ //memory management helps on my 603, but not on i386
   if(gFreeBugRecs)
   {
      newObject = gFreeBugRecs;
      gFreeBugRecs = gFreeBugRecs->next;
   }
   else
#endif
   {
      newObject = (BugRecordPtr)malloc(sizeof(BugRecord));
      if (newObject == nil) return nil;
   }
   
#if XCOMMON
   newObject->oldX=0;    //Since we no longer use NewPtrClear, these need
   newObject->oldY=0;    //to be cleared explicitly - everything else is
#endif //XCOMMON
   newObject->tempAge=0; //already set here (next, previous) or in Birth
   newObject->totalAge=0;//We only need to set oldX & oldY to draw
   
   if (gBugList != nil)
   {
      gBugList->previous = newObject;
   }
   newObject->next = gBugList;
   newObject->previous = nil;
   gBugList = newObject;
   numBugs++;
   return newObject;
}

void DisposeObject(BugRecordPtr doomedObject)
{
   if (doomedObject->next != nil)
      doomedObject->next->previous = doomedObject->previous;
   if (doomedObject->previous != nil)
      doomedObject->previous->next = doomedObject->next;
   if (doomedObject == gBugList)
      gBugList = doomedObject->next;
   
#if __MEMMANAGE__
   if(gFreeBugRecs != nil)
   {
      gFreeBugRecs->previous = doomedObject;
   }
   doomedObject->next = gFreeBugRecs;
   doomedObject->previous = nil;
   gFreeBugRecs = doomedObject;
#else
   free (doomedObject);
#endif
   numBugs--;
}

void OpenDataFile(void)
{
#if BINARY_OUT
   dataFile = open("./data.out", O_RDWR+O_CREAT+O_TRUNC, \
		   S_IREAD+S_IWRITE+S_IRGRP+S_IROTH);
   if(dataFile < 0)
   {
      perror("dataFile");
      exit(-1);
   }
   write (dataFile, "!", 1);  //Binary files are flagged by an initial "!"
#else
   dataFile = fopen("./data.out","w");
   if(!dataFile)
   {
      perror("dataFile");
      exit(-1);
   }
#endif //BINARY_OUT
}

void Initialize(void)
{
   unsigned long x,y;

   time(&x);
   if(testMode)
     srandom(44);
   else
     srandom(x);

   tileState = (Tile *)malloc(kHorizTiles*kVertTiles*sizeof(Tile));
   if(!tileState)
   {
     printf("Couldn't allocate memory for tileState - switching to hash table\n");
     lookupStyle=hash;
   }

   if(lookupStyle==hash)
   {
     for(y=0; y<kHashTableSize; y++)
       {
       hashArray[y]=nil;
       }
   }
   else  //lookupStyle == array
   {
     horizOffsets = (int *)malloc(kVertTiles * sizeof(int));
     for (y=0; y<kVertTiles; y++)
       {
	 horizOffsets[y]=y*kHorizTiles;
	 for (x=0; x<kHorizTiles; x++)
	   {
	     *tileAddress(y,x)=empty;
	   }
       }
   }

#if XCOMMON
   InitializeGraphics();
#endif
}

#if XCOMMON
void InitializeGraphics(void)
{
   XGCValues values;
   unsigned long valuemask = 0;
   int screen_num;
   int screen_width;
   int screen_height;
   Window root_window;

   display = XOpenDisplay(displayName);

   if(!display)
   {
      fprintf(stderr,"Could not connect to X display server %s\n",displayName);
      exit(-1);
   }

   screen_num = DefaultScreen(display);
   screen_width = DisplayWidth(display, screen_num);
   screen_height = DisplayHeight(display, screen_num);
   root_window = RootWindow(display, screen_num);
 
   win = XCreateSimpleWindow(display,
      RootWindow(display, screen_num),
      0, 0, kHorizTiles>>kHScale, kVertTiles>>kVScale,
      2, BlackPixel(display, screen_num),
      BlackPixel(display, screen_num));

   XMapWindow(display,win);
   XSync(display, False);

   gc = XCreateGC(display, win, valuemask, &values);
   if (gc < 0)
   {
      perror("XCreateGC");
      exit(-1);
   }

   XSetForeground(display, gc, BlackPixel(display, screen_num));
   XSetBackground(display, gc, WhitePixel(display, screen_num));
   XSetFillStyle(display, gc, FillSolid);
   XSetLineAttributes(display, gc, 1, LineSolid, CapRound, JoinRound);

   XDrawRectangle(display, win, gc, 0, 0, kHorizTiles>>kHScale, kVertTiles>>kVScale);
   XFillRectangle(display, win, gc, 0, 0, kHorizTiles>>kHScale, kVertTiles>>kVScale);
   XFlush(display);

   //Set up some predefined colors, so colors will appear the same on
   //different screens
   SetupColors();

#if XFAST
   //Set up an array of XPoints - doesn't need to be any larger than
   //the number of pixels we actually display
   points = (XPoint *)malloc((kHorizTiles >> kHScale) * 
			     (kVertTiles >> kVScale) * sizeof(XPoint));
#endif //XFAST
}

void SetupColors()
{
  int i, j, k, l=0;
  //Possible color values. Each color consists of doubled characters
  //only (eeff77, 556677, etc.)
  char vals[6] = {"579BDF"};
  char tempNam[8];
  Colormap colormap;
  XColor tempColor, green;
  char greenStr[] = "#00DD00";

  colormap = DefaultColormap(display, DefaultScreen(display));

  //Set up the food color
  XParseColor(display, colormap, greenStr, &green);
  XAllocColor(display, colormap, &green);
  greenVal = green.pixel;

  //Must null-terminate, otherwise it only works for true-color
  //displays (not 16-bit)
  tempNam[7] = '\0';

  //Simple kludge to map 216 distinct colors to 256 - just wrap around
  //again (so we don't have to perform a modulus operation every time
  //we draw a bug)
  for(i=0; i<7; i++)
    {
      for(j=0; j<6; j++)
	{
	  for(k=0; k<6; k++)
	    {
	      tempNam[0] = '#';
	      tempNam[1] = vals[i%6];
	      tempNam[2] = vals[i%6];
	      tempNam[3] = vals[j];
	      tempNam[4] = vals[j];
	      tempNam[5] = vals[k];
	      tempNam[6] = vals[k];
	      XParseColor(display, colormap, tempNam, &tempColor);
	      XAllocColor(display, colormap, &tempColor);
	      colorVals[l] = tempColor.pixel;
	      l++;
	      if(l>=256) break;
	    }
	}
    }
}
#endif //XCOMMON

void Creation(long numBugs)
{
   long i;
   GeneRecord randomGenes;
   
   for (i=0; i<numBugs; i++)
   {
      RandomGenes(&randomGenes);
      
      Birth(rInt(kHorizTiles), rInt(kVertTiles), kMaxEnergy, 
         &randomGenes, rInt(256));
   }
}

void Evolution(void)
{
   unsigned long i;
   unsigned long randomBug = rInt(numBugs);
   BugRecordPtr theBug = gBugList;
   
   if(theBug == nil)
      return;
   
   for (i=0; i<randomBug; i++)
   {
      theBug = theBug->next;
   }
   Mutation(theBug, (random()&7)+1, 1);
}

void Mutation(BugRecordPtr theBug, unsigned long numGenes, long changeColor)
{
   long didMutate = 0;

   do
   {
      long geneToMutate = random()&7;
      long mutationDelta = (random()-(RAND_MAX>>1)) % 4;
      
      if(mutationDelta)
	didMutate = 1;
      else           //minor speed optimization - skip the rest
	goto END;    //if mutataionDelta is zero
      
      switch (geneToMutate)
      {
         case 0:
         {
            theBug->genes.forward += mutationDelta;
            Restrict(theBug->genes.forward);
            break;
         }
         case 1:
         {
            theBug->genes.lright += mutationDelta;
            Restrict(theBug->genes.lright);
            break;
         }
         case 2:
         {
            theBug->genes.mright += mutationDelta;
            Restrict(theBug->genes.mright);
            break;
         }
         case 3:
         {
            theBug->genes.hright += mutationDelta;
            Restrict(theBug->genes.hright);
            break;
         }
         case 4:
         {
            theBug->genes.backward += mutationDelta;
            Restrict(theBug->genes.backward);
            break;
         }
         case 5:
         {
            theBug->genes.hleft += mutationDelta;
            Restrict(theBug->genes.hleft);
            break;
         }
         case 6:
         {
            theBug->genes.mleft += mutationDelta;
            Restrict(theBug->genes.mleft);
            break;
         }
         case 7:
         {
            theBug->genes.lleft += mutationDelta;
            Restrict(theBug->genes.lleft);
            break;
         }
         case 8:
         {
            theBug->genes.cannibalism += mutationDelta;
            if(theBug->genes.cannibalism < 0)
               theBug->genes.cannibalism = 0;
            break;
	    }
         default:
	 {
	    printf("Tried to mutate a non-existent gene - %li\n",geneToMutate);
	    exit(-1);
	    break;
	 }
      }
   END:
      numGenes--;
   }while(numGenes>0);
   
   TotalGenes(theBug);

   //We do this weird nested thing to preserve the original md5sums
   //from when kMutateChildren was a compile-time constant
   if(kMutateChildren)
   {
     if(changeColor)
       theBug->color = random()&255;
   }
   else if(didMutate)
   {
     theBug->color = random()&255;
   }
}
