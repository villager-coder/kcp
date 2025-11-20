
CXX := g++
CC  := gcc
CXXFLAGS := -O2 -Wall -Wextra -std=c++17
CFLAGS   := -O2 -Wall -Wextra
LDFLAGS  := 

# 源文件
SERVER_SRC := server.cc
CLIENT_SRC := client.cc
KCP_SRC    := ../../ikcp.c

# 目标
SERVER_BIN := server
CLIENT_BIN := client

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC) $(KCP_SRC)
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) $(KCP_SRC) -o $@ $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_SRC) $(KCP_SRC)
	$(CXX) $(CXXFLAGS) $(CLIENT_SRC) $(KCP_SRC) -o $@ $(LDFLAGS)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) *.o

.PHONY: all clean
