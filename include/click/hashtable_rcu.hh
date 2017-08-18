#ifndef CLICK_HASHTABLE_RCU_HH
#define CLICK_HASHTABLE_RCU_HH
/*
* hashtable_rcu.hh -- Lockless, threadsafe HashTable template
* Neil McGlohon
*
* Copyright (c) 2017 Cisco Meraki
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, subject to the conditions
* listed in the Click LICENSE file. These conditions include: you must
* preserve this copyright notice, and you cannot mention the copyright
* holders in advertising related to the Software without their permission.
* The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
* notice is a summary of the Click LICENSE file; the license in that file is
* legally binding.
*
*/
#include <click/hashcode.hh>
CLICK_DECLS

/** @file <click/hashtable_rcu.hh>
* @brief Lockless HashTable container template
*/

template <class K, class V> class HashTable_RCU;
template <class K, class V> class HashTable_RCU_const_iterator;
template <class K, class V> class HashTable_RCU_iterator;


/** @brief iterate over list of given type in manner of linux kernel 3.0x
 * The reason for this deprecated macro usage is that the algorithm benchmark
 * code that was referenced during implementation utilized one of the loop cursors
 * in the body of the loop as well as outside of the scope, expecting it defined.
 * @param tpos the type * to use as a loop cursor.
 * @param pos the &struct hlist_node to use as a loop cursor.
 * @param head the head for your list.
 * @param member the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_linuxV30(tpos, pos, head, member)            \
    for (pos = (head)->first;                    \
         pos &&                          \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->next)


/** @class HashTable_RCU
    @brief Hash Table Template with Lockless thread safety using RCU - READ DOCS BEFORE USE

    This HashTable template implementation is based off of the algorithm and
    benchmarking done by Triplett et. al. in 'Resizable, Scalable, Concurrent
    Hash Tables via Relativistic Programming'. It leverages RCU to allow for
    read operations on the table, those that do not alter the topology of the
    data structure itself, to be performed locklessly. Write operations,
    those that change the topology, can be
    performed without traditional locks and concurrently with read operations
    due to the safety guarantees of RCU.

    Methods that classify as write operations are:
		find_insert(const K &key)
		insert(const K &key)
		set(const K &key, const V &value)*
		erase(iterator it)
		erase(K &key)
		clear()
		swap(HashTable_RCU<K,V> &x)
		resize(size_t new_num_buckets)


    It is important to understand how RCU works before using this hashtable.
    If the rules of RCU are not respected it is very easy to misuse this table
    and result in invalid memory being read and ultimately causing a panic in a
    non-deterministic way. It working for 24 hours is not indicative of its
    stability in the next 24 if the rules are not followed.

    It is not possible to create an rcu lockless data structure that is
    guaranteed to be safe regardless of use. In C++ it is always possible
    to do something, even accidentally, that breaks the semantics of the
    data structure.

    -- RCU --
    It is a method of concurrency control that avoids the need for traditional
    locks. It allows for multiple readers and a single writer to be operating
    on the data at the same time without blocking or locking.

    It works by following simple rules.

    1. Adjustments to data structure must carefully avoid readers potentially 
    reading invalid memory or topologically different intermediate states

    2. Pointers to shared data should be considered invalid after leaving an
    RCU critical section or following a context switch

    If these rules are followed it is possible to adjust data inside
    the structure through write operations at the same time as reads. These
    adjustments are made in a way by leveraging the above rules. For example,
    a writer is able to wait until all readers that have a reference to the data
    structure with version 1 have undergone a context switch, at this point it
    is safe for the writer to make its adjustments using rcu primitives and
    publish version 2 that will be visible to new readers. If this write
    operation was one that resulted in data that is no longer in version 2,
    the writer knows that it can safely free said data as no reader currently
    has a right to read that data any longer.

    Where this can go wrong is if a developer does not follow the rules. If
    a reader obtains a reference to some data, undergoes a context switch, and
    then tries to use that same reference afterward, it is now in a nondeterministic
    race condition and can potentailly have its data freed out from underneath it.

    I have written this table so that it closely matches the current API of the
    Click HashTable. These methods have been implemented to use RCU primitives
    to make it threadsafe. That being said there are some methods that should
    be used with caution. unsafe_get() and unsafe_get_entry_ptr() are two functions
    that return a reference or a pointer to an object in memory. If the user
    of this hashtable doesn't utilize rcu primitives to signify an rcu critical
    seciton while getting the data and all while using the reference, then it
    is an illegal usage and will result in race conditions. These methdods should
    be avoided and I have considerd removing them entirely. Instead an iterator
    should be used as there is rcu critical section declarations built into
    the lifetime of the iterator.

    Even with the increased safety that the iterator brings, it is possible 
    to abuse the structure, copying and holding onto a pointer after undergoing
    a context switch is completely doable from the developers side and no
    compiler will give a warning about it. If the user of the data structure
    understands the risks and implications of rcu, then it's safe to use.

    -- Structure Characteristics --
	This data structure is made up of a member of type struct h_table* named
	table. Inside of this struct is all of the data that makes up the hash
	table. It is stored in this way so that changes can be published atomically
	so as to follow rcu protocols. This pointer can be swapped out with another
	to prevent intermediate states to become visible to readers during a write
	operation.

	This struct has a bitmask which encodes the number of buckets (zero indexed).
	The advantage of having this bitmask is that it allows for complete avoidance
	of modular arithmetic when inserting hashed items into the table. The caveat
	of this method is that it requires to total number of buckets to be powers of
	2. That being said, the algorithms for resizing also require that the number
	of buckets be power of 2 as well and thus the bitmask is not the limiting
	factor for why this structure requires a size equal to a power of 2.

TODO
    **Issues**
    -Change kcalloc methods to utilize the click allocation macros
    -The default value should probably be pushed inside of the h_table struct
        but is okay for now since it never changes.
    -Make safe usage examples
    -Determine if Swap is a good idea or not

*/
template <class K, class V>
class HashTable_RCU {

