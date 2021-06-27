CFLAGS=-DUNIX 
INCLUDE=-I/usr/local/include 
#CFLAGS=-DUNIX -lreadline 
LIBS= -L/usr/local/lib -lreadline -lcurses
DEBUG=-g
#DEBUG=
OBJS = parse.o shell.o

all: shell

%.o: %.c
	gcc -c -o $@ $(INCLUDE) ${CFLAGS} $< 

shell: $(OBJS)	
	gcc -o myshell $(OBJS) $(LIBS)
clean:
	rm -f shell $(OBJS)

