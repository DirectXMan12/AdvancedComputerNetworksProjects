#SOURCES = physical_layer.cpp data_link_layer.cpp server.cpp application_layer.cpp client.cpp
SOURCES = physical_layer.cpp data_link_layer.cpp server.cpp main_laucher.cpp client.cpp
CC = g++
CFLAGS = -lrt -gdwarf-2 -g3 -lpthread -lsqlite3
OUTPUT = prog1

main:
	$(CC) $(SOURCES) $(CFLAGS) -I . -o $(OUTPUT)

clean:
	rm $(OUTPUT)

default:
	main
