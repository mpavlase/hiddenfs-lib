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
    private:
        typedef std::map<hash_t, std::string> table_t;

        table_t table;
    public:
        hashTable();
        hashTable(const hashTable& orig);

        /**
         * Vyhledání v tabulce (mapování hash -> název souboru)
         * @param hash hash řetězec souboru
         * @param filename skrze tento parametr metoda vrací umístění skutečného souboru
         */
        void find(hash_t hash, std::string* filename) const;

        /**
         * Vloží nový záznam do hash tabulky
         * @param hash vypočítaný hash z obsahu souboru
         * @param filename umístění skutečného souboru
         */
        void add(hash_t hash, std::string filename);

        /**
         * User friendly zobrazení obsahu tabulky - používat pouze pro ladění!
         */
        void print() const;

        /**
         * Remove all items from table
         */
        void clear();
        virtual ~hashTable();

        typedef table_t::const_iterator table_t_constiterator;

        table_t_constiterator begin() const {
            return this->table.begin();
        }

        table_t_constiterator end() const {
            return this->table.end();
        }

        unsigned int size() const {
            return this->table.size();
        }
    };
};

#endif	/* HASHTABLE_H */

