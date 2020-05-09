INC_PATH=../include
LIB_PATH=../src
LIBS=-pthread -lltimer

TARGET=test_timer

all:$(TARGET) 

test_timer:test_timer.o
	gcc -g -Os -o $@ -L$(LIB_PATH) $(LIBS) $^ -Wl,-rpath $(LIB_PATH)

%.o:%.c
	gcc -g -Wall -Os -I$(INC_PATH) -c $< -o $@

clean:
	rm -f *.o
	rm -f $(TARGET)
