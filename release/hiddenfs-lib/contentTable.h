/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#ifndef CONTENTTABLE_H
#define	CONTENTTABLE_H

#include <map>
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
        set<vBlock*> reserved;
        map<int, vector<vBlock*> > content;
    } tableItem;

    typedef map<inode_t, tableItem> table_t;

    contentTable(structTable* ST);
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
     * Přečte kompletní obsah virtuálního souboru ze všech fragmentů
     * @param inode inode souboru
     * @param buffer obsah souboru
     * @param length délka souboru v bytech
     */
    void getContent(inode_t inode, char* buffer, size_t* length);

    /**
     * Inicializuje vnitřní struktury pro ukložení fragmentů
     * Je nutné volat ještě před jakoukoli manipulací s
     * @param inode inode souboru
     */
    void newContent(inode_t inode) throw (ExceptionInodeExists);

    //void

    void getReservedBlocks(inode_t inode, vector<vBlock*>* reserved);
private:
    /** map[inode_t] = tableItem */
    table_t table;
    structTable* ST;
};

#endif	/* CONTENTTABLE_H */

