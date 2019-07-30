CC := g++
CFLAGS := -Wall -O -g -fstack-protector -fstack-protector-all
TARGET := httpServer.out
SOURCES := $(wildcard *.cc)
OBJS := $(patsubst *.cc, *.o, $(SOURCES))
LIB := -lpthread
%.o:%.cc
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIB)
.PHONY : clean
clean:
	-rm $(TARGET) httpServer.out
debug:
	@echo $(SOURCES)