    /*Struct for hash table data structure
    * Follows open chaining
    * mask is a bitmask to avoid modular arithmetic - always (num_buckets)-1.
    * requires number of buckets to be power of 2
    */
    struct h_table {
        size_t mask;
        struct hlist_head* buckets;
        Vector<K> *keys;

        h_table()
        {
            mask = 0;
            // buckets = (hlist_head*) CLICK_LALLOC(sizeof(hlist_head));
            buckets = (hlist_head*) kcalloc(1, sizeof(hlist_head),GFP_KERNEL);
            keys = new Vector<K>();
        }

        h_table(size_t nbuckets)
        {
            if (nbuckets < 1)
                nbuckets = 1;
            mask = nbuckets-1;
            // buckets = (hlist_head*) CLICK_LALLOC(nbuckets * sizeof(hlist_head));
            buckets = (hlist_head*) kcalloc(nbuckets, sizeof(hlist_head),GFP_KERNEL);
            keys = new Vector<K>();
        }

        h_table(h_table *ot)
        {
            mask = ot->mask;
            // buckets = (hlist_head*) CLICK_LALLOC((ot->mask+1)*sizeof(hlist_head));
            buckets = (hlist_head*) kcalloc((ot->mask+1),sizeof(hlist_head),GFP_KERNEL);
            keys = new Vector<K>(*ot->keys);

            rcu_read_lock();
            size_t i;
            for (i = 0; i <= mask; i++) { //for each bucket in old table
                struct hash_entry *entry;

                hlist_for_each_entry(entry, &ot->buckets[i],node) { //for each item in the bucket
                    struct hash_entry *entry2 = new hash_entry(entry->key,entry->value);

                    size_t hashed_key = hashcode(entry2->key);
                    hlist_add_head_rcu(&entry2->node, &buckets[hashed_key & mask]);
                }
            }
            rcu_read_unlock();
        }

        /*@brief Destructor for h_table structure. Does not clear entries!*/
        ~h_table()
        {
            if (buckets)
                kfree(buckets);
                // CLICK_LFREE(buckets,(mask+1)*sizeof(hlist_head));

            delete (keys);
        }

        /*@brief goes through the table and frees all of the entries in it
        * NOT for use if the entries have been shallow copied into another table
        */
        void free_table_entries()
        {
            if (buckets)
            {
                for (size_t i = mask; i>0; i--) {
                    hash_entry *entry;
                    hlist_node *next;
                    hlist_for_each_entry_safe(entry,next,&buckets[i],node){
                        delete(entry);
                    }
                }
                hash_entry *entry;
                hlist_node *next;
                hlist_for_each_entry_safe(entry,next,&buckets[0],node){
                    delete(entry);
                }
            }
        }
    };

public:
    //Struct for intrusive hash table entry
    struct hash_entry {
        struct hlist_node node;
        K key;
        V value;

        hash_entry() : node(), key(), value()
        {
        }

