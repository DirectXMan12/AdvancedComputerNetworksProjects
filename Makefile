SOURCES = physical_layer.cpp data_link_layer.cpp main_laucher.cpp
CC = g++
CFLAGS = -lrt -gdwarf-2 -g3 -lpthread
OUTPUT = prog1

main:
	$(CC) $(SOURCES) $(CFLAGS) -I . -o $(OUTPUT)

clean:
	rm $(OUTPUT)

default:
	main
