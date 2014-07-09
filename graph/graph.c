#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <X11/Xlib.h>
#include <string.h>

#define kOffset 10

void Initialize(void);
void Graph(void);
void FindLength(void);
void GraphSingleGene(XColor, float, float *);
void ReadData(void);

FILE *dataFile;
float *f,*lr,*mr,*hr,*b,*hl,*ml,*ll,*c;
//NEWGENES
//float *bA, *bE;
float max=0.0, cMax=0.0;
//NEWGENES
//float bAMax=0.0, bEMax=0.0;
float *ft,*r,*l;

Display *display;
Window win;
GC gc;
int top, left, bottom, right, height, width;
long numEntries;
long binaryFile=0;

int main(void)
{
  XEvent theEvent;
  char binaryTest;
  
  Initialize();
  //Try for the current directory first
  dataFile = fopen("data.out","r");

  //If that fails, fallback to the home dir
  if(!dataFile)
    {
      dataFile = fopen("/home/kamui/graph/data.out","r");
      if(!dataFile)
      {
        fprintf(stderr,"Data file not found!\n");
        exit(-1);
      }
    }
  fread(&binaryTest, 1, 1, dataFile);
  if(binaryTest == '!')
    binaryFile = 1;
  
  FindLength();
  ReadData();
  
  Graph();
  
  while(1){
    XNextEvent(display, &theEvent);
    switch (theEvent.type)
      {
      case ButtonPress:
	exit(0);
	break;
      case Expose:
	if(theEvent.xexpose.count == 0)
	  Graph();
	break;
      }
  };
}

void Graph(void)
{
	long i,x,y, horizStep=1, vertStep=1;
	char t[31];
	//RGBColor g = {4096,4096,4096}, p = {25528, 0, 26869};
	XPoint *pointArray;

	XColor gray, red, green, blue, yellow, purple;
	char grayStr[] = "#101010";   //All these values are from
	char redStr[] = "#FF0000";    //the canonical Mac version
	char greenStr[] = "#00FF00";
	char blueStr[] = "#0000FF";
	char yellowStr[] = "#FFFF00";
	char purpleStr[] = "#63B8000068F5";
	Colormap colormap;

	pointArray = (XPoint *)malloc(numEntries * sizeof(XPoint));

	if(!pointArray)
	  {
	    printf("Could not allocate memory for pointArray\n");
	    exit(-1);
	  }

	colormap = DefaultColormap(display, DefaultScreen(display));
	XParseColor(display, colormap, grayStr, &gray);
	XAllocColor(display, colormap, &gray);
	XParseColor(display, colormap, redStr, &red);
	XAllocColor(display, colormap, &red);
	XParseColor(display, colormap, greenStr, &green);
	XAllocColor(display, colormap, &green);
	XParseColor(display, colormap, blueStr, &blue);
	XAllocColor(display, colormap, &blue);
	XParseColor(display, colormap, yellowStr, &yellow);
	XAllocColor(display, colormap, &yellow);
	XParseColor(display, colormap, purpleStr, &purple);
	XAllocColor(display, colormap, &purple);

	if(numEntries > 100)
		horizStep = numEntries/100;
	if(max > 100)
		vertStep = max/100;
	
	XSetForeground(display, gc, gray.pixel);
	for(i=0; i<max; i+=vertStep)
	{
		y = (1-(i/max))*height+top;
		XDrawLine(display, win, gc, left, y, right, y);
       	}

	for(i=0; i<numEntries; i+=horizStep)
	{
		x = i * width / numEntries + left;
		XDrawLine(display, win, gc, x, top, x, bottom);
	}

	//Text_________________________________________________
        XSetForeground(display,gc,WhitePixel(display, DefaultScreen(display)));
	//XSetForeground(display,gc,yellow.pixel);
	sprintf(t, "%f", max);
	XDrawString(display, win, gc, 0, 9, t, strlen(t));
      //XSetForeground(display,gc,WhitePixel(display, DefaultScreen(display)));
	sprintf(t, "%li", numEntries);
	x = right-(4*(log(numEntries)+1));
	y = bottom-1;
	XDrawString(display, win, gc, x, y, t, strlen(t));
	//NEWGENES
	//XSetForeground(display, gc, purple.pixel);	
	//sprintf(t, "%li", (long)bAMax);
	//x = right-(4*(log(bAMax)+1));
	//y = 9;
	//XDrawString(display, win, gc, x, y, t, strlen(t));
	
	//Forward gene_________________________________________
	GraphSingleGene(yellow, max, ft);
	
	//Right________________________________________________
	GraphSingleGene(red, max, r);

	//Blue_________________________________________________
	GraphSingleGene(blue, max, l);
	
	//Forward movement_____________________________________
	y = (1-(ft[0] / (ft[0]+r[0]+l[0]))) * height + top;
	x = left;
      	XSetForeground(display, gc, green.pixel);
	pointArray[0].x = x;
	pointArray[0].y = y;
	for(i=1;i<numEntries;i++)
	{
		y = (1-(ft[i] / (ft[i]+r[i]+l[i]))) * height + top;
		x = i * width / numEntries + left;
		
		pointArray[i].x = x;
		pointArray[i].y = y;
	}
	XDrawLines(display, win, gc, pointArray, i, CoordModeOrigin);

	//Cannibalism__________________________________________
	GraphSingleGene(purple, cMax, c);

	XSync(display, False);
	
}

