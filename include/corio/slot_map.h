#ifndef CORIO_SLOT_MAP_H
#define CORIO_SLOT_MAP_H

#include <vector>
#include <queue>
#include <stdint.h>

namespace corio 
{
    template<typename object> class slot_map {    
        struct entry {
            object *pointer;
            uint64_t generation;
            entry(object *ptr = NULL):
                pointer(ptr),
                generation(0ULL) { }
        };

        using entry_set = std::vector<entry>;
        using slot_pool = std::priority_queue<
            int, 
            std::vector<int>, 
            std::greater<int>>;

        entry_set m_entries;
        slot_pool m_pool;
        size_t m_size;

        inline entry &at(const int index) {
            assert(0 <= index && index <= m_entries.size());
            return m_entries.at(index);
        }

        public:

        using object_type = object;

        slot_map() : m_size(0) { }

        inline size_t size() {
            return m_size;
        }

        int acquire(object *pointer) {
            m_entries.emplace();
            if(pool.size() > 0) {
                const int slot = m_pool.top();
                m_pool.pop();
                entry &ent = at(slot);
                ent.pointer = pointer;
                ++ent.generation;
                ++m_size;
                return slot;
            } else {
                const int slot = (int)m_entries.size();
                m_entries.push_back(entry(pointer));
                ++m_size;
                return slot;
            }
        }

        void release(const int slot) {
            entry &ent = at(slot);
            ent.pointer = NULL;
            ++ent.generation;
            m_pool.push(slot);
            --m_size;
        }

        inline uint32_t get_generation(const int slot) {
            return at(slot).generation;
        }

        inline object* operator[](const size_t slot) {
            return at(slot).pointer;
        }

        inline uint32_t increment_generation(const int slot) {
            return ++at(slot).generation;
        }

        inline object* with_generation(const int slot, const uint32_t gen) {
            entry &ent = at(slot);
            return ent.generation == gen ? ent.pointer : NULL;
        }

        struct iterator {
            std::vector<entry> &entries;
            int slot;
            iterator(std::vector<entry> &entries_, int slot_) : 
                entries(entries_),
                slot(slot_) 
            {
                for(;slot < entries.size(); ++slot) 
                    if(entries[slot].pointer)
                        break;
            }
            iterator& operator++() {
                while(slot < entries.size()) 
                    if(entries[++slot].pointer)
                        break;
                return *this;
            }
            bool operator!=(const iterator& other) const {
                return slot != other.slot;
            }
            auto operator*() const {
                return entries[slot].pointer;
            }
        };

        iterator begin() {
            return iterator(m_entries, 0);
        }

        iterator end() {
            return iterator(m_entries, m_entries.size());
        }
    };
}

#endif