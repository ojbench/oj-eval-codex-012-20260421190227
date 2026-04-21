/**
 * implement a container like std::linked_hashmap
 */
#ifndef SJTU_LINKEDHASHMAP_HPP
#define SJTU_LINKEDHASHMAP_HPP

// only for std::equal_to<T> and std::hash<T>
#include <functional>
#include <cstddef>
#include "utility.hpp"
#include "exceptions.hpp"

namespace sjtu {
    /**
     * In linked_hashmap, iteration ordering is differ from map,
     * which is the order in which keys were inserted into the map.
     * You should maintain a doubly-linked list running through all
     * of its entries to keep the correct iteration order.
     *
     * Note that insertion order is not affected if a key is re-inserted
     * into the map.
     */
    
template<
	class Key,
	class T,
	class Hash = std::hash<Key>, 
	class Equal = std::equal_to<Key>
> class linked_hashmap {
private:
    typedef pair<const Key, T> kv_type_internal;
    struct Node {
        kv_type_internal kv;
        Node *prev;      // previous in insertion order
        Node *next;      // next in insertion order
        Node *bnext;     // next in bucket chain
        Node(const kv_type_internal &v) : kv(v), prev(nullptr), next(nullptr), bnext(nullptr) {}
    };

    Hash hasher;
    Equal equaler;

    Node **buckets = nullptr;
    size_t bucket_count = 0;
    size_t elem_count = 0;
    size_t grow_threshold = 0; // when elem_count >= threshold, rehash

    Node *head = nullptr; // first in insertion order
    Node *tail = nullptr; // last in insertion order

    static size_t initial_bucket_count() { return 8; }

    void init_buckets(size_t n) {
        bucket_count = n;
        buckets = new Node*[bucket_count];
        for (size_t i = 0; i < bucket_count; ++i) buckets[i] = nullptr;
        grow_threshold = bucket_count * 3 / 4; // load factor ~0.75
    }

    void unlink_from_order(Node *node) {
        if (!node) return;
        if (node->prev) node->prev->next = node->next; else head = node->next;
        if (node->next) node->next->prev = node->prev; else tail = node->prev;
        node->prev = node->next = nullptr;
    }

    void link_at_tail(Node *node) {
        node->prev = tail; node->next = nullptr;
        if (tail) tail->next = node; else head = node;
        tail = node;
    }

    void rehash(size_t new_bucket_count) {
        Node **newBuckets = new Node*[new_bucket_count];
        for (size_t i = 0; i < new_bucket_count; ++i) newBuckets[i] = nullptr;
        // re-link bucket chains by iterating insertion order
        for (Node *p = head; p != nullptr; p = p->next) {
            size_t idx = hasher(p->kv.first) % new_bucket_count;
            p->bnext = newBuckets[idx];
            newBuckets[idx] = p;
        }
        delete [] buckets;
        buckets = newBuckets;
        bucket_count = new_bucket_count;
        grow_threshold = bucket_count * 3 / 4;
    }

    void ensure_capacity_for_insert() {
        if (bucket_count == 0) init_buckets(initial_bucket_count());
        if (elem_count + 1 > grow_threshold) {
            size_t new_count = bucket_count ? bucket_count * 2 : initial_bucket_count();
            if (new_count == 0) new_count = initial_bucket_count();
            rehash(new_count);
        }
    }

    Node* find_node(const Key &key) const {
        if (bucket_count == 0) return nullptr;
        size_t idx = hasher(key) % bucket_count;
        for (Node *p = buckets[idx]; p; p = p->bnext) {
            if (equaler(p->kv.first, key)) return p;
        }
        return nullptr;
    }

    void insert_into_bucket(Node *node) {
        size_t idx = hasher(node->kv.first) % bucket_count;
        node->bnext = buckets[idx];
        buckets[idx] = node;
    }

    void erase_from_bucket(const Key &key, Node *node) {
        size_t idx = hasher(key) % bucket_count;
        Node *prev = nullptr;
        for (Node *p = buckets[idx]; p; p = p->bnext) {
            if (p == node) {
                if (prev) prev->bnext = p->bnext; else buckets[idx] = p->bnext;
                p->bnext = nullptr;
                return;
            }
            prev = p;
        }
    }
public:
	/**
	 * the internal type of data.
	 * it should have a default constructor, a copy constructor.
	 * You can use sjtu::linked_hashmap as value_type by typedef.
	 */
	typedef pair<const Key, T> value_type;
 
	/**
	 * see BidirectionalIterator at CppReference for help.
	 *
	 * if there is anything wrong throw invalid_iterator.
	 *     like it = linked_hashmap.begin(); --it;
	 *       or it = linked_hashmap.end(); ++end();
	 */
	class const_iterator;
	class iterator {
	private:
		linked_hashmap *map = nullptr;
		Node *node = nullptr; // current node in order list; if is_end is true, node is nullptr
		bool is_end = false;
		iterator(linked_hashmap *m, Node *n, bool e): map(m), node(n), is_end(e) {}
		friend class linked_hashmap;
		friend class const_iterator;
	public:
		// The following code is written for the C++ type_traits library.
		// Type traits is a C++ feature for describing certain properties of a type.
		// For instance, for an iterator, iterator::value_type is the type that the 
		// iterator points to. 
		// STL algorithms and containers may use these type_traits (e.g. the following 
		// typedef) to work properly. 
		// See these websites for more information:
		// https://en.cppreference.com/w/cpp/header/type_traits
		// About value_type: https://blog.csdn.net/u014299153/article/details/72419713
		// About iterator_category: https://en.cppreference.com/w/cpp/iterator
		using difference_type = std::ptrdiff_t;
		using value_type = typename linked_hashmap::value_type;
		using pointer = value_type*;
		using reference = value_type&;
		using iterator_category = std::bidirectional_iterator_tag;


		iterator() = default;
		iterator(const iterator &other) = default;
		/**
		 * iter++
		 */
		iterator operator++(int) { iterator tmp(*this); ++(*this); return tmp; }
		/**
		 * ++iter
		 */
		iterator & operator++() {
			if (map == nullptr) throw invalid_iterator();
			if (is_end) throw invalid_iterator();
			if (node->next) { node = node->next; }
			else { node = nullptr; is_end = true; }
			return *this;
		}
		/**
		 * iter--
		 */
		iterator operator--(int) { iterator tmp(*this); --(*this); return tmp; }
		/**
		 * --iter
		 */
		iterator & operator--() {
			if (map == nullptr) throw invalid_iterator();
			if (is_end) {
				if (map->tail == nullptr) throw invalid_iterator();
				node = map->tail; is_end = false; return *this;
			}
			if (node == nullptr || node->prev == nullptr) throw invalid_iterator();
			node = node->prev; return *this;
		}
		/**
		 * a operator to check whether two iterators are same (pointing to the same memory).
		 */
		value_type & operator*() const {
			if (map == nullptr || is_end || node == nullptr) throw invalid_iterator();
			return node->kv;
		}
		bool operator==(const iterator &rhs) const {
			if (map != rhs.map) return false;
			if (is_end && rhs.is_end) return true;
			return node == rhs.node && is_end == rhs.is_end;
		}
		bool operator==(const const_iterator &rhs) const;
		/**
		 * some other operator for iterator.
		 */
		bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
		bool operator!=(const const_iterator &rhs) const;

		/**
		 * for the support of it->first. 
		 * See <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/> for help.
		 */
		value_type* operator->() const noexcept { return &(node->kv); }
	};
 
	class const_iterator {
		// it should has similar member method as iterator.
		//  and it should be able to construct from an iterator.
		private:
			const linked_hashmap *map = nullptr;
			Node *node = nullptr;
			bool is_end = false;
			friend class linked_hashmap;
			friend class iterator;
		public:
			using difference_type = std::ptrdiff_t;
			using value_type = typename linked_hashmap::value_type;
			using pointer = const value_type*;
			using reference = const value_type&;
			using iterator_category = std::bidirectional_iterator_tag;

			const_iterator() = default;
			const_iterator(const const_iterator &other) = default;
			const_iterator(const iterator &other) {
				map = other.map;
				node = other.node;
				is_end = other.is_end;
			}
			const_iterator(const linked_hashmap *m, Node *n, bool e): map(m), node(n), is_end(e) {}

			const_iterator operator++(int) { const_iterator tmp(*this); ++(*this); return tmp; }
			const_iterator & operator++() {
				if (map == nullptr) throw invalid_iterator();
				if (is_end) throw invalid_iterator();
				if (node->next) { node = node->next; }
				else { node = nullptr; is_end = true; }
				return *this;
			}
			const_iterator operator--(int) { const_iterator tmp(*this); --(*this); return tmp; }
			const_iterator & operator--() {
				if (map == nullptr) throw invalid_iterator();
				if (is_end) {
					if (map->tail == nullptr) throw invalid_iterator();
					node = map->tail; is_end = false; return *this;
				}
				if (node == nullptr || node->prev == nullptr) throw invalid_iterator();
				node = node->prev; return *this;
			}
			reference operator*() const {
				if (map == nullptr || is_end || node == nullptr) throw invalid_iterator();
				return node->kv;
			}
			bool operator==(const const_iterator &rhs) const {
				if (map != rhs.map) return false;
				if (is_end && rhs.is_end) return true;
				return node == rhs.node && is_end == rhs.is_end;
			}
			bool operator==(const iterator &rhs) const {
				if (map != rhs.map) return false;
				if (is_end && rhs.is_end) return true;
				return node == rhs.node && is_end == rhs.is_end;
			}
			bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }
			bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
			pointer operator->() const noexcept { return &(node->kv); }
	};
 
