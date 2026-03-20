#ifndef CORIO_SLOT_MAP_H
#define CORIO_SLOT_MAP_H

#include <vector>
#include <stdint.h>
#include <assert.h>

namespace corio 
{
    template<typename object> class slot_map {    

        using object_type = object;

        struct entry {
            object *pointer;
            uint64_t generation;
            entry(object *ptr = NULL):
                pointer(ptr),
                generation(0ULL) { }
        };

        using entry_set = std::vector<entry>;
        using slot_pool = std::vector<int>;
      
        entry_set entries;
        slot_pool pool;
        size_t m_size;

        inline entry &at(const size_t index) {
            assert(0 <= index && index <= entries.size());
            return entries.at(index);
        }

        public:

        slot_map() : m_size(0) { }

        inline size_t size() {
            return m_size;
        }

        int acquire(object *pointer) {
            assert(pointer != nullptr);
            int slot = -1;
            if(pool.size() > 0) {
                slot = pool.back();
                pool.pop_back();
                entry &ent = at(slot);
                assert(ent.pointer == nullptr);
                ent.pointer = pointer;
                ++ent.generation;
            } else {
                slot = (int)entries.size();
                entries.push_back(entry(pointer));
            }
            ++m_size;
            return slot;
        }

        void release(const int slot) {
            auto &ent = at(slot);
            assert(ent.pointer != nullptr);
            ent.pointer = nullptr;
            ++ent.generation;
            pool.push_back(slot);
            --m_size;
        }

        inline uint64_t get_generation(const int slot) {
            return at(slot).generation;
        }

        inline object* operator[](const size_t slot) {
            return at(slot).pointer;
        }

        inline uint64_t increment_generation(const int slot) {
            return ++at(slot).generation;
        }

        inline object* with_generation(const int slot, const uint64_t gen) {
            auto &ent = at(slot);
            return ent.generation == gen ? ent.pointer : nullptr;
        }

        struct iterator {
            std::vector<entry> &entries;
            size_t slot;
            inline iterator(std::vector<entry> &entries_, size_t slot_) : 
                entries(entries_),
                slot(slot_) 
            {
                for(;slot < entries.size(); ++slot) 
                    if(entries[slot].pointer) break;
            }
            inline iterator& operator++() {
                while(++slot < entries.size()) 
                    if(entries[slot].pointer) break;
                return *this;
            }
            inline bool operator!=(const iterator& other) const {
                return slot != other.slot;
            }
            inline object* operator*() const {
                return entries[slot].pointer;
            }
        };

        inline iterator begin() {
            return iterator(entries, 0);
        }

        inline iterator end() {
            return iterator(entries, entries.size());
        }
    };
}

#endif