        hash_entry(const K key, const V value)
        {
            this->node = hlist_node();
            this->key = key;
            this->value = value;
        }
    };

    typedef HashTable_RCU_iterator<K,V> iterator;
    typedef HashTable_RCU_const_iterator<K,V> const_iterator;


    HashTable_RCU(): _default_value()
    {
        _table = new h_table();
    }

    HashTable_RCU(size_t nbuckets): _default_value()
    {
        _table = new h_table(nbuckets);
    }

    HashTable_RCU(const HashTable_RCU<K,V>& other): _default_value()
    {
        _table = new h_table(other._table);
    }

    ~HashTable_RCU()
    {
        _table->free_table_entries();
        delete (_table);
    }

    /*@brief returns a click Vector of the keys currently in the table */
    Vector<K> get_keys() const;

    /*@brief returns the number of items currently in the table */
    size_t size() const;

    /*@brief returns true if the table has no elements in it */
    bool empty() const;

    /*@brief returns the number of buckets currently in the table */
    size_t bucket_count() const;

    /*@brief returns the length of the bucket specified
     *@param bkt the index of the bucket to return the size of*/
    size_t bucket_size(size_t bkt) const;

    /*@brief returns the hash table's default value.*/
    const V& get_default_value() const;

    /*@brief returns an iterator pointing to the first item in the table */
    iterator begin();
   
    /*@brief returns an iterator pointing to the first item in the table */
    const_iterator begin() const;

    /*@brief returns a null iterator*/
    iterator end();

    /*@brief returns a null iterator*/
    const_iterator end() const;

    /*@brief returns an iterator pointing to the item indicated by the key
     *@param key the key of the item to find
     */
    iterator find(const K &key);

    /*@overload*/
    const_iterator find(const K &key) const;


    /*@brief returns an iterator pointing to the item indicated by the key - inserts if key is not found
     *@param key the key of the item to find
     *@param value the value of the item that is to be created if the key is not found
     */
    iterator find_insert(K &key, V &value);

    /*@brief returns 1 if an item with the key is found, 0 otherwise 
     *@param key the key of the item to find
     */
    size_t count(K &key) const;

    /*@brief inserts an item into the table with a specified key and value 
     *@param key the key of the item to insert
     *@param value the value of the item to insert
     */
    void insert(const K &key, const V &value);

    /*@brief returns a reference to the value referenced by the key
     *@param key the key of the item to be gotten
     *@note It is important to understand that this is not a safe action,
     * it is completely possible to break the rules of RCU using this function
     */
    const V& unsafe_get(const K &key) const;

    /*@brief returns a reference to the value referenced by the key
     *@param key the key of the item to be gotten
     *@param reference to an integer that will equal the total number of iterations to find the correct item
     *@note It is important to understand that this is not a safe action,
     * it is completely possible to break the rules of RCU using this function
     */
    const V& unsafe_get(const K &key, int &lookups) const;

    /*@brief sets the item indicated by the key to the specified value - inserts if not currently in table
     *@param key the key of the item to set/insert
     *@param value the value of the item to be set to
     */
    void set(const K &key, const V &value);

    /*@brief erases the item pointed to by the specified iterator
     *@param it the iterator that is currently pointing to the item that is to be removed from the table
     */
    void erase(iterator it);

    /*@brief erases the item pointed to by the specified iterator
     *@param it the iterator that is currently pointing to the item that is to be removed from the table
     */
    void erase(K &key);

    /*@brief erases all of the items in the table */
    void clear();

    /*@brief swaps the contents of this hash table and x
     *@param x The HashTable to be swapped with this 
     */
    void swap(HashTable_RCU<K,V>& x);

    /*@brief Assign this hashtable's contents to a copy of x
     *@param x Hashtable to be copied
     */
    HashTable_RCU<K,V> &operator=(const HashTable_RCU<K,V> &x);

    /*@brief resizes the table to the specified number of buckets, if new size is same as old: does nothing
     *@param new_num_buckets the new total number of buckets to be in the resized table
     */
    void resize(size_t new_num_buckets);

    /*@brief shrinks the table to the specified number of buckets, will perform even if new size == old size
     *@param new_num_buckets the new total number of buckets to be in the resized table - MUST BE POWER OF 2
     */
    void shrink(size_t new_num_buckets);

    /*@brief grows the table to the specified number of buckets through complete rehash, will perform even if new size == old size
     *@param new_num_buckets the new total number of buckets to be in the resized table
     */
    void grow_long(size_t new_num_buckets);

