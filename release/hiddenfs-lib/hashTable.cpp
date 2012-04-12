/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 23. březen 2012
 */

#include "exceptions.h"
#include "hashTable.h"

namespace HiddenFS {
    hashTable::hashTable() {
    }

    hashTable::hashTable(const hashTable& orig) {
    }

    hashTable::~hashTable() {
    }

    void hashTable::find(T_HASH hash, std::string* filename) {
        std::map<T_HASH, std::string>::iterator it = this->table.find(hash);
        if(it != this->table.end()) {
            *filename = it->second;
        } else {
            std::stringstream ss;
            ss << hash;
            throw ExceptionFileNotFound(ss.str());
        }
    }

    void hashTable::add(T_HASH hash, std::string filename) {
        this->table[hash] = filename;
    }

    void hashTable::clear() {
        this->table.clear();
    }
}