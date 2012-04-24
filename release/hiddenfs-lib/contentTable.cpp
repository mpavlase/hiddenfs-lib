/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */

#include <map>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string.h>
#include <cstring>

#include "common.h"
#include "contentTable.h"
#include "exceptions.h"

namespace HiddenFS {
    contentTable::contentTable() {
    }
    /*
    contentTable::contentTable(const contentTable& orig) {
    }
    */

    contentTable::~contentTable() {
        // uvolnění tabulky fragmentů
        for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
            for(std::map<fragment_t, std::vector<vBlock*> >::iterator i = it->second.content.begin(); i !=  it->second.content.end(); i++) {
                for(std::vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++) {
                    delete *j;
                }
            }
        }
    }

    void contentTable::serialize(chainList_t* output) {
        bytestream_t stream[BLOCK_MAX_LENGTH];
        size_t size = 0;

        bytestream_t* vBlockBuff = NULL;
        size_t vBlockBuffSize = 0;
        chainItem chain;

        // zjištění počtu dále zpracovaných bloků
        // iterace přes všechny soubory
        for(table_t::const_iterator i = this->table.begin(); i != this->table.end(); i++) {
            memset(stream, '\0', sizeof(BLOCK_MAX_LENGTH));
            //std::cout << "table size: " << this->table.size() << std::endl;
            //std::cout << "content: " << i->second.content.size() << std::endl;

            // iterace přes všechny fragmenty souboru
            for(std::map<fragment_t, std::vector<vBlock*> >::const_iterator j = i->second.content.begin(); j != i->second.content.end(); j++) {
                // iterace přes všechny bloky fragmentů
                //std::cout << "bloky fragmenů: " << j->second.size() << std::endl;

                for(std::vector<vBlock*>::const_iterator k = j->second.begin(); k != j->second.end(); k++) {
                    // hrubá kopie struktury

                    //memset(&chain, '\0', sizeof(chain));
                    chain.content = NULL;
                    chain.length = 0;

                    // # dump právě jedné složky do bufferu
                    size = 0;

                    // dump 'inode'
                    memcpy(stream + size, (&i->first), sizeof(i->first));
                    size += sizeof(i->first);

                    // dump 'vBlock'
                    serialize_vBlock(*k, &vBlockBuff, &vBlockBuffSize);
                    memcpy(stream + size, vBlockBuff, vBlockBuffSize);
                    size += vBlockBuffSize;

                    // kopie obsahu
                    //chain.content = new bytestream_t[size];
                    //chain.length = size;
                    //memcpy(chain.content, stream, size);

                    //delete [] vBlockBuff;

                    output->push_back(chain);

                    /*
                    memset(&chain, '\0', sizeof(chain));

                    // # dump právě jedné složky do bufferu
                    size = 0;

                    // dump 'inode'
                    memcpy(stream.sputn((const char*) (&i->first), sizeof(i->first));
                    size += sizeof(i->first);

                    // dump 'vBlock'
                    serialize_vBlock(*k, &vBlockBuff, &vBlockBuffSize);
                    stream.sputn((const char*) vBlockBuff, vBlockBuffSize);
                    size += vBlockBuffSize;

                    // kopie obsahu
                    chain.content = new bytestream_t[size];
                    chain.length = size;
                    stream.sgetn((char*) chain.content, size);

                    delete vBlockBuff;

                    output->push_back(chain);
                    */
                }
            }

            /* Pokud soubor nemá žádný obsah, jeden speciální mu přecejen vytvoříme,
             * jinak by se nedostal do seznamu entit vůbec. Speciální je ve složce
             * datového typu 'vBlock', která je po celé délce obsazena znakem \0.
             */
            if(i->second.content.begin() == i->second.content.end()) {
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

                /*
                memset(&chain, '\0', sizeof(chain));

                // # dump právě jedné složky do bufferu
                size = 0;

                // dump 'inode'
                stream.sputn((const char*) (&i->first), sizeof(i->first));
                size += sizeof(i->first);

                // -----
                // stream.sgetn((char*) tmpBuff, size);
                // std::cout.write(tmpBuff, size).flush();
                // -----

                // dump 'vBlock'
                vBlockBuff = new bytestream_t[SIZEOF_vBlock];
                memset(vBlockBuff, '?', SIZEOF_vBlock);
                stream.sputn((char*) vBlockBuff, SIZEOF_vBlock);
                size += SIZEOF_vBlock;


                // -----
                // stream.sgetn((char*) tmpBuff, size);
                // std::cout.write(tmpBuff, size).flush();
                // -----

                // kopie obsahu
                chain.content = new bytestream_t[size];
                chain.length = size;

                stream.sgetn((char*) tmpBuff, size);
                std::cout.write(tmpBuff, size).flush();


                //stream.sgetn((char*) chain.content, size);
                //std::cout.write((char*) chain.content, size).flush();

                delete vBlockBuff;

                output->push_back(chain);
                */
            }
        }
    }

    void contentTable::deserialize(chainList_t input) {
        size_t offset = 0;
        vBlock* block;
        inode_t inode;
        bytestream_t emptyBlock[SIZEOF_vBlock];
        memset(emptyBlock, '\0', SIZEOF_vBlock);

        for(chainList_t::iterator i = input.begin(); i != input.end(); i++) {
            offset = 0;

            memcpy(&inode, ((*i).content) + offset, sizeof(inode));
            offset += sizeof(inode);

            // nalezli jsme prázdný blok, takže pouze zinicializujeme vnitřní struktury
            if(memcmp(emptyBlock, (*i).content + offset, SIZEOF_vBlock) == 0) {
                this->newEmptyContent(inode);
            } else {
                unserialize_vBlock((*i).content + offset, SIZEOF_vBlock, &block);
                this->addContent(inode, block);
            }

            offset += SIZEOF_vBlock;

            assert(offset == i->length);
        }
    }

    void contentTable::getReservedBlocks(inode_t inode, std::vector<vBlock*>* reserved) {
        reserved->assign(this->table[inode].reserved.begin(), this->table[inode].reserved.end());
    }

    void contentTable::print() {
        std::cout << "== contentTable ==" << "\n";
        std::cout << std::setw(6) << std::left << "inode"
            << "|" << "content" << "\n";
        for(table_t::iterator it = this->table.begin(); it != this->table.end(); it++) {
            std::cout << std::setw(6) << std::left << it->first << "|";
            //std::cout << "počet složek v sub-mapě: " << it->second.content.size() << std::endl;
            /// @todo tisknout i rezervované bloky
            for(std::map<fragment_t, std::vector<vBlock*> >::iterator i = it->second.content.begin(); i !=  it->second.content.end(); i++) {
                std::cout << " [#" << i->first << ": ";
                for(std::vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++)
                {
                    std::cout << print_vBlock(*j);
                }
                std::cout << "]";
            }
            std::cout << "\n";
        }
    }

    void contentTable::getMetadata(inode_t inode, tableItem* content) {
        content = &this->table[inode];
    }

    void contentTable::newEmptyContent(inode_t inode) {
        tableItem ti;
        ti.reservedBytes = 0;
        ti.content.clear();
        ti.reserved.clear();

        table_t::iterator iter = this->table.find(inode);

        if(iter == table.end()) {
            this->table[inode] = ti;
        } else {
            throw ExceptionInodeExists(inode);
        }
    }

}