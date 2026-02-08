AR = ar
CC = gcc
CXX = g++

INC_DIR				= ./include
SRC_DIR				= ./src
TESTSRC_DIR			= ./testsrc
BIN_DIR				= ./bin
OBJ_DIR				= ./obj
LIB_DIR				= ./lib
TESTOBJ_DIR			= ./testobj
TESTBIN_DIR			= ./testbin

DEFINES             =
INCLUDE             = -I $(INC_DIR)
ARFLAGS             = rcs
CFLAGS              = -Wall
CPPFLAGS            = --std=c++20
LDFLAGS             = -lcurl

OPENWEATHER_OBJ     = $(OBJ_DIR)/OpenWeather.o

TARGET = $(BIN_DIR)/wind

all: directories $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(INCLUDE) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ_DIR)/main.o $(OBJ_DIR)/OpenWeather.o
	$(CXX) $^ -o $@ $(LDFLAGS)

directories:
	mkdir -p $(BIN_DIR)
	mkdir -p $(OBJ_DIR)
	mkdir -p $(LIB_DIR)
	mkdir -p $(TESTOBJ_DIR)
	mkdir -p $(TESTBIN_DIR)

clean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)
	rm -rf $(LIB_DIR)
	rm -rf $(TESTOBJ_DIR)
	rm -rf $(TESTBIN_DIR)