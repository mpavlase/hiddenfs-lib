/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 23. březen 2012
 */

#include <iostream>

#include "exceptions.h"
#include "hashTable.h"

namespace HiddenFS {
    hashTable::hashTable() {
        this->firstIndexing = true;
    }

    hashTable::hashTable(const hashTable& orig) {
    }

    hashTable::~hashTable() {
    }

    void hashTable::find(hash_t hash, std::string* filename) const {
        std::map<hash_t, tableItem>::const_iterator it = this->table.find(hash);
        if(it != this->table.end()) {
            *filename = it->second.filename;
        } else {
            std::stringstream ss;
            ss << hash;
            throw ExceptionFileNotFound(ss.str());
        }
    }

    void hashTable::add(hash_t hash, std::string filename) {
        std::string findFilename;

        try {
            this->find(hash, &findFilename);

            // pokud se pokusíme přidat vícekrát ten samý soubor, ničemu to nevadí.
            if(findFilename == filename) {
                return;
            }

            /* protože nemusíme mít jistoru, že se budou soubory přidávat do hashTable
             * vždy ve stejném pořadí, smažeme i už dříve přidaný hash patřící jinému souboru */
            this->table.erase(hash);

            throw ExceptionRuntimeError("Soubor již existuje, pro jistotu mažu i dříve přidaný hash.");
        } catch (ExceptionFileNotFound) {
            this->table[hash].filename = filename;
        }
    }

    void hashTable::clear() {
        this->table.clear();
    }

    void hashTable::print() const {
        std::cout << "== hashTable.table (hash -> adresa k fyzickému souboru) ==\n";
        std::cout << "počet záznamů: " << this->table.size() << "\n";

        for(table_t_constiterator it = this->table.begin(); it != this->table.end(); it++) {
            std::cout << it->first << "\t" << it->second.filename << (int) it->second.context << "\n";
        }
    }
}