void GraphSingleGene
(XColor color, float max, float *geneArray )
{
  long x = left;
  long y = (1-(geneArray[0] / max)) * height + top;
  long i;
  float recipValue = 1/max;
  XPoint *pointArray;
  pointArray = (XPoint *)malloc(numEntries * sizeof(XPoint));

  XSetForeground(display, gc, color.pixel);
  pointArray[0].x = x;
  pointArray[0].y = y;

  for(i=1;i<numEntries;i++)
    {
      y = (1-(geneArray[i] * recipValue)) * height + top;
      x = i * width / numEntries + left;
      
      pointArray[i].x = x;
      pointArray[i].y = y;
    }
  XDrawLines(display, win, gc, pointArray, i, CoordModeOrigin);
  XSync(display, False);
}

void FindLength(void)
{
  char c;
  
  fseek(dataFile, 0, SEEK_END);
  //Check for zero-length files
  if(ftell(dataFile) == 0) return;
  
  do { //A linux addition, since we can have partial data lines
    fseek(dataFile, -1, SEEK_CUR);
    fscanf(dataFile, "%c", &c);
  }while (c != 0xA);	     //0xA is the LINE FEED character
  
  if(!binaryFile)
    {		
      do {
	fseek(dataFile, -2, SEEK_CUR);
	fscanf(dataFile, "%c", &c);
      }while (c != 0x9);	     //0x9 is the TAB character
      fscanf(dataFile, "%li\n", &numEntries);
    }
  else  //assume that any end-of-line character actually ends a line, which
    {   //is a bad assumption
      fseek(dataFile, -5, SEEK_CUR);
      fread(&numEntries, sizeof(long), 1, dataFile);
    }
  
  rewind(dataFile);
  if(binaryFile)
    fseek(dataFile, 1, SEEK_SET); //1st char is '!'
}

