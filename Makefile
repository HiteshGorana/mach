# Mach - Cross-Platform Makefile

# Directories
SRC_DIR = src
ASM_DIR = $(SRC_DIR)/asm
OBJ_DIR = obj

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
UNAME_M := $(shell uname -m 2>/dev/null || echo x86_64)

# Compiler
CC ?= gcc
BASE_CFLAGS = -Wall -Wextra -I$(SRC_DIR)
OPT_CFLAGS ?= -O3
EXTRA_CFLAGS ?=
CFLAGS = $(BASE_CFLAGS) $(OPT_CFLAGS) $(EXTRA_CFLAGS)
LDFLAGS_EXTRA ?=
STRIP ?= 1

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
CORE_SRCS = main.c attacker.c stats.c ui.c storage.c updater.c url.c
SRCS = $(addprefix src/, $(CORE_SRCS) $(HTTP_SRC) $(TERM_SRC))
OBJS = $(patsubst src/%.c,obj/%.o,$(SRCS)) $(OBJ_DIR)/$(ASM_SRC:.s=.o)

# Rules
all: $(OBJ_DIR) $(TARGET)
	@echo "Built for $(UNAME_S) $(UNAME_M)"

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LDFLAGS_EXTRA)
ifeq ($(STRIP),1)
ifeq ($(UNAME_S),Darwin)
	strip $(TARGET)
else ifeq ($(UNAME_S),Linux)
	strip $(TARGET)
endif
endif

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(ASM_DIR)/%.s
	$(CC) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET) mach.exe

debug:
	$(MAKE) clean
	$(MAKE) OPT_CFLAGS="-O0 -g3 -DDEBUG" STRIP=0

asan:
	$(MAKE) clean
	$(MAKE) OPT_CFLAGS="-O1 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS_EXTRA="-fsanitize=address,undefined" STRIP=0

.PHONY: all clean debug asan
