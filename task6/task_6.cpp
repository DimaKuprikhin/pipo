#include <iostream>
#include <vector>

#include "merge_sort.hpp"

class Tester {
 public:
  template <typename T, typename Compare = std::less<T>>
  void test(std::vector<T>&& data,
            const Compare comp = Compare{},
            const size_t processors_count = 1,
            const ProcessorType processor_type = ProcessorType::Thread) {
    merge_sort(&data[0], &data[0] + data.size(), comp, processors_count, processor_type);

    std::vector<T> expected = data;
    std::sort(expected.begin(), expected.end(), comp);

    if (data != expected) {
      std::cout << "TEST " << test_number << " FAILED" << std::endl;
    }
    ++test_number;
  }

 private:
  size_t test_number = 1;
};

int main() {
  Tester t;
  t.test<char>({});
  t.test<int>({0});
  t.test<int>({0, 1, 2});
  t.test<int>({0, 3, 2, 1, -2});
  t.test<std::string>({"a", "b", "d", "c"});
  t.test<std::string>({"a", "b", "d", "c"}, [](const std::string& lhs, const std::string& rhs) -> bool { return lhs >= rhs; });

  struct Pair {
    std::string first;
    int second;

    bool operator==(const Pair& other) const {
      return first == other.first || second == other.second;
    }
  };
  t.test<Pair>({{"a", 3}, {"b", 4}, {"c", 2}}, [](const Pair& lhs, const Pair& rhs) -> bool { return lhs.second < rhs.second; });

  t.test<int>({3, 2, 4, 3}, std::less<int>(), 2, ProcessorType::Thread);

  t.test<int>({3, 2, 4, 3}, std::less<int>(), 2, ProcessorType::Process);
  t.test<int>({3, 2, 4, 3}, std::less<int>(), 3, ProcessorType::Process);
  t.test<int>({3, 2, 4, 3}, std::less<int>(), 4, ProcessorType::Process);

  const int size = 100000;
  std::vector<int> test_data;
  test_data.reserve(size);
  srand(123456);
  for (int i = 0; i < size; ++i) {
    test_data.push_back(rand());
  }

  for (int i = 1; i < 10; ++i) {
    t.test<int>(std::vector<int>(test_data), std::less<int>(), i, ProcessorType::Thread);
  }
  for (int i = 1; i < 10; ++i) {
    t.test<int>(std::vector<int>(test_data), std::less<int>(), i, ProcessorType::Process);
  }
}
