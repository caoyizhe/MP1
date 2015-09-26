# the compiler: gcc for C program, define as g++ for C++
CC = g++

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g

# the build target executable:
TARGET = MP1

all: $(TARGET)

$(TARGET): $(TARGET).C
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).C

clean:
	$(RM) $(TARGET)