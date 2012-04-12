/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 23. březen 2012
 */

#include "hashTable.h"
#include "exceptions.h"

hashTable::hashTable() {
}

hashTable::hashTable(const hashTable& orig) {
}

hashTable::~hashTable() {
}

void hashTable::find(T_HASH hash, string* filename) {
    map<T_HASH, string>::iterator it = this->table.find(hash);
    if(it != this->table.end()) {
        *filename = it->second;
    } else {
        stringstream ss;
        ss << hash;
        throw ExceptionFileNotFound(ss.str());
    }
}

void hashTable::add(T_HASH hash, string filename) {
    this->table[hash] = filename;
}

void hashTable::clear() {
    this->table.clear();
}