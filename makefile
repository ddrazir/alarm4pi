# Makefile for alarm4pi project
# Declaration of variables

ODIR = obj
SDIR = src

CC = gcc
CC_FLAGS = -Wall -g

LIBS = -lminiupnpc -lresolv -pthread

# alarm4pi main (deamon) executable file
EXEC = alarm4pid

SOURCES = $(wildcard $(SDIR)/*.c)
OBJECTS = $(patsubst $(SDIR)/%,$(ODIR)/%,$(SOURCES:.c=.o))

# Main target
$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LIBS)

# To obtain object files which use header file
$(ODIR)/%.o: $(SDIR)/%.c $(SDIR)/%.h
	$(CC) -c $< -o $@ $(CC_FLAGS)

# To obtain object files which do not use header
$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -c $< -o $@ $(CC_FLAGS)

# To remove generated temporary files
clean:
	rm -f $(OBJECTS)

# To create the log directory for alarm4pi 
install:
	mkdir -p log
