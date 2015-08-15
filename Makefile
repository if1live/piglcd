CC	= clang
CFLAGS	= -Iexternal/glfw/include -W

UNAME	:= $(shell uname)
ifeq ($(UNAME), Linux)
LIBS	:= $(shell PKG_CONFIG_PATH=external/glfw/src pkg-config --libs --static glfw3)
LDFLAGS	= -lwiringPi -lglfw3 -Lexternal/glfw/src $(LIBS)
else
LDFLAGS = \
	-framework OpenGL	\
	-framework Cocoa	\
	-framework CoreVideo	\
	-framework IOKit
endif

OBJS	= piglcd.o main.o
TARGET	= a.out

all: main.o piglcd.o
	$(CC) main.o piglcd.o -o $(TARGET) $(LDFLAGS)

main.o: main.c
	$(CC) main.c -c $(CFLAGS)

piglcd.o: piglcd.c
	$(CC) piglcd.c -c $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf $(TARGET)

run: all
ifeq ($(UNAME), Linux)
	sudo -E bash -c "time sudo ./$(TARGET)"
else
	./$(TARGET)
endif
