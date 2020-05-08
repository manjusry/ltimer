TEST_TARGET=test_timer

all:$(TEST_TARGET)

$(TEST_TARGET):timer.o utils.o test_timer.o
	gcc -g -Wall -pthread $^ -o $@
	
%.o:%.c
	gcc -g -Wall -c $< -o $@

clean:
	rm -f $(TEST_TARGET) *.o