void ReadData(void)
{
  long i,junk;

  f = (float *)malloc(numEntries * sizeof(float));
  lr = (float *)malloc(numEntries * sizeof(float));
  mr = (float *)malloc(numEntries * sizeof(float));
  hr = (float *)malloc(numEntries * sizeof(float));
  b = (float *)malloc(numEntries * sizeof(float));
  hl = (float *)malloc(numEntries * sizeof(float));
  ml = (float *)malloc(numEntries * sizeof(float));
  ll = (float *)malloc(numEntries * sizeof(float));
  
  c = (float *)malloc(numEntries * sizeof(float));
  
  ft = (float *)malloc(numEntries * sizeof(float));
  r = (float *)malloc(numEntries * sizeof(float));
  l = (float *)malloc(numEntries * sizeof(float));
  
  if(!f || !lr || !mr || !hr || !b || !hl || !ml || !ll || !c || !ft || !r || !l)
    {
      fprintf(stderr, "Could not allocate memory\n");
      exit(-1);
    }
  
  for(i=0; i<numEntries; i++)
    {
      
      if(!binaryFile)
	fscanf(dataFile, "%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%li\n",
	       &f[i],&lr[i],&mr[i],&hr[i],&b[i],&hl[i],&ml[i],&ll[i],&c[i],&junk);
      else
	{
	  fread(&f[i], sizeof(float), 1, dataFile);
	  fread(&lr[i], sizeof(float), 1, dataFile);
	  fread(&mr[i], sizeof(float), 1, dataFile);
	  fread(&hr[i], sizeof(float), 1, dataFile);
	  fread(&b[i], sizeof(float), 1, dataFile);
	  fread(&hl[i], sizeof(float), 1, dataFile);
	  fread(&ml[i], sizeof(float), 1, dataFile);
	  fread(&ll[i], sizeof(float), 1, dataFile);
	  fread(&c[i], sizeof(float), 1, dataFile);

	  fread(&junk, sizeof(unsigned long), 1, dataFile); //i
	  fread(&junk, sizeof(char), 1, dataFile); //new line
	}

      ft[i] = f[i] - b[i];
      if(ft[i] > max)
	max = ft[i];
      
      r[i] = lr[i] + 2*mr[i] + 3*hr[i];
      if(r[i] > max)
	max = r[i];
      l[i] = ll[i] + 2*ml[i] + 3*hl[i];
      if(l[i] > max)
	max = l[i];
      if(c[i] > cMax)
	cMax = c[i];
    }
}

void Initialize(void)
{
  int screen_num;
  int screen_width;
  int screen_height;
  unsigned long white_pixel;
  unsigned long black_pixel;
  Window root_window;
  XGCValues values;
  unsigned long valuemask = 0;
  
  display = XOpenDisplay("\0");
  if(!display)
    {
      fprintf(stderr, "Could not connect to X display server :0\n");
      exit (-1);
    }
  screen_num = DefaultScreen(display);
  screen_width = DisplayWidth(display, screen_num);
  screen_height = DisplayHeight(display, screen_num);
  root_window = RootWindow(display, screen_num);
  white_pixel = WhitePixel(display, screen_num);
  black_pixel = BlackPixel(display, screen_num);
  
  width = 806;
  height = 550;
  top = 0;//kOffset;
  bottom = height;// - kOffset;
  left = 0;//kOffset;
  right = width;// - kOffset;
  
  win = XCreateSimpleWindow(display, RootWindow(display, screen_num), 
			    0, 0, width, height, 2,
			    BlackPixel(display, screen_num),
			    BlackPixel(display, screen_num));
  
  XMapWindow(display, win);
  XSync(display, False);
  
  gc = XCreateGC(display, win, valuemask, &values);
  if(gc < 0)
    {
      fprintf(stderr, "XCreateGC: \n");
    }
  
  XSetForeground(display, gc, BlackPixel(display, screen_num));
  XSetBackground(display, gc, WhitePixel(display, screen_num));
  XSetFillStyle(display, gc, FillSolid);
  XSetLineAttributes(display, gc, 1, LineSolid, CapRound, JoinRound);
  
  XDrawRectangle(display, win, gc, 0, 0, width, height);
  XFillRectangle(display, win, gc, 0, 0, width, height);
  XFlush(display);
  
  XSelectInput(display, win, ExposureMask | ButtonPressMask);
}
