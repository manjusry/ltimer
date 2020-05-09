LIB_TARGET=libltimer.so
INC_PATH=../include

all:$(LIB_TARGET)

$(LIB_TARGET):ltimer.o utils.o
	gcc -shared -o $@ $^
	
%.o:%.c
	gcc -g -Wall -Os -fPIC -I$(INC_PATH) -c $< -o $@

clean:
	rm -f $(LIB_TARGET) *.o