    /*@brief grows the table to the specified number of buckets, will perform even if new size == old size
     *@param new_num_buckets the new total number of buckets to be in the resized table - MUST BE POWER OF 2
     */
    void grow(size_t new_num_buckets);

    friend class HashTable_RCU_iterator<K,V>;
    friend class HashTable_RCU_const_iterator<K,V>;

private:
    /*@brief advance to the next last item in the list
    *@param old_last_next pointer to the pointer of the next node that is starting from
    */
    static struct hlist_node **hlist_advance_last_next(struct hlist_node **old_last_next)
    {
        struct hlist_head h = {.first = *old_last_next};
        struct hlist_node *node;

        hlist_for_each(node, &h){
            if(!node->next)
                return &node->next;
        }
        return old_last_next;
    }

    /*@brief returns a pointer to the entry referenced by the key
     *@param key the key of the item to be gotten
     *@note It is important to understand that this is not a safe action,
     * it is completely possible to break the rules of RCU using this function
     */
    hash_entry* unsafe_get_entry_ptr(const K &key);
   
    /*@overload*/
    const hash_entry* unsafe_get_entry_ptr(const K &key) const;


    struct h_table *_table;
	V _default_value;
};

template<class K, class V>
Vector<K> HashTable_RCU<K,V>::get_keys() const
{
    // return keys;
    rcu_read_lock();
    Vector<K> keys = *(rcu_dereference(_table))->keys;
    rcu_read_unlock();
    return (keys);
}

template<class K, class V>
size_t HashTable_RCU<K,V>::size() const
{
    rcu_read_lock();
    size_t ret = (rcu_dereference(_table))->keys->size();
    rcu_read_unlock();
    return ret;
}

template<class K, class V>
bool HashTable_RCU<K,V>::empty() const
{
    return (size() == 0);
}

template<class K, class V>
size_t HashTable_RCU<K,V>::bucket_count() const
{
    rcu_read_lock(); //enter rcu crit sec
    size_t ret = rcu_dereference(_table)->mask+1;
    rcu_read_unlock(); //exit rcu crit sec

    return ret;
}

template<class K, class V>
size_t HashTable_RCU<K,V>::bucket_size(size_t bkt) const
{
    rcu_read_lock(); //enter rcu crit sec
    size_t len = 0;
    struct hlist_node *node = rcu_dereference(_table)->buckets[bkt].first;
    while (node) {//while node is defined, increase the counter and set node to the next one
        len++;
        node = node->next;
    }
    rcu_read_unlock(); //exit rcu crit sec
    return len;
}

template<class K, class V>
const V& HashTable_RCU<K,V>::get_default_value() const
{
    return _default_value;
}

template<class K, class V>
typename HashTable_RCU<K,V>::const_iterator HashTable_RCU<K,V>::begin() const
{
    rcu_read_lock();
    auto it = iterator();

    hlist_node * elt;
    size_t b = 0;
    for (; b < bucket_count(); b++) {//need to go through the buckets to find the first node that is defined
        elt = rcu_dereference(_table)->buckets[b].first;
        if(elt)
            break;
    }
    it = const_iterator(this,b,elt);
    rcu_read_unlock();
    return it;
}

template<class K, class V>
typename HashTable_RCU<K,V>::iterator HashTable_RCU<K,V>::begin()
{
    rcu_read_lock();
    auto it = iterator();

    hlist_node * elt;
    size_t b = 0;
    for (; b < bucket_count(); b++) { //need to go through the buckets to find the first node that is defined
        elt = rcu_dereference(_table)->buckets[b].first;
        if (elt)
            break;
    }
    it = iterator(this,b,elt);
    rcu_read_unlock();
    return it;
}

template<class K, class V>
typename HashTable_RCU<K,V>::const_iterator HashTable_RCU<K,V>::end() const
{
    return const_iterator(this, bucket_count(), 0); //return an iterator with null
}

template<class K, class V>
typename HashTable_RCU<K,V>::iterator HashTable_RCU<K,V>::end()
{
    return iterator(this, bucket_count(), 0); //return an iterator with null
}

template<class K, class V>
typename HashTable_RCU<K,V>::iterator HashTable_RCU<K,V>::find(const K &key)
{
    size_t hashed_key = hashcode(key);
    return iterator(this, hashed_key & _table->mask, (hlist_node*) unsafe_get_entry_ptr(key)); //return iterator pointing to the item specified
}

