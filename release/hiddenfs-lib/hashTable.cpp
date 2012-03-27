/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 23. březen 2012
 */

#include "hashTable.h"

hashTable::hashTable() {
}

hashTable::hashTable(const hashTable& orig) {
}

hashTable::~hashTable() {
}

string hashTable::find(T_HASH hash) {
    return this->table[hash];
}

void hashTable::add(T_HASH hash, string filename) {
    this->table[hash] = filename;
}

void hashTable::clear() {
    this->table.clear();
}