	/**
	 * two constructors
	 */
	linked_hashmap() {}
	linked_hashmap(const linked_hashmap &other) : hasher(other.hasher), equaler(other.equaler) {
		if (other.elem_count == 0) {
			return;
		}
		init_buckets(other.bucket_count ? other.bucket_count : initial_bucket_count());
		for (Node *p = other.head; p != nullptr; p = p->next) {
			insert(p->kv);
		}
	}
 
	/**
	 * assignment operator
	 */
	linked_hashmap & operator=(const linked_hashmap &other) {
		if (this == &other) return *this;
		clear();
		hasher = other.hasher;
		equaler = other.equaler;
		if (other.elem_count == 0) return *this;
		init_buckets(other.bucket_count ? other.bucket_count : initial_bucket_count());
		for (Node *p = other.head; p != nullptr; p = p->next) insert(p->kv);
		return *this;
	}
 
	/**
	 * Destructors
	 */
	~linked_hashmap() { clear(); delete [] buckets; buckets = nullptr; bucket_count = 0; }
 
	/**
	 * access specified element with bounds checking
	 * Returns a reference to the mapped value of the element with key equivalent to key.
	 * If no such element exists, an exception of type `index_out_of_bound'
	 */
	T & at(const Key &key) {
		Node *n = find_node(key);
		if (!n) throw index_out_of_bound();
		return n->kv.second;
	}
	const T & at(const Key &key) const {
		Node *n = find_node(key);
		if (!n) throw index_out_of_bound();
		return n->kv.second;
	}
 
