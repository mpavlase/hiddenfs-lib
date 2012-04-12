/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 23. březen 2012
 */

#ifndef HASHTABLE_H
#define	HASHTABLE_H

#include <map>
#include <string>

#include "types.h"

namespace HiddenFS {
    /**
     * Bidirectional mapping real file path and its uniqe hash string
     */
    class hashTable {
    public:
        hashTable();
        hashTable(const hashTable& orig);

        /**
         * Vyhledání v tabulce (mapování hash -> název souboru)
         * @param hash hash řetězec souboru
         * @param path umístění skutečného souboru
         */
        void find(T_HASH hash, std::string* filename);

        /**
         * Vloží nový záznam do hash tabulky
         * @param hash vypočítaný hash z obsahu souboru
         * @param filename umístění skutečného souboru
         */
        void add(T_HASH hash, std::string filename);

        /**
         * Remove all items from table
         */
        void clear();
        virtual ~hashTable();
    private:
        std::map<T_HASH, std::string> table;
    };
};

#endif	/* HASHTABLE_H */

