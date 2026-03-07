CXX := g++

MUDUO_INCLUDE ?= /usr/include
MUDUO_LIB ?= /usr/local/lib

CXXFLAGS := -g -O0 -std=c++17 -D_XOPEN_SOURCE=700 -I$(MUDUO_INCLUDE)
LDFLAGS := -L$(MUDUO_LIB)
LIBS := -lmuduo_net -lmuduo_base -lssl -lcrypto -lpthread

SRC_DIRS := src src/CoroutineLibrary src/server src/util src/gateway

BIN_DIR := bin
BUILD_DIR := build

SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp)) \
        $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cc))

OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(SRCS))) \
        $(patsubst %.cc,$(BUILD_DIR)/%.o,$(filter %.cc,$(SRCS)))

OBJS_NO_MAIN := $(filter-out $(BUILD_DIR)/src/main.o,$(OBJS))

COROUTINE_SERVER := $(BIN_DIR)/CoroutineServer
GATEWAY_SERVER := $(BIN_DIR)/GatewayServer
UPSTREAM_SERVER := $(BIN_DIR)/UpstreamEchoServers
SMOKE_CLIENT := $(BIN_DIR)/GatewaySmokeClient

all: $(COROUTINE_SERVER) $(GATEWAY_SERVER) $(UPSTREAM_SERVER) $(SMOKE_CLIENT)

$(COROUTINE_SERVER): $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@

$(GATEWAY_SERVER): $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@

$(UPSTREAM_SERVER): $(OBJS_NO_MAIN)
	mkdir -p $(BIN_DIR)
	$(CXX) src/examples/upstream_echo_server.cpp $^ $(LDFLAGS) $(LIBS) -o $@

$(SMOKE_CLIENT): $(OBJS_NO_MAIN)
	mkdir -p $(BIN_DIR)
	$(CXX) src/examples/gateway_smoke_client.cpp $^ $(LDFLAGS) $(LIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cc
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: run-gateway run-upstreams run-smoke run-all

run-gateway: $(GATEWAY_SERVER)
	cd $(BIN_DIR) && ./GatewayServer -p 9006 -t 8 -n 0

run-upstreams: $(UPSTREAM_SERVER)
	cd $(BIN_DIR) && ./UpstreamEchoServers

run-smoke: $(SMOKE_CLIENT)
	cd $(BIN_DIR) && ./GatewaySmokeClient 127.0.0.1:9006 ../conf/gateway.conf

run-all: run-upstreams run-gateway
	$(MAKE) run-smoke

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
