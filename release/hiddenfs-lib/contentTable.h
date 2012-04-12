/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#ifndef CONTENTTABLE_H
#define	CONTENTTABLE_H

#include "types.h"
#include "exceptions.h"
#include "structTable.h"
#include <vector>
#include <map>
#include <set>

using namespace std;

class contentTable {
public:
    typedef struct {
        /** množina rezervovaných bloků nad rámec bloků v atributu content */
        set<vBlock*> reserved;

        /** součet délek všech rezervovaných bloků */
        size_t reservedBytes;

        /** množina obsazených bloků; mapování: číslo fragmentu -> pole bloků, které uchovávají obsah fragmentu */
        map<int, vector<vBlock*> > content;
    } tableItem;

    typedef map<inode_t, tableItem> table_t;

    contentTable();
    contentTable(const contentTable& orig);
    virtual ~contentTable();

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
    void newEmptyContent(inode_t inode) throw (ExceptionInodeExists);

    //void

    void getReservedBlocks(inode_t inode, vector<vBlock*>* reserved);
private:
    /** map[inode_t] = tableItem */
    table_t table;
};

#endif	/* CONTENTTABLE_H */

