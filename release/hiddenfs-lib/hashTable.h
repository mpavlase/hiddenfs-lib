/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 23. březen 2012
 */

#ifndef HASHTABLE_H
#define	HASHTABLE_H

#include <string>
#include <map>
#include "types.h"

using namespace std;

/**
 * Bidirectional mapping real file path and its uniqe hash string
 */
class hashTable {
public:
    hashTable();
    hashTable(const hashTable& orig);

    /**
     * Mapping hash to real file
     * @param hash hash of file
     * @return path to real file
     */
    string find(T_HASH hash);

    /**
     * Insert new item to hash table
     * @param hash calculated hash
     * @param filename path of real file
     */
    //void add(T_HASH hash, string& filename);
    void add(T_HASH hash, string filename);

    /**
     * Remove all items from table
     */
    void clear();
    virtual ~hashTable();
private:
    map<T_HASH, string> table;
};

#endif	/* HASHTABLE_H */

