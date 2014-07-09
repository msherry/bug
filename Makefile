FLAGS=-O3 -fomit-frame-pointer -Wall
#FLAGS=-fprofile-arcs -pg -g -Wall
XFLAGS=-L/usr/X11R6/lib -lX11 -lm

all: bug xbug xbugfast bugwsave

clean:
	rm -f bug xbug xbugfast bugwsave

bug: bug.c
	gcc $(FLAGS) -o bug bug.c

xbug: bug.c
	gcc -DXCOMMON -DX $(FLAGS) $(XFLAGS) -o xbug bug.c

xbugfast: bug.c
	gcc -DXCOMMON -DXFAST $(FLAGS) $(XFLAGS) -o xbugfast bug.c

bugwsave: bug.c
	gcc $(FLAGS) -o bugwsave bug.c

