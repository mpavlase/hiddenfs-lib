/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#ifndef STRUCTTABLE_H
#define	STRUCTTABLE_H

#include <string>
#include <map>
#include <set>
#include <vector>

using namespace std;

#include "types.h"
#include "exceptions.h"


class structTable {
public:
    structTable();
    structTable(const structTable& orig);
    virtual ~structTable();
    static const inode_t FIRST_INODE = 1;

    /**
     * user-friendly obsah tabulek
     */
    void print(void);

    /**
     * Vytvoří nový soubor (přepíše hodnotu inode!)
     * @param vFile očekává se vyplněná struktura (mimo složky inode)
     * @return inode přávě vloženého souboru
     */
    inode_t newFile(vFile*);

    /**
     * Odstraní soubor z tabulek
     * @param inode inode mazaného souboru
     */
    void removeFile(inode_t inode);

    /**
     * Naplní strukturu o souboru, jinak vyhazuje výjimku "FileNotFound"
     * @param filename hledaný název souboru
     * @param parent inode nadřazeného adresáře
     * @param file podrobnosti o souboru
     */
    void findFileByName(string filename, inode_t parent, vFile* file) throw (ExceptionFileNotFound);

    /**
     * Vyhledá soubor na základě inode, jinak vyhazuje výjimku "FileNotFound"
     * @param inode identifikátor souboru
     * @param file file podrobnosti o souboru
     */
    void findFileByInode(inode_t inode, vFile* file) throw (ExceptionFileNotFound);


private:
    typedef map<inode_t, vFile* > table_t;

    /** nejvyšší doposud využité inode číslo */
    inode_t maxInode;

    /**
     * hlavní hashovací tabulka
     * mapování: inode -> atributy souboru
     */
    table_t table;

    /**
     * Pomocná tabulka pro rychlejší vyhledávání v adresářích podle názvu
     * mapování: parent inode -> inodes (inode souborů uvnitř rodiče)
     */
    map<inode_t, set<inode_t> > tableDirContent;
};

#endif	/* STRUCTTABLE_H */

