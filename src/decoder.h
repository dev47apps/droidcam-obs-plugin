// Copyright (C) 2020 github.com/aramg
#ifndef __DECODER_H__
#define __DECODER_H__

#include <vector>
#include <mutex>

template<typename T>
struct Queue {
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

struct DataPacket {
    uint8_t *data;
    size_t size;
    size_t used;
    uint64_t pts;

    DataPacket(size_t new_size) {
        size = 0;
        data = 0;
        resize(new_size);
    }

    ~DataPacket(void) {
        if (data) bfree(data);
    }

    void resize(size_t new_size) {
        if (size < new_size){
            data = (uint8_t*) brealloc(data, new_size);
            size = new_size;
        }
    }
};

struct Decoder {
    Queue<DataPacket*> recieveQueue;
    Queue<DataPacket*> decodeQueue;
    int alloc_count;
    bool ready;
    bool failed;

    ~Decoder(void) {
        DataPacket* packet;
        while ((packet = recieveQueue.next_item()) != NULL) {
            delete packet;
            alloc_count --;
        }
        while ((packet = decodeQueue.next_item()) != NULL){
            delete packet;
            alloc_count --;
        }
        ilog("~decoder alloc_count=%d", alloc_count);
    }

    inline DataPacket* pull_ready_packet(void) {
        return decodeQueue.next_item();
    }

    DataPacket* pull_empty_packet(size_t size) {
        DataPacket* packet = recieveQueue.next_item();
        if (!packet) {
            packet = new DataPacket(size);
            ilog("@decoder alloc: size=%ld", size);
            alloc_count ++;
        } else {
            packet->resize(size);
        }
        packet->used = 0;
        return packet;
    }

    inline void push_empty_packet(DataPacket* packet) {
        recieveQueue.add_item(packet);
    }

    virtual void push_ready_packet(DataPacket*) = 0;
    virtual bool decode_video(struct obs_source_frame2*, DataPacket*, bool *got_output) = 0;
};

#endif