template<class K, class V>
typename HashTable_RCU<K,V>::const_iterator HashTable_RCU<K,V>::find(const K &key) const
{
    size_t hashed_key = hashcode(key);
    return const_iterator(this, hashed_key & _table->mask, (hlist_node*) unsafe_get_entry_ptr(key)); //return iterator pointing to the item specified
}

template<class K, class V>
typename HashTable_RCU<K,V>::iterator HashTable_RCU<K,V>::find_insert(K &key, V &value)
{
    iterator it = find(key); //find
    if(it.live()) //if it is a non null iterator
        return it;
    else { 
    	//null iterator, meaning it wasn't found
        insert(key, value); //time to insert it then
        return find(key); //return an iterator pointing to the new item
    }
}

template<class K, class V>
size_t HashTable_RCU<K,V>::count(K &key) const
{
    return find(key).live(); //count returns only 0 or 1, so it just needs to return whether or not it is found
};

template<class K, class V>
void HashTable_RCU<K,V>::insert(const K &key, const V &value)
{
    auto itr = find(key);
	if (!(itr.live())) { //then it wasn't in the table
		_table->keys->push_front(key);

        hash_entry *entry = new hash_entry(key,value);

        size_t hashed_key = hashcode(key);
        hlist_add_head_rcu(&entry->node, &_table->buckets[hashed_key & _table->mask]); //add to the head of the bucket
    }
    else {
    	//it was already in the table, just need to change the value
        set(key,value);
    }
};

template<class K, class V>
const V& HashTable_RCU<K,V>::unsafe_get(const K &key) const //TODO separate these two overlaods into separately named methods
{
    int dummyLookups = 0; //makes the compiler happy
    return unsafe_get(key, dummyLookups);
};

template<class K, class V>
const V& HashTable_RCU<K,V>::unsafe_get(const K &key, int &lookups) const
{
    rcu_read_lock(); //enter rcu crit
    struct h_table *tab = rcu_dereference(_table);
    size_t hashed_key = hashcode(key);
    struct hash_entry *entry;
    struct hlist_node *node;

    lookups = 0;
    hlist_for_each_entry_linuxV30(entry,node,&tab->buckets[hashed_key & tab->mask], node) {
        if (entry->key == key) {
        	//if true, it was found!
            V *retVal = &(entry->value); //get the pointer to the value
            rcu_read_unlock(); //exit rcu crit
            return *retVal;
        }
        lookups++;
    }

    rcu_read_unlock(); //exit rcu crit
    return _default_value; //it wasn't found
};

template<class K, class V>
typename HashTable_RCU<K,V>::hash_entry* HashTable_RCU<K,V>::unsafe_get_entry_ptr(const K &key)
{
    rcu_read_lock(); //enter rcu crit
    struct h_table *tab = rcu_dereference(_table);
    size_t hashed_key = hashcode(key);

    struct hash_entry *entry;
    struct hlist_node *node;

    hlist_for_each_entry_linuxV30(entry, node, &tab->buckets[hashed_key & tab->mask], node) {
        if (entry->key == key) {
        	 //if true, it was found!
            rcu_read_unlock(); //exit rcu crit
            return entry;
        }
    }

    rcu_read_unlock(); //exit rcu crit
    return (hash_entry*)NULL; //it wasn't found
};

template<class K, class V>
const typename HashTable_RCU<K,V>::hash_entry* HashTable_RCU<K,V>::unsafe_get_entry_ptr(const K &key) const
{
    rcu_read_lock(); //enter rcu crit
    struct h_table *tab = rcu_dereference(_table);
    size_t hashed_key = hashcode(key);

    struct hash_entry *entry;
    struct hlist_node *node;

    hlist_for_each_entry_linuxV30(entry, node, &tab->buckets[hashed_key & tab->mask], node) {
        if (entry->key == key) { 
        	//if true, it was found!
            rcu_read_unlock(); //exit rcu crit
            return entry;
        }
    }

    rcu_read_unlock(); //exit rcu crit
    return (hash_entry*)NULL; //it wasn't found
};

template<class K, class V>
void HashTable_RCU<K,V>::set(const K &key, const V &value)
{
    rcu_read_lock(); //enter rcu crit

    hash_entry *entry = (hash_entry*) unsafe_get_entry_ptr(key); //get pointer to the entry
    if (entry) {
    	//key was in the table
        entry->value = value; //set the value
        rcu_read_unlock(); //exit rcu crit
    } else {
    	//key wasn't in the table
        rcu_read_unlock(); //exit rcu crit before entering the insert function
        insert(key,value);
    }
};


