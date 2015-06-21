#
# Copyright (c) 2015 Sergi Granell (xerpi)
#

TARGET = nidsparser
OBJS = main.o

CFLAGS = -Wall -std=c++0x
LIBS =

all: $(TARGET)

$(TARGET): $(OBJS)
	g++ -o $@ $^ $(LIBS)

%.o: %.cpp
	g++ -c -o $@ $< $(CFLAGS)

clean:
	@rm -f $(OBJS) $(TARGET)
