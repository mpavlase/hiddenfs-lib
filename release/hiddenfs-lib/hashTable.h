/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 23. březen 2012
 */

#ifndef HASHTABLE_H
#define	HASHTABLE_H

#include <map>
#include <set>
#include <string>

#include "types.h"

namespace HiddenFS {
    /**
     * Obousměrné mapování hash a adresy ke skutečnému souboru
     */
    class hashTable {
    public:
        typedef struct {
            std::string filename;
            context_t* context;
        } tableItem;

        typedef std::map<hash_ascii_t, tableItem> table_t;

        typedef table_t::const_iterator table_t_constiterator;
        hash_ascii_t lastIndexed;

        /** Příznak pro identifikaci prvního indexování. True = indexování již proběhlo alespoň 1x */
        bool firstIndexing;

        hashTable();
        hashTable(const hashTable& orig);

        /** Seznam hash, které jsou zcela neobsazené (neobsahuje žádné obsazené bloky)
         * @todo Jak najít další hash, které nemá doposud vytvořený kontext? */
        std::set<hash_ascii_t> auxList_unused;

        /** Seznam hash, které mají obsazen alespoň 1 blok a současně je dostupný alespoň 1 blok */
        std::set<hash_ascii_t> auxList_partlyUsed;

        /**
         * Modifikací pomocných tabulek označí blok jako obsazený. Pro zajištění
         * konzistence metadat je nutné tuto metodu volat těsně po skutečně provedené operaci.
         * @param hash jednoznačná identifikace skutečného souboru
         * @param block číslo bloku, kteý označit jako obsazený
         */
        void auxUse(hash_ascii_t hash, block_number_t block) {
            context_t* context;

            context = this->getContext(hash);

            if(context->avaliableBlocks.size() == 0) {
                this->auxList_partlyUsed.erase(hash);
            } else {
                /// @todo odkomentova!
                this->auxList_partlyUsed.insert(hash);
            }
        };

        /**
         * Modifikací pomocných tabulek označí blok jako volný (tzn. k dalšímu použití).
         * Pro zajištění konzistence metadat je nutné tuto metodu volat těsně po
         * skutečně provedené operaci.
         * @param hash jednoznačná identifikace skutečného souboru
         * @param block číslo bloku, kteý označit jako volný
         */
        void auxFree(hash_ascii_t hash, block_number_t block) {
            context_t* context;

            context = this->getContext(hash);

            if(context->avaliableBlocks.size() == context->maxBlocks) {
                /// @todo odkomentova!
                this->auxList_unused.insert(hash);
                this->auxList_partlyUsed.erase(hash);
            } else {
                /// @todo odkomentova!
                this->auxList_partlyUsed.insert(hash);
            }
        }

        /**
         * Vyhledání v tabulce (mapování hash -> název souboru)
         * @param hash hash řetězec souboru
         * @throw ExceptionFileNotFound pokud soubor ve vnitřní tabulce neexistuje
         * @param filename skrze tento parametr metoda vrací umístění skutečného souboru
         */
        void find(hash_ascii_t hash, std::string* filename) const;

        /**
         * Metoda vrací iterátor prvku podle hash
         * @param hash jednoznačná identifikace hledaného souboru
         * @return konstantní iterátor
         */
        table_t_constiterator find(hash_ascii_t hash) {
            return this->table.find(hash);
        }

        /**
         * Vloží nový záznam do hash tabulky
         * @param hash vypočítaný hash z obsahu souboru
         * @param filename umístění skutečného souboru
         */
        void add(hash_ascii_t hash, std::string filename);

        /**
         * User friendly zobrazení obsahu tabulky - používat pouze pro ladění!
         */
        void print() const;

        /**
         * Přiřadí kontext k souboru určenému hashem)
         * @param context
         */
        void setContext(hash_ascii_t hash, context_t* context) {
            this->table[hash].context = context;

            // zařazení do do aux* pomocných seznamů
            if(context->avaliableBlocks.size() == context->maxBlocks) {
                this->auxList_unused.insert(hash);
            } else if(context->usedBlocks.size() < context->maxBlocks) {
                this->auxList_partlyUsed.insert(hash);
            }
        }

        /**
         * Smaže odkaz na kontext, ale původní ukazatel už neuvolňuje
         * @param hash identifikace souboru
         */
        void clearContext(hash_ascii_t hash) {
            this->setContext(hash, NULL);
        }

        /**
         * Vrací kontext k souboru určeném hashem, pokud existuje
         * @param hash identifikace souboru
         * @throw ExceptionRuntimeError pokud kontext neexistuje
         * @return ukazatel na instanci kontextu k souboru
         */
        context_t* getContext(hash_ascii_t hash) {
            if(this->table[hash].context == NULL) {
                throw ExceptionRuntimeError("Kontext k tomuto souboru nebyl přiřazen.");
            }

            return this->table[hash].context;
        }

        /**
         * Remove all items from table
         */
        void clear();
        virtual ~hashTable();

        table_t_constiterator begin() const {
            return this->table.begin();
        }

        table_t_constiterator end() const {
            return this->table.end();
        }

        unsigned int size() const {
            return this->table.size();
        }


        private:
            table_t table;

    };
};

#endif	/* HASHTABLE_H */