template<class K, class V>
void HashTable_RCU<K,V>::erase(HashTable_RCU::iterator it)
{
    if (it.live())
    {
        //follows the hlist_del from linux kernel
        struct hlist_node *next = (&it.get()->node)->next;
        struct hlist_node **pprev = (&it.get()->node)->pprev;

        rcu_assign_pointer(*pprev, next);
        if (next)
            next->pprev = pprev;

        //remove it from the keys
        auto vectit = _table->keys->begin();
        for(; vectit != _table->keys->end(); vectit++) {
            if ((*vectit) == it.get()->key)
                break;
        }
        _table->keys->erase(vectit);

        delete(it.get());
    }
}


template<class K, class V>
void HashTable_RCU<K,V>::erase(K &key)
{
    //follows the hlist_del from linux kernel
    hash_entry *he = unsafe_get_entry_ptr(key);
    if (he) {
        struct hlist_node *next = (&he->node)->next;
        struct hlist_node **pprev = (&he->node)->pprev;

        rcu_assign_pointer(*pprev, next);
        if(next)
            next->pprev = pprev;

        //remove it from the keys
        auto vectit = _table->keys->begin();
        for (; vectit != _table->keys->end(); vectit++) {
            if ((*vectit) == key)
                break;
        }
        _table->keys->erase(vectit);

        delete (he);
    }
};

template<class K, class V>
void HashTable_RCU<K,V>::clear()
{
    //allocate a new empty table
    h_table *temp_table = new h_table(bucket_count());

    h_table *old_table = _table;

    rcu_assign_pointer(_table, temp_table); //rcu assign the new empty table to be in the pointer of this's table

    synchronize_rcu(); //wait for readers before reclaiming old table

    old_table->free_table_entries();
    delete (old_table);
}

//TODO I do not like this function - too easy to read incorrect data
template<class K, class V>
void HashTable_RCU<K,V>::swap(HashTable_RCU<K,V>& x)
{
    // need a table for the intermediate to swap
    h_table *intermediate_table; 

    intermediate_table = _table;

    rcu_assign_pointer(_table, x._table);
    rcu_assign_pointer(x._table, intermediate_table);

    synchronize_rcu();
}

template<class K, class V>
HashTable_RCU<K,V>& HashTable_RCU<K,V>::operator=(const HashTable_RCU<K,V> &x)
{
    h_table *old_table =  _table;

    h_table *new_table;

    new_table = new h_table(x._table);

    rcu_assign_pointer(_table, new_table);

    synchronize_rcu();

    old_table->free_table_entries();
    delete (old_table);

    return *this;
}


template<class K, class V>
void HashTable_RCU<K,V>::resize(size_t new_num_buckets)
{
    size_t mask2 = new_num_buckets -1;
    if(mask2 < _table->mask) //shrink
        shrink(new_num_buckets);
    else if(mask2 > _table->mask) //grow
        grow(new_num_buckets);
};

template<class K, class V>
void HashTable_RCU<K,V>::shrink(size_t new_num_buckets)
{
    size_t mask2 = new_num_buckets - 1;
    size_t i,j;

//    struct hlist_head* new_buckets;
    //Concatenate all buckets which contain entries that hash to the same bucket in the smaller table
    for (i = 0; i <= mask2; i++) {
        struct hlist_node **last_next = &_table->buckets[i].first; //get address of the pointer of the first node in the ith bucket
        for (j = i+new_num_buckets; j <= _table->mask; j+= new_num_buckets) {
            if (hlist_empty(&_table->buckets[j]))
                continue; //if empty, nothing to concat

            last_next = hlist_advance_last_next(last_next); //now loking at the next pointer in the last node in the list
            *last_next = _table->buckets[j].first; //the value at that pointer is now the first of the jth bucket
            (*last_next)->pprev = last_next;
        }
    }

    synchronize_rcu(); //wait for readers
    _table->mask = mask2;
    synchronize_rcu(); //wait for readers

    _table->buckets = (hlist_head*) krealloc(_table->buckets, (mask2+1)*sizeof(hlist_head),GFP_KERNEL); //reclaim the old bucket space
}