	/**
	 * access specified element 
	 * Returns a reference to the value that is mapped to a key equivalent to key,
	 *   performing an insertion if such key does not already exist.
	 */
	T & operator[](const Key &key) {
		Node *n = find_node(key);
		if (n) return n->kv.second;
		ensure_capacity_for_insert();
		Node *nn = new Node(value_type(key, T()));
		link_at_tail(nn);
		insert_into_bucket(nn);
		++elem_count;
		return nn->kv.second;
	}
 
	/**
	 * behave like at() throw index_out_of_bound if such key does not exist.
	 */
	const T & operator[](const Key &key) const {
		Node *n = find_node(key);
		if (!n) throw index_out_of_bound();
		return n->kv.second;
	}
 
	/**
	 * return a iterator to the beginning
	 */
	iterator begin() { return head ? iterator(this, head, false) : iterator(this, nullptr, true); }
	const_iterator cbegin() const { return head ? const_iterator(this, head, false) : const_iterator(this, nullptr, true); }
 
	/**
	 * return a iterator to the end
	 * in fact, it returns past-the-end.
	 */
	iterator end() { return iterator(this, nullptr, true); }
	const_iterator cend() const { return const_iterator(this, nullptr, true); }
 
	/**
	 * checks whether the container is empty
	 * return true if empty, otherwise false.
	 */
	bool empty() const { return elem_count == 0; }
 
