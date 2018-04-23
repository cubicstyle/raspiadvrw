CC=gcc 
CPP=g++
CFLAGS=-I/usr/local/include -L/usr/local/lib -Wall -g
INCS=
SRCDIR= src
SOURCES   = $(wildcard $(SRCDIR)/*.cpp)
OBJS=$(SOURCES:%.cpp=%.o)
LIBS=-lwiringPi
TARGET=rpa

all:$(TARGET)

%.o: %.cpp $(INCS)
	$(CPP) -c -O1 -o $@ $<

$(TARGET): $(OBJS)
	$(CPP) -o $@ $(OBJS) $(LIBS)

clean:
	rm -rf $(TARGET) $(SRCDIR)/*.o

pkg:
	cp $(TARGET) deb_pkg/raspiadvrw/usr/local/bin/
	dpkg-deb -b deb_pkg/raspiadvrw/ raspiadvrw.deb

