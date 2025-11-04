# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS = -pthread

# Targets
TARGETS = temp_reader temp_logger temp_daemon

# Source files
READER_SRC = temp_reader.c
LOGGER_SRC = temp_logger.c
DAEMON_SRC = temp_daemon.c

# Object files
READER_OBJ = $(READER_SRC:.c=.o)
LOGGER_OBJ = $(LOGGER_SRC:.c=.o)
DAEMON_OBJ = $(DAEMON_SRC:.c=.o)

# Default target
all: $(TARGETS)

# Build temp_reader
temp_reader: $(READER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Build temp_logger
temp_logger: $(LOGGER_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Build temp_daemon
temp_daemon: $(DAEMON_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Generic rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(TARGETS) *.o *.log

# Clean everything including logs
distclean: clean
	rm -f temperature_log.csv *.dat

# Install (copy to /usr/local/bin)
install: all
	sudo cp $(TARGETS) /usr/local/bin/
	sudo chmod 755 /usr/local/bin/temp_*

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/temp_reader
	sudo rm -f /usr/local/bin/temp_logger
	sudo rm -f /usr/local/bin/temp_daemon

# Run temp_reader
run-reader: temp_reader
	./temp_reader

# Run temp_logger
run-logger: temp_logger
	./temp_logger

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Help
help:
	@echo "Available targets:"
	@echo "  all         - Build all programs (default)"
	@echo "  temp_reader - Build basic temperature reader"
	@echo "  temp_logger - Build multi-threaded logger"
	@echo "  temp_daemon - Build daemon version"
	@echo "  clean       - Remove build artifacts"
	@echo "  distclean   - Remove all generated files"
	@echo "  install     - Install programs to /usr/local/bin"
	@echo "  uninstall   - Remove installed programs"
	@echo "  run-reader  - Build and run temp_reader"
	@echo "  run-logger  - Build and run temp_logger"
	@echo "  debug       - Build with debug symbols"
	@echo "  help        - Show this help message"

.PHONY: all clean distclean install uninstall run-reader run-logger debug help