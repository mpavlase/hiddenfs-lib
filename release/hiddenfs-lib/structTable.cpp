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
        this->insertRootFile();
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



    void structTable::findFileByInode(inode_t inode, vFile* & file) {
        table_t::iterator i = this->table.find(inode);

        if(i == table.end()) {
            throw ExceptionFileNotFound(inode);
        }

        //*file = i->second;
        file = this->table[inode];
    }

    void structTable::findFileByName(std::string filename, inode_t parent, vFile** file) {
        std::set<inode_t> dirContent = this->tableDirContent[parent];
        vFile* tmpFile = NULL;

        for(std::set<inode_t>::iterator it = dirContent.begin(); it != dirContent.end(); it++) {
            this->findFileByInode(*it, tmpFile);

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
            ret_inode = rit->first;
            assert(rit->first == rit->second->inode);
            ret_inode = rit->second->inode + 1;
        }

        file->inode = ret_inode;

        return this->insertFile(file);
    }

    inode_t structTable::insertFile(vFile* file) {
        // vložení souboru do hlavní tabulky
        //this->table[ret_inode] = file;
        this->table.insert(std::pair<inode_t, vFile*>(file->inode, file));
        //this->table[ret_inode]file;

        // vložení souboru do (pomocné) tabulky rodičů
        this->tableDirContent[file->parent].insert(file->inode);

        return file->inode;
    }

    void structTable::removeFile(inode_t inode) {
        vFile* fileInfo;

        //try {
            this->findFileByInode(inode, fileInfo);
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

    void structTable::serialize(chainList_t* output) {
        // tato délka by měla být více než dostačující, protože serializované entity nezaberou mnoho prostoru
        bytestream_t stream[BLOCK_MAX_LENGTH];
        size_t size = 0;

        chainItem chain;
        vFile* file;

        // zjištění počtu dále zpracovaných bloků
        // iterace přes všechny soubory
        for(table_t::const_iterator i = this->table.begin(); i != this->table.end(); i++) {
            file = i->second;
            //std::cout << "table size: " << this->table.size() << std::endl;
            //std::cout << "content: " << i->second.content.size() << std::endl;

            // hrubá kopie struktury

            memset(&chain, '\0', sizeof(chain));
            memset(stream, '\0', sizeof(stream));
            // # dump právě jedné složky do bufferu

            size = 0;

            // dump 'inode'
            memcpy(stream + size, &(file->inode), sizeof(file->inode));
            size += sizeof(file->inode);

            // dump 'parent'
            memcpy(stream + size, &(file->parent), sizeof(file->parent));
            size += sizeof(file->parent);

            // dump 'size'
            memcpy(stream + size, &(file->size), sizeof(file->size));
            size += sizeof(file->size);

            // dump 'filename'
            assert(file->filename.size() <= FILENAME_MAX_LENGTH);
            memcpy(stream + size, file->filename.data(), file->filename.size());
            size += file->filename.size();

            // přidat '\0' pro korektní detekci konce řetězce -- memset není zcele nezbytný
            memset(stream + size, '\0', size);
            size += 1;

            // dump 'flags'
            memcpy(stream + size, &(file->flags), sizeof(file->flags));
            size += sizeof(file->flags);

            //std::cout << "#:";
            //std::cout.write((char*) vBlockBuff, vBlockBuffSize);
            //std::cout << "\n";

            // kopie obsahu
            chain.content = new bytestream_t[size];
            memset(chain.content, '\0', size);

            chain.length = size;
            memcpy(chain.content, stream, size);

            output->push_back(chain);
        }

        /* Pokud soubor nemá žádný obsah, jeden speciální mu přecejen vytvoříme,
         * jinak by se nedostal do seznamu entit vůbec. Speciální je ve složce
         * datového typu 'vBlock', která je po celé délce obsazena znakem \0.
         */
        /*
        if(this->table.begin() == this->table.end()) {
           memset(&chain, '\0', sizeof(chain));

            // # dump právě jedné složky do bufferu
            size = 0;

            // dump 'inode'
            memcpy(stream + size, (&i->first), sizeof(i->first));
            size += sizeof(i->first);

            // dump 'vBlock'
            memset(stream + size, '\0', SIZEOF_vBlock);
            size += SIZEOF_vBlock;

            // kopie obsahu
            chain.content = new bytestream_t[size];
            chain.length = size;
            memcpy(chain.content, stream, size);

            output->push_back(chain);
        }
        */
    }

    void structTable::deserialize(const chainList_t& input) {
        size_t offset = 0;
        vFile* file;
        inode_t inode;
        size_t filenameLen;
        int c;

        bytestream_t emptyBlock[SIZEOF_vBlock];
        memset(emptyBlock, '\0', SIZEOF_vBlock);

        for(chainList_t::const_iterator i = input.begin(); i != input.end(); i++) {
            offset = 0;
            filenameLen = 0;

            if(&((*i).content) == NULL) {
                std::cout << "CT::deserializace.content == NULL!\n";
                continue;
            }

            file = new vFile;

                //std::cout << "CT::deserializace.content adredsa: " << (int) (i->content) << std::endl;
                //std::cout << "CT::deserializace.content adredsa: " << (int) (i->content + offset) << std::endl;
                //std::cout << "hodnota: " << (int) *(i->content + offset) << std::endl;

            // rekonstrukce složky 'inode'
            memcpy(&(file->inode), i->content + offset, sizeof(file->inode));
            offset += sizeof(inode);

            // rekonstrukce složky 'parent'
            memcpy(&(file->parent), i->content + offset, sizeof(file->parent));
            offset += sizeof(file->parent);

            // rekonstrukce složky 'size'
            memcpy(&(file->size), i->content + offset, sizeof(file->size));
            offset += sizeof(file->size);

            // rekonstrukce složky 'filename'
            for(filenameLen = 0; filenameLen < FILENAME_MAX_LENGTH; filenameLen++) {
                c = *(i->content + offset + filenameLen);

                if(c == '\0') {
                    break;
                }

                file->filename.push_back(c);
            }
            offset += filenameLen;

            /* +1 za koncovou '\0' řetězce počítáme také, protože se ve vstupní
             * serializované entitě vyskytuje také, i když ji nikde dále neukládáme */
            offset += 1;

            // rekonstrukce složky 'size'
            memcpy(&(file->flags), i->content + offset, sizeof(file->flags));
            offset += sizeof(file->flags);

            // nalezli jsme prázdný blok, takže pouze zinicializujeme vnitřní struktury
            /*
            if(memcmp(emptyBlock, i->content + offset, SIZEOF_vBlock) == 0) {
                this->newEmptyContent(inode);
            } else {
                unserialize_vBlock(i->content + offset, SIZEOF_vBlock, &block);
                this->addContent(inode, block);
            }
            */

            assert(offset == i->length);

            if(file->inode == ROOT_INODE) {
                this->insertRootFile();
            } else {
                this->insertFile(file);
            }
        }
    }
}