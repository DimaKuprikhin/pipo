CXX = g++
CFLAGS = -g -Wall -Werror -std=c++20

task1:
	$(CXX) $(CFLAGS) task1/useless.cpp -o task1/useless

task1-test: task1
	./task1/useless task1/commands.txt

task3:
	$(CXX) $(CFLAGS) task3/runsim.cpp -o task3/runsim

task4:
	$(CXX) $(CFLAGS) task4/src/dish_washing.cpp -o task4/dish_washing

task4-test: task4
	export TABLE_LIMIT=3
	./task4/dish_washing task4/test-data/washing_times.txt task4/test-data/wiping_times.txt task4/test-data/dishes.txt fifo > task4/test-data/output.txt
	sort -k2n -k1.1,1.2 task4/test-data/output.txt > task4/test-data/sorted_output.txt
	cmp task4/test-data/sorted_output.txt task4/test-data/expected_output.txt
	./task4/dish_washing task4/test-data/washing_times.txt task4/test-data/wiping_times.txt task4/test-data/dishes.txt pipe > task4/test-data/output.txt
	sort -k2n -k1.1,1.2 task4/test-data/output.txt > task4/test-data/sorted_output.txt
	cmp task4/test-data/sorted_output.txt task4/test-data/expected_output.txt
	./task4/dish_washing task4/test-data/washing_times.txt task4/test-data/wiping_times.txt task4/test-data/dishes.txt msg > task4/test-data/output.txt
	sort -k2n -k1.1,1.2 task4/test-data/output.txt > task4/test-data/sorted_output.txt
	cmp task4/test-data/sorted_output.txt task4/test-data/expected_output.txt
	./task4/dish_washing task4/test-data/washing_times.txt task4/test-data/wiping_times.txt task4/test-data/dishes.txt socket > task4/test-data/output.txt
	sort -k2n -k1.1,1.2 task4/test-data/output.txt > task4/test-data/sorted_output.txt
	cmp task4/test-data/sorted_output.txt task4/test-data/expected_output.txt
	./task4/dish_washing task4/test-data/washing_times.txt task4/test-data/wiping_times.txt task4/test-data/dishes.txt shm > task4/test-data/output.txt
	sort -k2n -k1.1,1.2 task4/test-data/output.txt > task4/test-data/sorted_output.txt
	cmp task4/test-data/sorted_output.txt task4/test-data/expected_output.txt

task5:
	$(CXX) $(CFLAGS) -c task5/async_multiplier.cpp -o task5/async_multiplier.o
	$(CXX) $(CFLAGS) task5/main.cpp -o task5/factorial task5/async_multiplier.o
	rm task5/async_multiplier.o

task6:
	$(CXX) $(CFLAGS) task6/task_6.cpp -o task6/task_6

task6-test: task6
	./task6/task_6

.PHONY: task1 task3 task4 task5 task6
