/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#ifndef CONTENTTABLE_H
#define	CONTENTTABLE_H

#include <map>
#include <set>
#include <vector>

#include "exceptions.h"
#include "types.h"
#include "structTable.h"

namespace HiddenFS {
    class contentTable {
    public:
        typedef struct {
            /** množina rezervovaných bloků nad rámec bloků ve složce "content"
             * této struktury je pouze pomocným seznamem, resp. jiným
             * pohledem na tabulku content */
            std::set<vBlock*> reserved;

            /** součet délek všech rezervovaných bloků, lze odvodit z průchodu
             * složkou struktury "content" */
            size_t reservedBytes;

            /** množina obsazených bloků; mapování: číslo fragmentu -> pole bloků,
             * které uchovávají obsah fragmentu */
            std::map<int, std::vector<vBlock*> > content;
        } tableItem;

        typedef std::map<inode_t, tableItem> table_t;

        contentTable();
        contentTable(const contentTable& orig);
        virtual ~contentTable();

        /**
         * Provede dump hlavní tabulky do podoby řetězce
         * @param ouput výstupní serializovaný obsah, metoda sama alokuje tento ukazatel
         * @param size délka výsledného serializovaného obsahu
         */
        void serialize(unsigned char** ouput, size_t* size);

        void deserialize(unsigned char* input, size_t size);

        /**
         * user-friendly obsah tabulky
         */
        void print();

        /*
         * Find and iterate through all fragments
         * @param inode file identified by inode
         * @param buffer content of file
         */
        /**
         * Vrací metadata obsahu k souboru (množina fragmentů, rezervované bloky atd.)
         * @param inode inode souboru
         * @param content metadata obsahu souboru
         */
        void getMetadata(inode_t inode, tableItem* content);

        /**
         * Inicializuje vnitřní struktury pro ukložení fragmentů
         * Je nutné volat ještě před jakoukoli manipulací s uskladněným inode
         * @param inode inode souboru
         */
        void newEmptyContent(inode_t inode);

        //void

        void getReservedBlocks(inode_t inode, std::vector<vBlock*>* reserved);
    private:
        /** map[inode_t] = tableItem */
        table_t table;
    };
}

#endif	/* CONTENTTABLE_H */

