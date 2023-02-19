CXX = g++
CFLAGS = -g -Wall -Werror -std=c++20

task5:
	$(CXX) $(CFLAGS) -c task5/async_multiplier.cpp -o task5/async_multiplier.o
	$(CXX) $(CFLAGS) task5/main.cpp -o task5/factorial task5/async_multiplier.o
	rm task5/async_multiplier.o

task6:
	$(CXX) $(CFLAGS) task6/task_6.cpp -o task6/task_6

.PHONY: task5 task6
