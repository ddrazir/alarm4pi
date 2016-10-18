# Makefile for alarm4pi project
# Declaration of variables
CC = gcc
CC_FLAGS = -Wall -g

# alarm4pi main (deamon) executable file 
EXEC = alarm4pid
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

# Main target
$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) -lminiupnpc -lresolv -pthread

# To obtain object files which use header file
%.o: %.c %.h
	$(CC) -c $< -o $@ $(CC_FLAGS)

# To obtain object files which do not use header header
%.o: %.c
	$(CC) -c $< -o $@ $(CC_FLAGS)

# To remove generated temporary files
clean:
	rm -f $(OBJECTS)
