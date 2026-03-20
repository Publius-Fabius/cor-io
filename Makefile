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
run_defer_test : bin/defer_test
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# coroutines 
bin/coroutine_test : tests/corio/coroutine.cpp include/corio/coroutine.h bin  
	$(CC) $(CFLAGS) -o $@ $<
run_coroutine_test : bin/coroutine_test
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# slot_map 
bin/slot_map_test : tests/corio/slot_map.cpp include/corio/slot_map.h bin  
	$(CC) $(CFLAGS) -o $@ $<
run_slot_map_test : bin/slot_map_test
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# kernel_events 
build/kernel_events.o : source/corio/kernel_events.cpp include/corio/kernel_events.h build 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/kernel_events_test : tests/corio/kernel_events.cpp build/kernel_events.o 
	$(CC) $(CFLAGS) -o $@ $^
run_kernel_events_test : bin/kernel_events_test 
	valgrind -q --error-exitcode=1 --track-fds=yes --leak-check=full $^ 1>/dev/null

# server 
build/server.o : source/corio/server.cpp include/corio/server.h build 
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm bin/* || true 
	rm build/* || true 
	rm -r bin || true 
	rm -r build || true