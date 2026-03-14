# Makefile

CFLAGS = -g -std=c++23 -pedantic -Wconversion -Wall -I include
CC = g++

build :
	mkdir build 
bin :
	mkdir bin 

# coroutines 
bin/coroutine_test : tests/corio/coroutine.cpp include/corio/coroutine.h bin  
	$(CC) $(CFLAGS) -o $@ $<
mt_coroutine_test : bin/coroutine_test
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# error.h
#build/selc/error.o : source/selc/error.c include/selc/error.h
#	$(CC) $(CFLAGS) -c -o $@ $<
#bin/test_error : tests/selc/error.c build/selc/error.o
#	$(CC) $(CFLAGS) -o $@ $^
#lib/libselc.a : \
#	build/selc/error.o
#	ar -crs $@ $^

clean:
	rm bin/* || true 
	rm build/* || true 
	rm -r bin || true 
	rm -r build || true