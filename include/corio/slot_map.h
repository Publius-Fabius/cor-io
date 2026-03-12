#ifndef CORIO_SLOT_MAP_H
#define CORIO_SLOT_MAP_H

#include <vector>
#include <stdint.h>

namespace corio 
{
    template<typename object> class slot_map 
    {    
        struct entry 
        {
            object *pointer;
            uint32_t generation;
            uint32_t epoch;
            entry(object *ptr):
                pointer(ptr),
                generation(0),
                epoch(0) { }
        };

        std::vector<entry> entries;
        std::vector<int> pool;

        public:

        using object_type = object;

        inline entry &at(const int index)
        {
            return entries.at(index);
        }

        int acquire(object *pointer)
        {
            if(pool.size() > 0) {
                const int slot = pool.back();
                pool.pop_back();
                entry &ent = at(slot);
                ent.pointer = pointer;
                ++ent.epoch;
                ++ent.generation;
                return slot;
            } else {
                const int slot = (int)entries.size();
                entries.push_back(entry(pointer));
                return slot;
            }
        }

        void release(const int slot)
        {
            entry &ent = at(slot);
            ent.pointer = NULL;
            ++ent.epoch;
            ++ent.generation;
            pool.push_back(slot);
        }

        inline uint32_t get_generation(const int slot)
        {
            return at(slot).generation;
        }

        inline uint32_t get_epoch(const int slot)
        {
            return at(slot).epoch;
        }

        inline object* get_pointer(const int slot)
        {
            return at(slot).pointer;
        }

        inline uint32_t increment_epoch(const int slot)
        {
            return ++at(slot).epoch;
        }

        inline object* with_generation(const int slot, const uint32_t gen)
        {
            entry &ent = at(slot);
            return ent.generation == gen ? ent.pointer : NULL;
        }

        inline object* with_epoch(const int slot, const uint32_t epoch)
        {
            entry &ent = at(slot);
            return ent.epoch == epoch ? ent.pointer : NULL;
        }
    };
}

#endif