# Mach - Cross-Platform Makefile

# Directories
SRC_DIR = src
ASM_DIR = $(SRC_DIR)/asm
OBJ_DIR = obj

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
UNAME_M := $(shell uname -m 2>/dev/null || echo x86_64)

# Compiler
CC = gcc
CFLAGS = -Wall -Wextra -O3 -I$(SRC_DIR)

# Target
TARGET = mach

# Platform-specific settings
ifeq ($(UNAME_S),Darwin)
    # macOS
    CFLAGS += -I/opt/homebrew/opt/openssl@3/include
    LDFLAGS = -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -lpthread
    ifeq ($(UNAME_M),arm64)
        ASM_SRC = fast_stats_arm64.s
    else
        ASM_SRC = fast_stats_x86.s
    endif
    HTTP_SRC = http.c
    TERM_SRC = terminal.c
else ifeq ($(UNAME_S),Linux)
    # Linux
    LDFLAGS = -lssl -lcrypto -lpthread
    ASM_SRC = fast_stats_x86.s
    HTTP_SRC = http.c
    TERM_SRC = terminal.c
else
    # Windows (MinGW)
    TARGET = mach.exe
    LDFLAGS = -lssl -lcrypto -lws2_32
    ASM_SRC = fast_stats_x86.s
    HTTP_SRC = http_win.c
    TERM_SRC = terminal_win.c
endif

# Source files
SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,obj/%.o,$(SRCS)) $(OBJ_DIR)/$(ASM_SRC:.s=.o)

# Rules
all: $(OBJ_DIR) $(TARGET)
	@echo "Built for $(UNAME_S) $(UNAME_M)"

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
ifeq ($(UNAME_S),Darwin)
	strip $(TARGET)
else ifeq ($(UNAME_S),Linux)
	strip $(TARGET)
endif

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(ASM_DIR)/%.s
	$(CC) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET) mach.exe

.PHONY: all clean