	/**
	 * returns the number of elements.
	 */
	size_t size() const { return elem_count; }
 
	/**
	 * clears the contents
	 */
	void clear() {
		// delete all nodes
		Node *p = head;
		while (p) {
			Node *nxt = p->next;
			delete p;
			p = nxt;
		}
		head = tail = nullptr;
		elem_count = 0;
		// reset buckets
		if (buckets) {
			for (size_t i = 0; i < bucket_count; ++i) buckets[i] = nullptr;
		}
	}
 
	/**
	 * insert an element.
	 * return a pair, the first of the pair is
	 *   the iterator to the new element (or the element that prevented the insertion), 
	 *   the second one is true if insert successfully, or false.
	 */
	pair<iterator, bool> insert(const value_type &value) {
		Node *exist = find_node(value.first);
		if (exist) return pair<iterator, bool>(iterator(this, exist, false), false);
		ensure_capacity_for_insert();
		Node *nn = new Node(value);
		link_at_tail(nn);
		insert_into_bucket(nn);
		++elem_count;
		return pair<iterator, bool>(iterator(this, nn, false), true);
	}
 
	/**
	 * erase the element at pos.
	 *
	 * throw if pos pointed to a bad element (pos == this->end() || pos points an element out of this)
	 */
	void erase(iterator pos) {
		if (pos.map != this || pos.is_end || pos.node == nullptr) throw invalid_iterator();
		Node *n = pos.node;
		erase_from_bucket(n->kv.first, n);
		unlink_from_order(n);
		delete n;
		--elem_count;
	}
 
	/**
	 * Returns the number of elements with key 
	 *   that compares equivalent to the specified argument,
	 *   which is either 1 or 0 
	 *     since this container does not allow duplicates.
	 */
	size_t count(const Key &key) const { return find_node(key) ? 1 : 0; }
 
	/**
	 * Finds an element with key equivalent to key.
	 * key value of the element to search for.
	 * Iterator to an element with key equivalent to key.
	 *   If no such element is found, past-the-end (see end()) iterator is returned.
	 */
	iterator find(const Key &key) {
		Node *n = find_node(key);
		if (!n) return end();
		return iterator(this, n, false);
	}
	const_iterator find(const Key &key) const {
		Node *n = find_node(key);
		if (!n) return cend();
		return const_iterator(this, n, false);
	}
};

// cross-type equality implementations
template<class Key, class T, class Hash, class Equal>
bool linked_hashmap<Key,T,Hash,Equal>::iterator::operator==(const const_iterator &rhs) const {
    if (map != rhs.map) return false;
    if (is_end && rhs.is_end) return true;
    return node == rhs.node && is_end == rhs.is_end;
}

template<class Key, class T, class Hash, class Equal>
bool linked_hashmap<Key,T,Hash,Equal>::iterator::operator!=(const const_iterator &rhs) const {
    return !(*this == rhs);
}

}

#endif
