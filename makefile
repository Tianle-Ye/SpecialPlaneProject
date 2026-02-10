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

OPENSKY_OBJ         = $(OBJ_DIR)/OpenSky.o

WIND_TARGET = $(BIN_DIR)/wind
FLIGHT_TARGET = $(BIN_DIR)/flights

all: directories $(WIND_TARGET) $(FLIGHT_TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(INCLUDE) $(CFLAGS) -c $< -o $@

$(WIND_TARGET): $(OBJ_DIR)/main.o $(OBJ_DIR)/OpenWeather.o $(OBJ_DIR)/OpenSky.o
	$(CXX) $^ -o $@ $(LDFLAGS)

$(FLIGHT_TARGET): $(OBJ_DIR)/main.o $(OBJ_DIR)/OpenWeather.o $(OBJ_DIR)/OpenSky.o
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