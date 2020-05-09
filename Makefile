all: lib_target test_target

lib_target:
	cd src && make

test_target:
	cd test && make

clean:
	cd src  && make clean;
	cd test && make clean;
