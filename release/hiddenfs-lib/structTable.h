/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#ifndef STRUCTTABLE_H
#define	STRUCTTABLE_H

#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "types.h"
#include "exceptions.h"

namespace HiddenFS {
    class structTable {
    public:
        structTable();
        structTable(const structTable& orig);
        virtual ~structTable();
        static const inode_t ROOT_INODE = 1;
        static const inode_t ERR_INODE = -1;
        static const char PATH_DELIM = '/';
        typedef std::map<inode_t, std::set<inode_t> > table_content_t;

        /**
         * user-friendly obsah tabulek
         */
        void print(void);

        /**
         * Vytvoří nový soubor (přepíše hodnotu inode!) a zapíše všechna potřebná metadata (především přiřazení k "parent")
         * @param file očekává se vyplněná struktura (mimo složky inode)
         * @return inode přávě vloženého souboru
         */
        inode_t newFile(vFile* file);

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
        void findFileByName(std::string filename, inode_t parent, vFile** file);

        /**
         * Vyhledá soubor na základě inode, jinak vyhazuje výjimku "FileNotFound"
         * @param inode identifikátor souboru
         * @param file file podrobnosti o souboru
         */
        void findFileByInode(inode_t inode, vFile** file);


        /**
         * Překlad cesty ("/dir1/dir2/dir3") na inode (dir3)
         * @param path absolutní cesta ve virtuálním systému souborů
         * @return inode cílového adresáře
         */
        inode_t pathToInode(std::string path);
        /**
         * Překlad cesty ("/dir1/dir2/dir3") na inode (dir3)
         * @param path absolutní cesta ve virtuálním systému souborů
         * @return inode cílového adresáře
         */
        inode_t pathToInode(const char* path);
        /**
         * Překlad cesty ("/dir1/dir2/dir3") na inode (dir3)
         * @param path absolutní cesta ve virtuálním systému souborů
         * @param retFile naplní ukazatel na cílový soubor
         * @return inode cílového adresáře
         */
        inode_t pathToInode(std::string path, vFile** retFile);
        /**
         * Překlad cesty ("/dir1/dir2/dir3") na inode (dir3)
         * @param path absolutní cesta ve virtuálním systému souborů
         * @param retFile naplní ukazatel na cílový soubor
         * @return inode cílového adresáře
         */
        inode_t pathToInode(const char* path, vFile** retFile);

        /**
         * Rozdělení cesty (path) na inode rodiče (parent) a zbylý název souboru (filename)
         * @param path vstupní cesta pro překlad
         * @param parent vrací inode rodiče po překladu základu cesty
         * @param filename vrací zbylý název souboru z konce cesty
         */
        void splitPathToFilename(std::string path, inode_t* parent, std::string* filename);

        /**
         * Vrací iterátor na obsah virtuálního adresáře
         * @param inode inode adresáře
         * @return iterátor na seznam inode vnořených souborů
         */
        table_content_t::iterator directoryContent(inode_t inode) {
            return this->tableDirContent.find(inode);
        }

        /**
         * Změní atribut 'parent' od inode na jiný. Funkce volající tuto metodu MUSÍ ZAJISTIT,
         * aby se pod stejném 'parent' nevyskytovaly dva soubory stejného jména.
         * @param inode inode souboru, kterému se má změnit nadřazený adresáč
         * @param parent nový nadřazený adresář pro 'inode'
         */
        void moveInode(vFile* inode, inode_t parent);

    private:
        typedef std::map<inode_t, vFile* > table_t;

        /** nejvyšší doposud využité inode číslo */
        inode_t maxInode;

        /**
         * hlavní hashovací tabulka
         * mapování: inode -> atributy souboru
         */
        table_t table;

        /**
         * Pomocná tabulka pro rychlejší vyhledávání v adresářích podle názvu,
         * která je jen jiným pohledem na hlavní tabulku a proto ji nikdy neukládat.
         * mapování: parent inode -> inodes (inode souborů uvnitř rodiče)
         */
        table_content_t tableDirContent;
    };
}

#endif	/* STRUCTTABLE_H */

