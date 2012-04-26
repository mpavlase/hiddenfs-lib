/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include <exception>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>

#include "structTable.h"
#include "common.h"

namespace HiddenFS {
    structTable::structTable() {
        this->maxInode = 0;
        // TODO: vytvořit root?
        vFile* root = new vFile;
        root->filename = "/";
        root->flags = vFile::FLAG_DIR;
        root->parent = 1;
        this->newFile(root);
    }

    structTable::structTable(const structTable& orig) {
    }

    structTable::~structTable() {
        // uvolnění hlavní tabulky
        for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
            delete it->second;
        }

        /*
        // uvolnění pomocné tabulky
        for(map<inode_t, set<inode_t> >::iterator it = this->tableDirContent.begin(); it != this->tableDirContent.end(); it++) {
            delete it->second;
        }
        */
    }



    void structTable::findFileByInode(inode_t inode, vFile** file) {
        table_t::iterator i = table.find(inode);

        if(i == table.end()) {
            throw ExceptionFileNotFound(inode);
        }

        *file = i->second;
    }

    void structTable::findFileByName(std::string filename, inode_t parent, vFile** file) {
        std::set<inode_t> dirContent = this->tableDirContent[parent];
        vFile* tmpFile = NULL;

        for(std::set<inode_t>::iterator it = dirContent.begin(); it != dirContent.end(); it++) {
            this->findFileByInode(*it, &tmpFile);

            if(tmpFile->filename == filename) {
                *file = tmpFile;
                break;
            } else {
                tmpFile = NULL;
            }
        }

        if(tmpFile == NULL) {
            throw ExceptionFileNotFound(filename);
            std::cout << "toto by se nemělo nikdy zobrazit!!";
        }
    }


    void structTable::print() {
        std::cout << "== structTable.table (inode -> metadata souboru) ==\n";
        /*
        cout << setw(5) << left << "inode"
            << "|" << setw(6) << left << "parent"
            << "|" << setw(20) << left << "filename"
            << "|" << setw(4) << left << "size" << endl;
        cout << "--------------------------------------" << endl;
        */
        for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
            std::cout << print_vFile(it->second) << "\n";
        }
        std::cout << "\n";

        std::cout << "== structTable.tableDirContent (parent -> content) ==\n";
        std::cout << std::setw(6) << std::left << "parent"
            << "|" << "content\n";
        for(std::map<inode_t, std::set<inode_t> >::iterator it = this->tableDirContent.begin(); it != this->tableDirContent.end(); it++) {
            std::cout << std::setw(6) << std::left << it->first << "|";
            for(std::set<inode_t>::iterator i = it->second.begin(); i != it->second.end(); i++) {
                std::cout << *i << " ";
            }
            std::cout << "\n";
        }

        std::cout << "\n";
    }

    inode_t structTable::newFile(vFile* file) {
        inode_t ret_inode;

        if(this->table.size() == 0) {
            ret_inode = ROOT_INODE;
        } else {
            // nalezení posledního souboru a vypočtení nového (neobsazeného) inode.
            table_t::reverse_iterator rit = this->table.rbegin();
            ret_inode = rit->second->inode + 1;
        }

        file->inode = ret_inode;
        this->maxInode = ret_inode;

        // vložení souboru do hlavní tabulky
        this->table[ret_inode] = file;

        // vložení souboru do tabulky rodičů
        this->tableDirContent[file->parent].insert(ret_inode);

        return ret_inode;
    }

    void structTable::removeFile(inode_t inode) {
        vFile* fileInfo;

        //try {
            this->findFileByInode(inode, &fileInfo);
            this->table.erase(inode);
            this->tableDirContent[fileInfo->parent].erase(inode);
        //}
        //catch (ExceptionFileNotFound e) {
            //this->log()
        //}
    }

    inode_t structTable::pathToInode(const char* path, vFile** retFile) {
        std::string str = path;

        return this->pathToInode(str, retFile);
    }

    inode_t structTable::pathToInode(std::string path) {
        vFile* retFile;

        return this->pathToInode(path, &retFile);
    }

    inode_t structTable::pathToInode(const char* path) {
        vFile* retFile;
        std::string str = path;

        return this->pathToInode(str, &retFile);
    }
    inode_t structTable::pathToInode(std::string path, vFile** retFile) {

        //cout << "překlad cesty: '" << path << "' na inode:";
        std::string dirname;
        inode_t inode = -1;
        inode_t parent_inode;
        vFile* file;

        if(path == "/") {
            return structTable::ROOT_INODE;
        }

        // 1 = délka oddělovače (datový typ char má délku vždy 1)
        const unsigned int DELIM_LENGTH = 1;
        unsigned int pos = DELIM_LENGTH;

        // odseknutí prvního a posledního lomítka
        pos = path.find_first_of(PATH_DELIM);
        if(pos != std::string::npos) {
            path = path.substr(pos + DELIM_LENGTH);
        }
        pos = path.find_last_of(PATH_DELIM);
        if(pos == path.length()) {
            path = path.substr(0, pos - 1);
        }

        pos = 0;
        parent_inode = structTable::ROOT_INODE;

        /*
        try {
        */
            // průchod přes všechny adresáře cesty
            while((pos = path.find_first_of(PATH_DELIM)) != std::string::npos) {
                dirname = path.substr(0, pos);
                this->findFileByName(dirname, parent_inode, &file);
                parent_inode = file->inode;

                pos += DELIM_LENGTH;
                path = path.substr(pos);
            }

            //throw ExceptionFileNotFound(inode);

            this->findFileByName(path, parent_inode, &file);
            inode = file->inode;
            *retFile = file;
        /*
        } catch(ExceptionFileNotFound) {
            throw ExceptionFileNotFound(path);
        }
        */

        return inode;
    }

    void structTable::splitPathToFilename(std::string path, inode_t* parent, std::string* filename) {
        unsigned int pos;
        std::string destPath;

        pos = path.find_last_of(PATH_DELIM);
        if(pos == path.length()) {
            *parent = ROOT_INODE;
            *filename = "";
            return;
        }

        // oddělení cesty od samozného názvu souboru
        // pokud je path: /soubor.txt,
        if(pos == 0) {
            *parent = ROOT_INODE;
            *filename = path.substr(pos + 1);

            return;
        } else {
            destPath = path.substr(0, pos);
            *parent = this->pathToInode(destPath);

            // +1 je délka oddělovače (char má délku vždy 1)
            *filename = path.substr(pos + 1);
        }
    }

    void structTable::moveInode(vFile* file, inode_t parent) {
        // Volající metoda musí zajistit, aby se v novém umístění nenacházely dva soubory stejného jména!
        this->tableDirContent[file->parent].erase(file->inode);
        file->parent = parent;
        this->tableDirContent[parent].insert(file->inode);

        /// @todo tisknout obsah pouze pro ladění!
        this->print();
    }
}