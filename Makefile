CC	= clang
CFLAGS	= -Iexternal/glfw/include -W

LIBS	:= $(shell PKG_CONFIG_PATH=external/glfw/src pkg-config --libs --static glfw3)
LDFLAGS	= -lglfw3 -Lexternal/glfw/src $(LIBS)

UNAME	:= $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS	+= -lwiringPi
endif

OBJS	= piglcd.o main.o lodepng.c
TARGET	= a.out

all: main.o piglcd.o lodepng.o
	$(CC) main.o piglcd.o lodepng.o -o $(TARGET) $(LDFLAGS)

main.o: main.c
	$(CC) main.c -c $(CFLAGS)

piglcd.o: piglcd.c
	$(CC) piglcd.c -c $(CFLAGS)

lodepng.o: lodepng.c
	$(CC) lodepng.c -c $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf $(TARGET)

run: all
ifeq ($(UNAME), Linux)
	sudo -E bash -c "time sudo ./$(TARGET)"
else
	./$(TARGET)
endif