template<class K, class V>
void HashTable_RCU<K,V>::grow_long(size_t new_num_buckets)
{
	struct h_table *temp_table, *old_table;

    temp_table = new h_table(new_num_buckets);
    temp_table->keys = new Vector<K>(*_table->keys);

	size_t i;
	for (i = 0; i <= _table->mask; i++) { //for each bucket in old table
        struct hash_entry *entry;

        hlist_for_each_entry(entry, &_table->buckets[i],node) { //for each item in the bucket 
            struct hash_entry *entry2 = new hash_entry(entry->key, entry->value);

		    size_t hashed_key = hashcode(entry2->key);
		    hlist_add_head_rcu(&entry2->node, &temp_table->buckets[hashed_key & temp_table->mask]);
        }
    }

    old_table = _table;
    rcu_assign_pointer(_table, temp_table); //rcu assign new table to be this's table

    synchronize_rcu(); //wait for readers before reclamation

    old_table->free_table_entries();
    delete (old_table);
}

template<class K, class V>
void HashTable_RCU<K,V>::grow(size_t new_num_buckets)
{
    size_t mask2 = new_num_buckets-1;
    size_t i;

    struct h_table *temp_table, *old_table;
    bool moved_one;

    temp_table = new h_table(new_num_buckets);
    temp_table->keys = new Vector<K>(*_table->keys);

    //for each new bucket, search corresponding old bucket for first entry that hashes to the new bucket and link the new bucket to that entry.
    //since all the entries which will end up in the new bucket appear in the same old bucket, this constructs
    //and entirely valid new hashtbable, but with multiple buckets zipped together in a single imprecise chain
    for (i = 0; i <= mask2; i++) {
        struct hash_entry *entry;
        struct hlist_node *node;
        hlist_for_each_entry_linuxV30(entry, node, &_table->buckets[i & _table->mask], node) {
            size_t hashed_key = hashcode(entry->key);
            if ((hashed_key & mask2) == i) {
                temp_table->buckets[i].first = node;
                node->pprev = &temp_table->buckets[i].first;
                break;
            }
        }
    }

    old_table = _table;
    rcu_assign_pointer(_table, temp_table); //publish the new table pointer, lookups now traverse the new table but don't benefit
    //from any efficiency until later steps unzip the buckets
    synchronize_rcu(); //wait for readers

    //for each bucket in the old table, adv old bucket poitner one or more times until it reaches a node that doesn't hash to the
    //same bucket as the previous node "p".
    //find subsequent node which does hash to same bucket as node p or null if no such node exists
    //set p's next poiter to that subsequent nodes pointer, bypassing the nodes which don't hash to p's bucket
    do {
        moved_one = false;
        for (i = 0; i <= old_table->mask; i++) {
            struct hash_entry *entry_prev, *entry;
            struct hlist_node *node;
            if(hlist_empty(&old_table->buckets[i]))
                continue;

            entry_prev = hlist_entry(old_table->buckets[i].first, struct hash_entry,node);
            hlist_for_each_entry_linuxV30(entry, node, &old_table->buckets[i], node) {
                size_t hashed_key = hashcode(entry->key);
                size_t hashed_key_prev = hashcode(entry_prev->key);
                if((hashed_key & mask2) != (hashed_key_prev & mask2))
                    break;
                entry_prev = entry;
            }
            old_table->buckets[i].first = node;
            if(!node)
                continue;
            moved_one = true;
            hlist_for_each_entry_linuxV30(entry, node, &old_table->buckets[i],node) {
                size_t hashed_key = hashcode(entry->key);
                size_t hashed_key_prev = hashcode(entry_prev->key);
                if((hashed_key & mask2) == (hashed_key_prev & mask2))
                    break;
            }
            entry_prev->node.next = node;
            if(node)
                node->pprev = &entry_prev->node.next;
        }
        synchronize_rcu(); //wait for readers
    } while (moved_one);

    delete (old_table);
};

/** @class HashTable_RCU_const_iterator
    @brief Iterator for Hash Table Template with Lockless thread safety using RCU

    This iterator is a simple iterator that ensures that you can easily iterate over
    all of the items in the referenced hash table, despite the fact that the items
    are not necessarily contiguous in the structure

    **Issues**
    -No comparator for iterators, while(it < ht.end()) will not work in current state
        workaround is to use while(it.live()) instead
*/
template <class K, class V>
class HashTable_RCU_const_iterator {
public:

	typedef HashTable_RCU<K,V> ht_rcu_t; //the hlist_entry macro can't accept a template

    HashTable_RCU_const_iterator()
    {
        rcu_read_lock();
    }
    ~HashTable_RCU_const_iterator()
    {
        rcu_read_unlock();
    }

