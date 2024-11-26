# Název výstupního souboru
TARGET = simulation

# Kompilátor
CC = g++

# Cesty ke knihovně a hlavičkovým souborům
SIMLIB_PATH = ./simlib/src
CFLAGS = -Wall -Wextra -std=c++17 -I$(SIMLIB_PATH)
LDFLAGS = -L./simlib/src -l:simlib.a -lm

# Pravidlo pro kompilaci a linkování
all: $(TARGET)

$(TARGET): main.cpp
	$(CC) $(CFLAGS) main.cpp -o $(TARGET) $(LDFLAGS)

# Pravidlo pro spuštění
run: $(TARGET)
	./$(TARGET)

# Pravidlo pro vyčištění
clean:
	rm -f $(TARGET)
