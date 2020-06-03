// Copyright (C) 2020 github.com/aramg
#ifndef __QEUUUE_H__
#define __QEUUUE_H__

#include <vector>
#include <mutex>

template<typename T>
struct queue {
    std::mutex items_lock;
    std::vector<T> items;

    void add_item(T item) {
        items_lock.lock();
        items.push_back(item);
        items_lock.unlock();
    }

    T next_item(void) {
        T item{};
        if (items.size()) {
            items_lock.lock();
            item = items.front();
            items.erase(items.begin());
            items_lock.unlock();
        }
        return item;
    }
};

#endif