    HashTable_RCU_const_iterator(const HashTable_RCU_const_iterator<K,V> &oit)
    {
        rcu_read_lock();
        _bucket = oit._bucket;
        _element = oit._element;
        _hc = oit._hc;
    }

    /*@brief get the entry that is pointed to by the iterator */
    typename HashTable_RCU<K,V>::hash_entry* get() const
    {
        if(_element)
        	return hlist_entry(_element, typename ht_rcu_t::hash_entry, node);
        else
            return NULL;
    }

    /*@brief get the entry that is pointed to by the iterator */
    typename HashTable_RCU<K,V>::hash_entry* operator->() const
    {
        return get();
    }

    /*@brief get reference to entry that is pointed to by the iterator */
    typename HashTable_RCU<K,V>::hash_entry& operator*() const
    {
        return *get();
    }

    /*@brief get reference to key of entry that is pointed to by the iterator */
    K& key() const
    {
        return get()->key;
    }

    /*@brief returns true iff the element pointed to by the iterator is defined */
    bool live() const
    {
        return (bool) (_element);
    }

    /*@brief returns true iff the element pointed to by the iterator is defined */
    inline explicit operator bool() const
    {
        return live();
    }

    /*@brief advances iterator to next entry in the table */
    void operator++(int)
    {
        size_t total_buckets = _hc->bucket_count();
        if (_element->next) {
            _element = _element->next;
        } else {
            while (_bucket < total_buckets) {
                _bucket++;
                _element = _hc->_table->buckets[_bucket].first;
                if(_element)
                    return;
            }
            _element = 0;
        }  
    }

    /*@brief advances iterator to next entry in the table */
    void operator++()
    {
        size_t total_buckets = _hc->bucket_count();
        if (_element->next) {
            _element = _element->next;
        } else {
            while (_bucket < total_buckets) {
                _bucket++;
                _element = _hc->_table->buckets[_bucket].first;
                if(_element)
                    return;
            }
            _element = 0;
        }
    }

    HashTable_RCU_const_iterator<K,V>& operator=(const HashTable_RCU_const_iterator<K,V> &oit)
    {
        rcu_read_lock();
        _bucket = oit._bucket;
        _element = oit._element;
        _hc = oit._hc;
        return *this;
    }

private:
    size_t _bucket;
    hlist_node *_element;
    const HashTable_RCU<K,V> *_hc;

    HashTable_RCU_const_iterator(const HashTable_RCU<K,V> *hc, size_t b, hlist_node * element)
    {
        rcu_read_lock();
        _element = element;
        _bucket = b;
        _hc = rcu_dereference(hc);
    }

    friend class HashTable_RCU<K,V>;
    friend class HashTable_RCU_iterator<K,V>;

};

/** @class HashTable_RCU_iterator
    @brief Iterator for Hash Table Template with Lockless thread safety using RCU

    This iterator is a simple iterator that ensures that you can easily iterate over
    all of the items in the referenced hash table, despite the fact that the items
    are not necessarily contiguous in the structure
*/
template <class K, class V>
class HashTable_RCU_iterator: public HashTable_RCU_const_iterator<K,V> {
public:

    typedef HashTable_RCU_const_iterator<K,V> inherited;

    HashTable_RCU_iterator()
    {
        rcu_read_lock();
    }

    ~HashTable_RCU_iterator()
    {
        rcu_read_unlock();
    }

    
private:

    HashTable_RCU_iterator(HashTable_RCU<K,V> *hc, size_t b, hlist_node * element) : inherited(hc,b,element){
    }

    HashTable_RCU_iterator(const typename HashTable_RCU<K,V>::const_iterator &i) : inherited(i){
    }

    friend class HashTable_RCU<K,V>;

};


template <class K, class V>
bool hashtable_rcus_differ(const HashTable_RCU<K,V> &a, const HashTable_RCU<K,V> &b)
{
    if (a.size() != b.size())
        return true;

    for (auto a_itr = a.begin(); a_itr.live(); a_itr++) {
        auto b_itr = b.find(a_itr.key());
        if(!(b_itr.live()) || *a_itr != *b_itr)
            return true;
    }

    return false;
}

template <class K, class V>
inline bool operator!=(const HashTable_RCU<K,V> &a, const HashTable_RCU<K,V> &b)
{
    return &a != &b && hashtable_rcus_differ(a,b);
}

template <class K, class V>
inline bool operator==(const HashTable_RCU<K,V> &a, const HashTable_RCU<K,V> &b)
{
    return !(a != b);
}


CLICK_ENDDECLS
#endif