SOURCES = physical_layer.cpp data_link_layer.cpp main_laucher.cpp
CC = g++
CFLAGS = -lrt -g
OUTPUT = prog1

main:
	$(CC) $(SOURCES) $(CFLAGS) -o $(OUTPUT)

clean:
	rm $(OUTPUT)

default:
	main
