DIR_BIN := .
DIR_OBJ := ./obj
DIR_SRC := ./HttpServer

SRC := $(wildcard $(DIR_SRC)/*.cc)
OBJ := $(patsubst %.cc,$(DIR_OBJ)/%.o,$(notdir $(SRC)))
LIB := -lpthread


CXX_FLAGS := -Wall -O3 -g -fstack-protector -fstack-protector-all
CC := g++

TARGET := httpServer

$(DIR_BIN)/$(TARGET) : $(OBJ)
	$(CC) $(CXX_FLAGS) -o $@ $^ $(LIB)

$(DIR_OBJ)/%.o : $(DIR_SRC)/%.cc
	if [ ! -d $(DIR_OBJ) ];	then mkdir -p $(DIR_OBJ); fi;
	$(CC) $(CXX_FLAGS) -c $< -o $@

.PHONY : clean
clean : 
	-rm -rf $(DIR_OBJ)
