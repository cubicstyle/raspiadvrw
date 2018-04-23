CC=gcc 
CPP=g++
CFLAGS=-I/usr/local/include -L/usr/local/lib -Wall -g
INCS=
OBJS=main.o memory.o gpio.o
LIBS=-lwiringPi
TARGET=rpa

all:$(TARGET)

%.o: %.cpp $(INCS)
	$(CPP) -c -O1 -o $@ $<

$(TARGET): $(OBJS)
	$(CPP) -o $@ $(OBJS) $(LIBS)

clean:
	rm -rf $(TARGET) *.o
