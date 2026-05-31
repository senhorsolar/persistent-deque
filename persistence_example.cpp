#include <algorithm>
#include <iostream>

#include "PersistentDeque.h"

int main()
{
    using DequeType=PersistentDeque<int>;

    std::cout << "Testing PersistentQueue\n";
    DequeType deque;

    std::cout << "Buffer capacity = " << deque.BufferCapacity << '\n';

    std::cout << "Push Back:\n";
    for (std::size_t i = 0; i < 10; ++i) {
        std::cout << "-- " << i << "\n";
        deque.PushBack(i);
    }

    std::cout << "Accessing:\n";
    for (std::size_t i = 0; i < 10; ++i) {
        std::cout << "-- deque[" << i << "] = " << deque.At(i) << '\n';
    }

    std::cout << "\nCopying deque\n\n";
    DequeType other = deque;

    std::cout << "Pop Front (original deque):\n";
    while (!deque.IsEmpty()) {
        std::cout << "-- " << deque.PopFront() << '\n';
    }

    std::cout << "Accessing copy:\n";
    for (std::size_t i = 0; i < 10; ++i) {
        std::cout << "-- deque[" << i << "] = " << other.At(i) << '\n';
    }

    int value_to_find = 3;

    std::cout << "\nSearching for value " << value_to_find << '\n';
    auto it = std::lower_bound(other.begin(), other.end(), 3);
    if (it == other.end()) {
        std::cout << "-- Couldn't find " << value_to_find << '\n';
    } else {
        std::cout << "-- Found " << value_to_find << " using binary search\n";
    }
}
