CC	= clang
CFLAGS	= -Iexternal/glfw/include -W
#LDFLAGS	= -lwiringPi
LDFLAGS	= \
	-lglfw3 -Lexternal/glfw/src	\
	-framework OpenGL	\
	-framework Cocoa	\
	-framework CoreVideo	\
	-framework IOKit

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
	#sudo -E bash -c "time sudo ./$(TARGET)"
	./$(TARGET)
