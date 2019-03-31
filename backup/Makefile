CC := g++
CFLAGS := -Wall -O -g
TARGET := httpServer.out
SOURCES := $(wildcard *.cc)
OBJS := $(patsubst *.cc, *.o, $(SOURCES))
LIB := -lpthread
%.o:%.cc
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET):$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIB)
.PHONY : clean
clean:
	-rm $(TARGET) httpServer.out
debug:
	@echo $(SOURCES)
