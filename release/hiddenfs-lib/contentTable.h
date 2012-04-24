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

#include "common.h"
//#include "hiddenfs-lib.h"
#include "types.h"
#include "exceptions.h"
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

            /** množina obsazených bloků \
             * mapování: číslo fragmentu -> pole bloků,
             * které uchovávají obsah fragmentu */
            std::map<fragment_t, std::vector<vBlock*> > content;
        } tableItem;

        /** datový typ pro mapování pomocných dat k souboru určeným inode */
        typedef std::map<inode_t, tableItem> table_t;

        contentTable();
        //contentTable(const contentTable& orig);
        virtual ~contentTable();

        /**
         * Provede dump hlavní tabulky do podoby řetězce
         * @param ouput metoda naplní seznam entitami pro pozdější uložení
         */
        void serialize(chainList_t* output);

        void deserialize(chainList_t input);

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

        void addContent(inode_t inode, vBlock* block) {
            if(block->used) {
                try {
                    this->newEmptyContent(inode);
                } catch (ExceptionInodeExists) { }

                // zde to padá, protože přistupuju do nezinicializvaného pole!
                // špatně deserializovaná složka .block v vBlock
                //this->table[inode].content[block->fragment].push_back(block);
                //tableItem ti = this->table[inode];
                //std::cout << "ti.content.size() = " << this->table[inode].content.size() << "\n";
                this->table[inode].content[block->fragment].push_back(block);
                /*
                std::cout << "ti.content.size() = " << this->table[inode].content.size() << "\n";
                for(std::map<fragment_t, std::vector<vBlock*> >::iterator i = this->table[inode].content.begin(); i != this->table[inode].content.end(); i++) {
                    std::cout << "fragment = " << i->first << ", ";
                    for(std::vector<vBlock*>::iterator j = i->second.begin(); j != i->second.end(); j++) {
                        std::cout << print_vBlock(*j);
                    }
                    std::cout << "\n";
                }
                std::cout << "............................\n";
                this->print();
                */
            } else {
                this->table[inode].reserved.insert(block);
                this->table[inode].reservedBytes += block->length;
            }
        }

        void getReservedBlocks(inode_t inode, std::vector<vBlock*>* reserved);
    private:
        /** map[inode_t] = tableItem */
        table_t table;
    };
}

#endif	/* CONTENTTABLE_H */

