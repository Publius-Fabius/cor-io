# Makefile

CFLAGS = -g -std=c++23 -pedantic -Wconversion -Wall -I include
CC = g++

build :
	mkdir build 
bin :
	mkdir bin 

# defer 
bin/defer_test : tests/corio/defer.cpp include/corio/defer.h bin  
	$(CC) $(CFLAGS) -o $@ $<
mt_defer_test : bin/defer_test
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null


# coroutines 
bin/coroutine_test : tests/corio/coroutine.cpp include/corio/coroutine.h bin  
	$(CC) $(CFLAGS) -o $@ $<
mt_coroutine_test : bin/coroutine_test
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# slot_map 
bin/slot_map_test : tests/corio/slot_map.cpp include/corio/slot_map.h bin  
	$(CC) $(CFLAGS) -o $@ $<
mt_slot_map_test : bin/slot_map_test
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