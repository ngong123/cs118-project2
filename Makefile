# CC=g++
# CFLAGS=-Wall -Wextra -Werror

# all: clean build

# default: build

# build: server.cpp client.cpp
# 	gcc -Wall -Wextra -o server server.cpp
# 	gcc -Wall -Wextra -o client client.cpp

# clean:
# 	rm -f server client output.txt project2.zip

# zip: 
# 	zip project2.zip server.cpp client.cpp utils.h Makefile README


CC=g++
CFLAGS=-Wall -Wextra

all: clean build

default: build

build: server client

server: server.cpp
	$(CC) $(CFLAGS) -o server server.cpp

client: client.cpp
	$(CC) $(CFLAGS) -o client client.cpp

clean:
	rm -f server client output.txt project2.zip

zip: 
	zip project2.zip server.cpp client.cpp utils.h Makefile README
