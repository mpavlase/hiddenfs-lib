/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 18. březen 2012
 */


#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "hiddenfs-lib.h"
#include "common.h"

/*
struct fuse_file_info {
	// Open flags.	 Available in open() and release()
	int flags;

	// Old file handle, don't use
	unsigned long fh_old;

	// In case of a write operation indicates if this was caused by a
	    writepage
	int writepage;

	// Can be filled in by open, to use direct I/O on this file.
	    Introduced in version 2.4
	unsigned int direct_io : 1;

	// Can be filled in by open, to indicate, that cached file data
	    need not be invalidated.  Introduced in version 2.4
	unsigned int keep_cache : 1;

	// Indicates a flush operation.  Set in flush operation, also
	    maybe set in highlevel lock operation and lowlevel release
	    operation.	Introduced in version 2.6
	unsigned int flush : 1;

	// Can be filled in by open, to indicate that the file is not
	    seekable.  Introduced in version 2.8
	unsigned int nonseekable : 1;

	// Padding.  Do not use
	unsigned int padding : 28;

	// File handle.  May be filled in by filesystem in open().
	    Available in all other file operations
	uint64_t fh;

	// Lock owner id.  Available in locking operations and flush
	uint64_t lock_owner;
};
*/


namespace HiddenFS {

    void hiddenFs::allocatorFindFreeBlock_sameHash(vBlock*& block, hash_t hash) {
        /// @todo doimplementovat!
        throw ExceptionNotImplemented("hiddenFs::allocatorFindFreeBlock_sameHash");
    }

    void hiddenFs::allocatorFindFreeBlock_differentHash(vBlock*& block, std::set<hash_t>& excluded) {
        /// @todo doimplementovat!
        throw ExceptionNotImplemented("hiddenFs::allocatorFindFreeBlock_differentHash");
    }

    // hotovo
    void hiddenFs::allocatorFindFreeBlock_any(vBlock*& block) {
        std::set<hash_t> empty;
        this->allocatorFindFreeBlock_partUsedPreferred(block, empty);
    }

    // hotovo
    void hiddenFs::allocatorFindFreeBlock_any(vBlock*& block, hash_t prefered, std::set<hash_t>& excluded) {
        try {
            this->allocatorFindFreeBlock_sameHash(block, prefered);
        } catch(ExceptionBlockNotFound) {
            // I metoda allocatorFindFreeBlock_differentHash() může vyhazovat výjimku ExceptionBlockNotFound,
            // což je v pořádku, akorát to znamená, že v systému se už nenacházejí žádné volné bloky.
            try {
                 this->allocatorFindFreeBlock_differentHash(block, excluded);
            } catch (ExceptionBlockNotFound) {
                throw ExceptionDiscFull();
            }
        }
    }

    // hotovo
    void hiddenFs::allocatorFindFreeBlock_unused(vBlock*& block) {
        // momentálně žádné takové neexistují, proto se pokusíme najít ručně
        if(this->HT->auxList_unused.empty()) {
            this->findUnusedHash();

            if(this->HT->auxList_unused.empty()) {
                throw ExceptionBlockNotFound();
            }
        }

        hash_t hash = *(this->HT->auxList_unused.begin());

        this->allocatorFillFreeBlockByHash(hash, block);
    }

    void hiddenFs::allocatorFindFreeBlock_unusedPreferred(vBlock*& block) {
        try {
            this->allocatorFindFreeBlock_unused(block);
        } catch(ExceptionBlockNotFound) {
            this->allocatorFindFreeBlock_any(block);
        }
    }

    void hiddenFs::allocatorFillFreeBlockByHash(hash_t hash, vBlock*& block) {
        block = new vBlock;

        block->block = *(this->HT->find(hash)->second.context->avaliableBlocks.begin());
        block->fragment = 0;    // libovolná hodnota
        block->hash = hash;
        block->length = 0;      // libovolná hodnota
        block->used = false;
    }

    void hiddenFs::allocatorFindFreeBlock_partUsed(vBlock*& block, std::set<hash_t>& excluded) {
        /* Taktika pro výběr se zkouší jedna po druhé a pokud některá uspěje,
         * vrátí výsledek a metoda končí. Pořadí:
         * 1) zkusit vybrat některý již z obsazených souborů
         * 2) zkusit některý z volných hash
         * 3) pokud nejsou žádné volné, zkusit zaindexovat další soubory
         * 4) pokud ani tak nebudou existovat žádné volné, vyhazuje se výjimka ExceptionDiscFull
         * **/
        hash_t hash;

        if(this->HT->auxList_partlyUsed.empty()) {
            this->findPartlyUsedHash();

            if(this->HT->auxList_partlyUsed.empty()) {
                throw ExceptionBlockNotFound();
            }
        }

        hash = *(this->HT->auxList_partlyUsed.begin());

        this->allocatorFillFreeBlockByHash(hash, block);
    }

    void hiddenFs::allocatorFindFreeBlock_partUsedPreferred(vBlock*& block, std::set<hash_t>& excluded) {
        try {
            // zkusit najít částečně obsazený
            this->allocatorFindFreeBlock_partUsed(block, excluded);
        } catch(ExceptionBlockNotFound) {
            try {
                // pokud takový neexistuje, zkusit najít nějaký dosud neobsazený
                this->allocatorFindFreeBlock_unused(block);
            } catch(ExceptionBlockNotFound) {
                // pokud ani nevyjde, nemáme k dispozici už žádné bloky.
                throw ExceptionDiscFull();
            }
        }
    }

    void hiddenFs::allocatorStealReservedBlock(inode_t& inode, vBlock*& block) {
        this->CT->findAnyReservedBlock(inode, block);
        this->CT->setBlockAsUnused(inode, block);
    }

    size_t hiddenFs::allocatorAllocate(inode_t inode, bytestream_t* buffer, size_t lengthParam) {
        vFile* file = NULL;

        // počet fragmentů, nutných pro uložení celé délky souboru
        unsigned int fragmentsCount;
        off_t offset;
        bytestream_t* buffToWrite;
        vBlock* block;
        size_t contentLength;
        std::set<hash_t> excludedHash;

        contentTable::tableItem cTableItem;
        /*
        std::map<int, std::vector<vBlock*> >::iterator mapit;
        std::vector<vBlock*>::iterator vectit;
        */

        this->ST->findFileByInode(inode, file);
        this->encryption->encrypt(buffer, lengthParam, &buffToWrite, &contentLength);

        /// @todo smazat následující řádek!
        //length = file->size;

        fragmentsCount = ceil(1.0 * contentLength / BLOCK_USABLE_LENGTH);

        // 1) rozdělit celý soubor na fragmenty
        // 2) vypočítat počet bloků (myslet na redundanci!)
        // 3) načíst stávající obsah do bufferu pro případ obnovy
        // 4) v cyklu uložit fragmenty do bloků
        // - pokud se něco pokazí vrátit chybovou ERRNO

        unsigned int redundancyFinished;
        id_byte_t idByte;
        int blockLength;
        inode_t robbedInode;
        bool stealing;
        unsigned int maxReservedLength = round(ALLOCATOR_RESERVED_QUANTITY / 100.0 * contentLength);

        this->CT->getMetadata(inode, cTableItem);

        //assert(cTableItem.content.size() >= fragmentsCount);
        offset = 0;

        // iterace přes všechny fragmenty souboru

        //for(std::map<fragment_t, std::vector<vBlock*> >::iterator i = cTableItem.content.begin(); i != cTableItem.content.end(); i++) {}
        for(fragment_t f = FRAGMENT_FIRST; f < FRAGMENT_FIRST + fragmentsCount; f++) {

            // výpočet zbývající délky bloku namísto výchozí BLOCK_USABLE_LENGTH, pokud je to nutné
            // záporná hodnota blockLength signalizuje přebytek naalokovaných bloků
            if(offset + BLOCK_USABLE_LENGTH < contentLength) {
                // Offset ještě není takové délky, abychom museli zapsat jinou (=menší) délku, než je BLOCK_USABLE_LENGTH
                blockLength = BLOCK_USABLE_LENGTH;
            } else {
                /* Zapisujeme buď:
                 * a) poslední blok ze souboru, takže obsah je zbytek do BLOCK_USABLE_LENGTH
                 * b) zapsali jsme již všechny bloky skutečným obsahem; nyní už jen označujeme zbývající fragmenty jako rezervované
                 * **/
                blockLength = contentLength - offset;
            }

            redundancyFinished = 0;

            // uložení všech redundantních kopii jednoho fragmentu
            //std::cout << "cTableItem.content[f].size() = " << cTableItem.content[f].size() << std::endl;
            //assert(false);
            for(std::vector<vBlock*>::iterator j = cTableItem.content[f].begin() ; j != cTableItem.content[f].end(); j++) {
                /* Soubor má naalokováno příliš mnoho bloků, takže všechny ty,
                 * do kterých nebyl aktuálně proveden zápis označíme jako rezervované */
                if(offset > contentLength) {
                    blockLength = BLOCK_USABLE_LENGTH;      // může být libovolné nezáporné číslo
                    this->CT->setBlockAsReserved(file->inode, *j);

                    continue;
                }

                // následuje klasický zápis obsahu
                idByte = idByteGenDataBlock();

                try {
                    this->writeBlock((*j)->hash, (*j)->block, buffer + offset, blockLength, idByte);
                    (*j)->length = blockLength;
                    (*j)->used = true;
                    this->CT->setBlockAsUsed(file->inode, *j);
                    excludedHash.insert((*j)->hash);
                    redundancyFinished++;
                } catch(...) {
                    continue;
                }
            }

            // ještě nebyly zapsány všechny kopie souboru, zapsat je "ručně"
            if(redundancyFinished < this->allocatorRedundancy) {

                // Maximálně využít úložiště - do jednoho fyzického souboru ukládat bloky více virtuálních souborů
                if((this->allocatorFlags & ALLOCATOR_SPEED) != 0) {
                    while(redundancyFinished < this->allocatorRedundancy) {
                        try {
                            stealing = false;
                            this->allocatorFindFreeBlock_partUsedPreferred(block, excludedHash);
                        } catch(ExceptionDiscFull) {
                            /* Všechny bloky v celém FS jsou již obsazené, zbývá proto
                             * jen "ukrást" jiným virt. souborům jejich rezervované bloky */
                            stealing = true;

                            try {
                                this->allocatorStealReservedBlock(robbedInode, block);
                            } catch(ExceptionDiscFull) {
                                break;
                            }
                        }

                        block->fragment = f;
                        block->length = blockLength;
                        block->used = true;

                        idByte = idByteGenDataBlock();
                        excludedHash.insert(block->hash);

                        try {
                            this->writeBlock(block->hash, block->block, buffer + offset, blockLength, idByte);
                        } catch(...) {
                            if(stealing) {
                                /// @todo vracet rezervovaný blok původnímu souboru? Možná nakonec ani ne...
                                ///this->CT->setBlockAsReserved(robbedInode, block);
                            } else {
                                delete block;
                            }

                            continue;
                        }

                        this->CT->addContent(file->inode, block);
                        redundancyFinished++;
                    }
                }

                // Nemíchat více virtuálních souborů do jednoho fyzického
                if((this->allocatorFlags & ALLOCATOR_SECURITY) != 0) {
                    while(redundancyFinished < this->allocatorRedundancy) {
                        try {
                            stealing = false;
                            this->allocatorFindFreeBlock_unusedPreferred(block);
                        } catch(ExceptionDiscFull) {
                            /* Všechny bloky v celém FS jsou již obsazené, zbývá proto
                             * jen "ukrást" jiným virt. souborům jejich rezervované bloky */
                            stealing = true;

                            try {
                                this->allocatorStealReservedBlock(robbedInode, block);
                            } catch(ExceptionDiscFull) {
                                break;
                            }
                        }

                        block->fragment = f;
                        block->length = blockLength;
                        block->used = true;

                        idByte = idByteGenDataBlock();
                        excludedHash.insert(block->hash);

                        try {
                            this->writeBlock(block->hash, block->block, buffer + offset, blockLength, idByte);
                        } catch(...) {
                            if(stealing) {
                                /// @todo vracet rezervovaný blok původnímu souboru? Možná nakonec ani ne...
                                ///this->CT->setBlockAsReserved(robbedInode, block);
                            } else {
                                delete block;
                            }

                            continue;
                        }

                        this->CT->addContent(file->inode, block);
                        redundancyFinished++;
                    }
                }
            }       // ještě nebyly zapsány všechny kopie souboru, zapsat je "ručně"

            offset += blockLength;
        } // iterace přes všechny již obsazené fragmenty


        /* Obsah je už zapsán celý, nyní jen zkontrolujeme, zda soubor
         * nemá zarezervováno příliš mnoho bloku */
        std::set<vBlock*>::iterator setvBlockIterator;
        while(cTableItem.reservedBytes > maxReservedLength) {
            setvBlockIterator = cTableItem.reserved.begin();

            // nejdříve fyzicky odstraníme blok ...
            this->removeBlock((*setvBlockIterator)->hash, (*setvBlockIterator)->block);

            // ... a až poté jej uvolníme i pomocných tabulek
            this->CT->setBlockAsUnused(file->inode, *setvBlockIterator);
        }

        // "Na disku není dostatek místa"
        //throw ExceptionDiscFull();

        return lengthParam;
    }

    void hiddenFs::allocatorSetStrategy(unsigned char strategy, unsigned int redundancy = 1) {
        this->allocatorFlags = 0;
        unsigned int defaultRedundancy = 1;

        // příznaky ALLOCATOR_SECURITY a ALLOCATOR_SPEED jsou navzájem výlučné
        if((strategy & ALLOCATOR_SECURITY) > 0) {
            this->allocatorFlags = (this->allocatorFlags & ~ALLOCATOR_SPEED) | ALLOCATOR_SECURITY;
        }

        if((strategy & ALLOCATOR_SPEED) > 0) {
            this->allocatorFlags = (this->allocatorFlags & ~ALLOCATOR_SECURITY) | ALLOCATOR_SPEED;
        }

        if((strategy & ALLOCATOR_REDUNDANCY) > 0) {
            this->allocatorFlags = this->allocatorFlags | ALLOCATOR_REDUNDANCY;
            if(redundancy > 0) {
                this->allocatorRedundancy = redundancy;
            } else {
                _D("hiddenFs::allocatorSetStrategy - množství redundance musí být > 0 (nastavuji na " << defaultRedundancy << ").");
                this->allocatorRedundancy = defaultRedundancy;
            }
        } else {
            this->allocatorRedundancy = defaultRedundancy;
        }

            /*
            default: {
                stringstream ss;
                ss << "allocatorEngine::allocate - strategy '";
                ss << this->allocatorStrategy;
                ss << "' is not implemented.";

                throw ExceptionNotImplemented(ss.str());
            }
            */
    }

    hiddenFs::hiddenFs(IEncryption* instance = NULL) {
        ///@todo po odladění odkomentovat!
        //srand(time(NULL));
        srand(1);

        this->HT = new hashTable();
        this->ST = new structTable();
        this->CT = new contentTable();
        this->SB = new superBlock();
        this->checksum = new CRC;
        if(instance == NULL) {
            instance = new DefaultEncryption();
        }
        this->encryption = instance;

        this->SB = new superBlock();
        this->SB->setEncryptionInstance(this->encryption);

        /*
        std::stringbuf s;
        s.sputn("abc", 3);
        s.sputc(0);
        s.sputc('d');
        //std::cout << sizeof(vBlock) << std::endl;
        int si = 5;
        si = sizeof(vFile);
        si = sizeof(vBlock);
        si = sizeof(hash_t);
        si = sizeof(HiddenFS::superBlock::tableItem);
        char* bf = new char[si];
        char c;
        s.sgetn(bf, si);

        for(int i = 0; i < si; i++) {
            std::cout.flags(std::stringstream::hex | std::stringstream::showbase);
            std::cout.setf(std::ios::hex | std::ios::showbase);
            c = bf[i];
            std::cout << (unsigned char)c << " ";
            //printf("%X ", bf[i]);
        }
        throw ExceptionRuntimeError("...");
        */

        this->allocatorSetStrategy(ALLOCATOR_SPEED);
        //this->cacheHashTable = true;
    }

    hiddenFs::~hiddenFs() {
        delete this->HT;
        delete this->ST;
        delete this->CT;
        delete this->checksum;
        delete this->encryption;
        delete this->SB;
    }

    /** do stbuf nastavit délku souboru */
    int hiddenFs::fuse_getattr(const char* path, struct stat* stbuf) {
        int ret = 0;
        inode_t inode;
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;

        std::cout << "\n------------------------------------- \"" << path << "\" -- GET ATTR --\n";

        try {
            inode = hfs->ST->pathToInode(path);
            hfs->ST->findFileByInode(inode, file);
        } catch (ExceptionFileNotFound) {
            return -ENOENT;
        }

        std::cout << "-------------------------------------\n";
        std::cout << print_vFile(file) << "\n";
        std::cout << "-------------------------------------\n";
        hfs->ST->print();
        std::cout << "-------------------------------------\n";
        std::cout << "-------------------------------------\n";

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_uid = getuid();
        stbuf->st_gid = stbuf->st_uid;
        stbuf->st_mode = S_IWUSR | S_IRUSR;
        if(file->flags & vFile::FLAG_DIR) {
            stbuf->st_mode |= S_IFDIR;
            stbuf->st_nlink = 2;
        } else {
            stbuf->st_mode |= S_IFREG;
            stbuf->st_size = file->size;
            stbuf->st_nlink = 1;
        }

        return ret;
    }

    int hiddenFs::fuse_create(const char* path, mode_t mode, struct fuse_file_info* file_i) {
        std::cout << "\n&&&&&&&&& &&&&&&& &&&&&&& &&&&&&& &&&&&&& &&&&&&&\n";
        std::cout << "&&&&&&&&& &&&&&&& &&&&&&&      fuse_creat: " << path << "\n";
        std::cout << "&&&&&&&&& &&&&&&& &&&&&&& &&&&&&& &&&&&&& &&&&&&&\n\n";

        inode_t inode;
        hiddenFs* hfs = GET_INSTANCE;
        vFile* file = new vFile;

        hfs->ST->print();

        /// @todo nastavit mode podle 'mode'
        file->flags = vFile::FLAG_NONE | vFile::FLAG_READ | vFile::FLAG_WRITE | vFile::FLAG_EXECUTE;

        hfs->ST->splitPathToFilename(path, &file->parent, &file->filename);

        std::cout << print_vFile(file) << "\n";

        inode = hfs->ST->newFile(file);

        hfs->ST->print();

        hfs->CT->newEmptyContent(inode);

        return 0;
    }

    int hiddenFs::fuse_mkdir(const char* path, mode_t mode) {
        inode_t parent;
        std::string pathStr = path;
        std::string filename;
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< " << "\n";
        std::cout << " << <<<<<<<<<" << path << "<<<<<<<<<< " << "\n";
        std::cout << " << mode: " << mode << "\n";
        std::cout << (mode|S_IFDIR) << "\n";
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< " << "\n";

        hfs->ST->splitPathToFilename(pathStr, &parent, &filename);
        hfs->ST->print();

        file = new vFile;
        file->flags |= vFile::FLAG_DIR;
        file->filename = filename;
        file->parent = parent;
        hfs->ST->newFile(file);

        std::cout << " <<<<<<<<<<<<<<<<<<<<<<<<<< \n";
        std::cout << " <<<<<<<<<<<" << path << "<<<<<<<<<< \n";
        std::cout << "parent: " << parent << ", filename: " << filename << "\n";
        std::cout << " <<<<<<<<<<<<<<<<<<<<<<<<<< \n";

        return 0;
    }

    int hiddenFs::fuse_open(const char* path, struct fuse_file_info* file_i) {
        // povolit otevření souboru bez dalších okolků
        return 0;

        // nemáme oprávnění pro čtení tohoto souboru
        return -EACCES;

        // soubor neexistuje
        return -ENOENT;
        return 0;
    }

    int hiddenFs::fuse_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* file_i) {
        // 1) překlad cesty na cílový i-node
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;

        std::cout << " <<<<<<<<<<<<<<<<<<<<<<<<<< \n";
        std::stringstream str;
        str << "fuse_read('";
        str << path;
        str << "')\n";

        try {
            hfs->ST->pathToInode(path, &file);
            str << "length: " << file->size << "\n";
        } catch (ExceptionFileNotFound) {
            std::cout << " ## " << "path se nepodařilo přeložit na inode..." << " ##\n";
            /** @todo po doladění fuse_read odkomentovat! */
            return -ENOENT;
        }

        /*
        std::cout << "Obsah souboru: -" << str.str() << "-\n";
        size = str.str().length();
        memcpy(buffer, str.str().c_str(), size);
        std::cout << "buffer: -" << buffer << "-\n";

        */

        /// @todo odkomentovat!
        return size;


        // 2) načíst obsah
        bytestream_t* bufferContent;
        size_t sizeContent;

        hfs->getContent(file->inode, &bufferContent, &sizeContent);

        // 3) naplnění obsahu
        if(offset > sizeContent) {
            // ochrana proti čtení "za soubor"
            if(size + offset > sizeContent) {
                size = sizeContent - offset;
            }

            memcpy(buffer, bufferContent + offset, size);
        } else {
            size = 0;
        }

        return size;
    }

    int hiddenFs::fuse_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_i) {
        inode_t inode;
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;
        std::map<inode_t, std::set<inode_t> >::iterator it;

        /// @todo přidání následujících dvou řádek způsobuje SEGFAULT - jak je to možně?
        //filler(buffer, ".", NULL, 0);
        //filler(buffer, "..", NULL, 0);

        try {

            inode = hfs->ST->pathToInode(path);
            hfs->ST->findFileByInode(inode, file);

            it = hfs->ST->directoryContent(file->inode);
            for(std::set<inode_t>::iterator i = it->second.begin(); i != it->second.end(); i++) {
                // kořenový adresář nevypisovat, protože se jedná jen o logický záznam
                if(*i == structTable::ROOT_INODE) {
                    continue;
                }

                hfs->ST->findFileByInode((*i), file);
                filler(buffer, file->filename.c_str(), NULL, 0);
            }
        } catch (ExceptionFileNotFound) {
            return -ENOENT;
        }

        return -errno;
    }

    int hiddenFs::fuse_rename(const char* from, const char* to) {
        inode_t inodeParent, inode;
        std::string filenameTo;
        hiddenFs* hfs = GET_INSTANCE;
        vFile* fileFrom;
        vFile* fileTo;

        // Rozfázování jednotlivých operací:

        // 1) vyhledat "from"
        // 2) vyhledat "to"
        // 2a) pokud "to" EXISTUJE
        //     - ST->removeFile(from) -- pouze obsah, cestu budeme pořebovat
        //     - ST->moveFile(from(inode), to(inode) parent)
        // 2b) pokud "to" NEEXISTUJE
        //     - ST->moveFile(from(inode), to(inode) parent)

        inode = hfs->ST->pathToInode(from);
        hfs->ST->findFileByInode(inode, fileFrom);

        hfs->ST->splitPathToFilename(to, &inodeParent, &filenameTo);
        try {
            hfs->ST->findFileByName(filenameTo, inodeParent, &fileTo);

            // v cílovém adresáři se už tento souboru nachází -> starý smazat
            hfs->ST->removeFile(fileTo->inode);
        } catch (ExceptionFileNotFound) { }

        hfs->ST->moveInode(fileFrom, inodeParent);

        fileFrom->filename = filenameTo;

        return 0;
    }

    int hiddenFs::fuse_rmdir(const char* path) {
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< \n";
        std::cout << " << <<<<<<<<<" << path << "<<<<<<<<<< \n";
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< \n";
        throw "hiddenFs::fuse_rmdir ještě není implementované!";

        return 0;
    }

    int hiddenFs::fuse_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_i) {
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;
        size_t sizeWritten;
        size_t sizeNew;
        size_t sizeOriginal;
        bytestream_t* contentNew;
        bytestream_t* contentOriginal;

        hfs->ST->pathToInode(path, &file);

        assert(file->size >= offset);

        hfs->getContent(file->inode, &contentOriginal, &sizeOriginal);
        assert(sizeOriginal == file->size);

        if(file->size > offset + size) {
            // zápis nezpůsobí zvětšení délky souboru
            sizeNew = file->size;
            contentNew = new bytestream_t[sizeNew];
            memset(contentNew, '\0', sizeNew);

            memcpy(contentNew, contentOriginal, file->size);
            memcpy(contentNew + offset, buffer, size);
        } else {
            // nový zápis prodlužuje soubor
            sizeNew = offset + file->size;
            contentNew = new bytestream_t[sizeNew];
            memset(contentNew, '\0', sizeNew);

            // do nového obsahu zkopírujeme tu část původního obsahu, která nebude přepsána
            memcpy(contentNew, contentOriginal, offset);
            memcpy(contentNew + offset, buffer, size);
        }

        try {
            sizeWritten = hfs->allocatorAllocate(file->inode, contentNew, sizeNew);
        } catch(ExceptionDiscFull) {
            return -ENOSPC;
        } catch(...) {
            /// @todo ošetřit i jiné druhy chyb
        }

        return sizeWritten;
    }

    int hiddenFs::run(int argc, char* argv[]) {
        std::string path;
        //HiddenFS::hash_t_sizeof.set(20);

        if(argc == 2) {
            path.assign(argv[1]);
        } else {
            path.assign("../../mp3_examples");
        }

        /*
        smartConst<int> smart;
        smart.set(14);
        std::cout << "hash_sizeof: " << smart.get() << std::endl;
        assert(false);
        */

/*
        vBlock* bl = new vBlock;
        bl->block = 3;
        bl->fragment = 1;
        bl->hash = "hashString";
        bl->length = 4096;
        bl->used = true;
*/

        this->storageRefreshIndex(path);
        this->HT->print();

        if(this->HT->size() == 0) {
            std::cerr << "Úložiště neobsahuje žádné dostupné soubory.\n";

            return EXIT_FAILURE;
        }

        // 1) nalezení boot-block
        id_byte_t idByte;

        /*
        bytestream_t* bufferBlock;
        for(hashTable::table_t_constiterator i = this->HT->begin(); i != this->HT->end(); i++) {
            std::cout << "V souboru: " << i->second << " hledám superblock...\n";
            try {
                this->readBlock((hash_t)i->first, FIRST_BLOCK_NO, &bufferBlock, BLOCK_MAX_LENGTH, &idByte);
                this->SB->deserialize(bufferBlock, BLOCK_MAX_LENGTH);
                break;
            } catch (...) {
                std::cout << "SB nenalezen.\n\n";
                continue;
            }
        }

        */

        //bytestream_t* ctBuf = NULL;
        //size_t ctBufSize = 0;

        std::cout << "=====================================\n";
        std::cout << "=====================================\n";
        std::cout << "=====================================\n";

        hash_t h = this->HT->begin()->first;
        //std::cout << "První položka z HT: " << this->HT->begin()->second.filename << "\n";

        std::ifstream f;
        f.open("test.gif", std::fstream::binary);
        f.seekg(0, std::fstream::end);
        unsigned int length = f.tellg();
        f.seekg(0, std::fstream::beg);


        bytestream_t buffW[length];
        memset(buffW, '\0', sizeof(buffW));
        size_t len;
        f.read((char*)buffW, length);
        /*
        memcpy(buffW, "abcdefghijklmnopq", len);
        */

        /*
        block_number_t blockW = 1;
        //std::cout << "write: ";
        //std::cout.write((const char*)buffW, len);
        //std::cout << std::endl;
        bytestream_t* encBuf;
        bytestream_t* decBuf;
        size_t encBufLen;
        size_t decBufLen;
        this->encryption->encrypt(buffW, len, &encBuf, &encBufLen);

        //this->writeBlock(h, blockW, encBuf, encBufLen, idb);
        //this->writeBlock(h, blockW + 10, buffW, len, idb);
        //this->writeBlock(h, blockW + 16, buffW, len, idb);

        idb = 0;
        bytestream_t* buffR;
        //memset(buffW, '\0', sizeof(buffW));

        size_t r = this->readBlock(h, blockW, &buffR, BLOCK_USABLE_LENGTH, &idb);
        //std::cout << "read-bytes surové: " << r << ", obsah: ";
        //std::cout.write((const char*)buffR, BLOCK_USABLE_LENGTH);
        //std::cout << std::endl;

        this->encryption->decrypt(buffR, BLOCK_USABLE_LENGTH, &decBuf, &decBufLen);
        std::cout << "read-bytes po decryption " << r << ", obsah: ";
        std::cout.write((const char*)decBuf, len);
        std::cout << std::endl;

        this->removeBlock(h, 1);
        this->removeBlock(h, 11);
        this->removeBlock(h, 17);

        assert(false);
        */

        inode_t inode_ret;
        /*
        bytestream_t* encBuf;
        bytestream_t* decBuf;
        size_t encBufLen;
        size_t decBufLen;
        */

        vFile* file;
        file = new vFile;
        file->filename = "souborXYZ.txt";
        file->flags = vFile::FLAG_NONE;
        file->parent = 1;
        file->size = 47;
        this->ST->newFile(file);

        this->CT->newEmptyContent(file->inode);

        file = new vFile;
        file->filename = "soubor2.txt";
        file->flags = vFile::FLAG_NONE;
        file->parent = 1;
        file->size = sizeof(buffW);
        this->ST->newFile(file);

        this->CT->newEmptyContent(file->inode);


            chainList_t chainST;
            this->ST->serialize(&chainST);

            size_t sum = 0;
            for(chainList_t::const_iterator i = chainST.begin(); i != chainST.end(); i++) {
                sum += i->length;
            }

            std::cout << "celková délka: " << sum << "B\n";

            this->ST->print();
            delete this->ST;

            this->ST = new structTable();
            this->ST->deserialize(chainST);
            this->ST->print();
            assert(false);


        bytestream_t* bufRet;
        size_t bufRetLen;
        size_t r;

        id_byte_t idb;
        unsigned int bl;
        size_t lenBlock;
        //len = 10;
        //memcpy(buffW, "abcdefghijklMNOPQRSTUVWXYZ", len);

        this->ST->print();

        /*
        len = 0;
        while(len < sizeof(buffW)) {
            for(char i = 'a'; i <= 'z'; i++) {
                if(len < sizeof(buffW)) {
                    buffW[len] = i;
                    len++;
                } else {
                    break;
                }
            }
        }
        */

        //file->size += 300;

        /*
        this->allocatorAllocate(file->inode, buffW, file->size);
        this->CT->print();
        this->ST->print();
        */

        /*
        this->allocatorAllocate(file->inode, buffW, len);
        this->CT->print();
        this->ST->print();

        assert(false);
        */
        // ====================================
        /*
        std::cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=" << std::endl;
        this->getContent(file->inode, &bufRet, &bufRetLen);

        std::ofstream fout;
        fout.open("test-result.gif", std::fstream::binary);
        fout.write((char*)bufRet, bufRetLen);
        */

        //std::cout << "Kompletní obsah: _";
        //std::cout.write((const char*)bufRet, bufRetLen);
        //std::cout << "_" << std::endl;
        assert(false);

        //assert(false);
        // ====================================
        // ====================================
                //      serializace CONTENT TABLE
            /*
            chainList_t chainCT;
            this->CT->serialize(&chainCT);

            size_t sum = 0;
            for(chainList_t::const_iterator i = chainCT.begin(); i != chainCT.end(); i++) {
                sum += i->length;
            }

            std::cout << "celková délka: " << sum << "B\n";

            delete this->CT;

            this->CT = new contentTable();
            this->CT->deserialize(chainCT);
            this->CT->print();
            */
        // ====================================
        // ====================================
        //std::cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=" << std::endl;


        std::cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=" << std::endl;

        // ====================================
        this->getContent(file->inode, &bufRet, &bufRetLen);
        std::cout << "Kompletní obsah: _";
        std::cout.write((const char*)bufRet, bufRetLen);
        std::cout << "_" << std::endl;
        assert(false);

        bytestream_t* bufRetDec;
        size_t bufRetDecLen;
        this->encryption->decrypt(bufRet, bufRetLen, &bufRetDec, &bufRetDecLen);

        std::cout << "Dešifrovaný obsah, po slepení (getContent): _";
        std::cout.write((const char*)bufRetDec, bufRetDecLen);
        std::cout << "_" << std::endl;

        /*
        for(int i = 0; i < 1000; i++) {
            vb= new vBlock();
            vb->block = 17 + i;
            vb->fragment = i;
            vb->hash = "hahsX.XXXXXXXXXXXXXc";
            vb->length = 20 + i;
            vb->used = true;
            this->CT->addContent(file->inode, vb);
        }
        */

        assert(false);

        //this->allocator->allocate(file->inode, content, length);

        //std::cout << "\n";
        //this->CT->print();

        chainList_t chainCT;
        this->CT->serialize(&chainCT);

        sum = 0;
        for(chainList_t::const_iterator i = chainCT.begin(); i != chainCT.end(); i++) {
            sum += i->length;
        }

        std::cout << "celková délka: " << sum << "B\n";

        delete this->CT;
        this->CT = new contentTable();
        this->CT->deserialize(chainCT);
        //this->CT->print();


        assert(false);

        // první spuštění - super block zatím neexistuje, bude nutné jej vytvořit.
        if(!this->SB->isLoaded()) {
            // hlavní záznam uložit do prvního souboru (prvního z pohledu seřazených hash
            hashTable::table_t_constiterator i;
            bytestream_t* sbBuffer;
            size_t sbBufferLen = 0;
            idByte = idByteGenSuperBlock();

            // serializovat ST
            // serializovat CT

            for(hashTable::table_t_constiterator i = this->HT->begin(); i != this->HT->end(); i++) {
                try {
                    this->readBlock(i->first, FIRST_BLOCK_NO, &sbBuffer, BLOCK_MAX_LENGTH, &idByte);
                } catch (ExceptionBlockNotUsed) {
                    this->SB->serialize(&sbBuffer, &sbBufferLen);

                    if(sbBufferLen > BLOCK_MAX_LENGTH) {
                        std::cerr << "Superblok se nevejde do jediného bloku!\n";
                        return 0;
                    }

                    this->writeBlock(i->first, FIRST_BLOCK_NO, sbBuffer, sbBufferLen, idByte);
                } /*catch (std::exception& e) {
                    std::cerr << e.what() << "\n";
                    continue;
                }*/
            }

        }

        // 2) podle potřeby založit ST, CT, HT

        // 3) načíst ST, CT, HT

        // 3c) nové načtení obsahu hashTable

        assert(false);

        //inode_t inode_ret;

        //vFile* file;
        file = new vFile;
        file->filename = "soubor2.txt";
        file->flags = vFile::FLAG_NONE;
        file->parent = 1;
        file->size = 37;
        this->ST->newFile(file);

        file = new vFile;
        file->filename = "dir1";
        file->flags = vFile::FLAG_DIR;
        file->parent = 1;
        file->size = 0;
        inode_ret = this->ST->newFile(file);

        file = new vFile;
        file->filename = "souborJiný.txt";
        file->flags = vFile::FLAG_NONE;
        file->parent = inode_ret;
        file->size = 123;
        this->ST->newFile(file);
        this->ST->print();

        vFile* ftest;
        this->ST->findFileByInode(1, ftest);


        /*
        char* content = new char[file->size];
        string content_string = "obsah souboru";
        size_t length = sizeof(content_string.c_str());
        memcpy(content, content_string.c_str(), length);

        this->allocator->allocate(file->inode, content, length);

        cout << endl;
        this->CT->print();
        */

        // set allowed FUSE operations
        static struct fuse_operations fsOperations;

        fsOperations.getattr = this->fuse_getattr;
        fsOperations.readdir = this->fuse_readdir;
        fsOperations.open = this->fuse_open;
        fsOperations.read = this->fuse_read;
        fsOperations.write = this->fuse_write;
        fsOperations.create  = this->fuse_create;
        fsOperations.rename  = this->fuse_rename;
        fsOperations.mkdir  = this->fuse_mkdir;
        fsOperations.rmdir  = this->fuse_rmdir;

        //delete [] content;

        // private data = pointer to actual instance
        /** @todo vyřazení FUSE z provozu!! */
        return fuse_main(argc, argv, &fsOperations, this);
        return 0;
    }

    /*
    void hiddenFs::enableCacheHashTable(bool cache) {
        this->cacheHashTable = cache;
    }
    */

    void hiddenFs::packData(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize, id_byte_t id_byte) {
        size_t dataOffset;
        size_t offset = 0;
        checksum_t crc;

        bytestream_t* tmpOutput;
        size_t tmpOutputSize;

        if(inputSize > BLOCK_USABLE_LENGTH) {
            throw ExceptionRuntimeError("Vstupní data jsou delší, než je maximální dostupná délka jednoho bloku!");
        }

        // jednotlivé složky jsou uloženy v následujícím pořadí:
        // | CRC | id_byte |     užitný obsah bloku     |
        dataOffset = sizeof(checksum_t) + sizeof(id_byte_t);

        // zápis kontrolního součtu
        //memcpy(*output, &crc, sizeof(checksum_t));
        offset += sizeof(checksum_t);

        tmpOutputSize = BLOCK_MAX_LENGTH;
        tmpOutput = new bytestream_t[tmpOutputSize];
        memset(tmpOutput, '\0', tmpOutputSize);

        // uložení identifikačního byte
        memcpy(tmpOutput + offset, &id_byte, sizeof(id_byte));
        offset += sizeof(id_byte);

        // uložení užitného obsahu bloku
        memcpy(tmpOutput + offset, input, inputSize);
        offset += inputSize;

        assert(inputSize <= tmpOutputSize);

        // výpočet kontrolního součtu ze všech složek struktury, pochopitelně
        // až na složku se součtem samotným
        size_t crc_len = tmpOutputSize - sizeof(checksum_t);
        crc = this->checksum->calculate((((bytestream_t*) (tmpOutput)) + sizeof(checksum_t)), crc_len);

        // zápis kontrolního součtu
        memcpy(tmpOutput, &crc, sizeof(checksum_t));
        //std::cout << "\nPACKdata (s CRC): ";
        //pBytes(tmpOutput, crc_len - 4050);

        *output = tmpOutput;
        *outputSize = tmpOutputSize;
    }

    void hiddenFs::unpackData(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize, id_byte_t* id_byte) {
        checksum_t crcOrig, crcCalculated;
        size_t offset;

        offset = sizeof(checksum_t);

        // extrakce kontrolního součtu
        memcpy(&crcOrig, input, sizeof(checksum_t));
        size_t crc_len = inputSize - offset;
        //std::cout << "\nUnpackdata (s CRC): ";
        //pBytes(input, crc_len - 4050);
        crcCalculated = this->checksum->calculate(input + offset, crc_len);
        //std::cout << "\n";

        if(crcCalculated != crcOrig) {
            std::cout << " Nesouhlasí CRC! (orig " << crcOrig << " != calc " << crcCalculated << ") ";
            /// @todo po odladění odkomentovat
            throw ExceptionRuntimeError("Kontrolní součet bloku nesouhlasí!");
        }

        // extrakce identifikačního byte
        memcpy(id_byte, input + offset, sizeof(id_byte_t));
        offset += sizeof(id_byte_t);

        *outputSize = inputSize - offset;

        // extrakce užitné části
        *output = new bytestream_t[*outputSize];
        memset(*output, '\0', *outputSize);
        memcpy(*output, input + offset, *outputSize);
    }

    void hiddenFs::getContent(inode_t inode, bytestream_t** buffer, size_t* length) {
        contentTable::tableItem ti;
        vFile* file = NULL;
        bytestream_t* fragmentBuffer;
        bytestream_t* fragmentBufferPost;
        size_t len = 0;                     // pozice posledního zapsaného fragmentu
        *length = 0;
        unsigned int counter = FRAGMENT_FIRST;                    // pořadové číslo posledního zapsaného fragmentu
        size_t blockLen = 0;                // celková délka bloku (nejen jeho využitého obsahu)
        //size_t fragmentContentLen;
        id_byte_t idByte;

        this->CT->getMetadata(inode, ti);
        this->ST->findFileByInode(inode, file);

        // naalokování bufferu pro kompletní obsah souboru
        std::stringbuf stream;

        // iterace přes všechny fragmenty souboru
        for(std::map<fragment_t, std::vector<vBlock*> >::iterator i = ti.content.begin(); i != ti.content.end(); i++) {
            // detekce chybějícího fragmentu
            if(counter != i->first) {
                std::stringstream ss;

                ss << "Soubor s inode č. " << inode << " v souboru neobsahuje všechny fragmenty (díra)!";
                throw ExceptionRuntimeError(ss.str());
            }

            // nalezení prvního nepoškozeného bloku a připojení fragmentu ke zbytku skutečného souboru
            for(std::vector<vBlock*>::iterator j = i->second.begin() ; j != i->second.end(); j++) {
                /**
                 * 1) v try {} načíst blok podle hashe přes blockRead
                 * 2) unpackData
                 * 3)
                 */

                try {
                    fragmentBuffer = NULL;
                    fragmentBufferPost = NULL;
                    //std::cout << "\tgetcontent: counter(fragment) = " << counter << ", block = " << (*j)->block << ", surový obsah po readblock(hash):\n";
                    blockLen = this->readBlock((*j)->hash, (*j)->block, &fragmentBuffer, BLOCK_USABLE_LENGTH, &idByte);
                    //pBytes(fragmentBuffer, 30);
                    //this->unpackData(fragmentBuffer, BLOCK_MAX_LENGTH, &fragmentBufferPost, &blockLen, &idByte);

                    if(!idByteIsDataBlock(idByte)) {
                        // uvolnění alokovaných prostředků
                        if(fragmentBuffer != NULL) {
                            delete [] fragmentBuffer;
                        }
                        /*if(fragmentBufferPost != NULL) {
                            delete [] fragmentBufferPost;
                        }*/

                        continue;
                    }
                    assert((*j)->length <= blockLen);
                    stream.sputn((const char*) fragmentBuffer, (*j)->length);

                    // uvolnění alokovaných prostředků
                    if(fragmentBuffer != NULL) {
                        delete [] fragmentBuffer;
                    }
                    /*if(fragmentBufferPost != NULL) {
                        delete [] fragmentBufferPost;
                    }*/
                } catch(...) {
                    /* při načítání bloku něco nebylo v pořádku - nezkoumáme dál
                     * co se stalo, protože stejně se vždy pokusíme načíst další
                     * kopii bloku, pokud nějaká existuje */
                    if(fragmentBuffer != NULL) {
                        delete fragmentBuffer;
                    }
                    /*if(fragmentBufferPost != NULL) {
                        delete [] fragmentBufferPost;
                    }*/
                    continue;
                }

                // počítadlo fragmentů
                counter++;

                // celková délka streamu
                len += (*j)->length;

                // fragment jsme načetli korektně, není proto důvod zkoumat další
                break;
            }
        }

        bytestream_t* decryptInputBuff = new bytestream_t[len];
        memset(decryptInputBuff, '\0', len);
        stream.sgetn((char*)decryptInputBuff, len);

        //dešifrování obsahu
        this->encryption->decrypt(decryptInputBuff, len, buffer, &blockLen);

        if(blockLen != file->size) {
            std::stringstream ss;
            ss << "Soubor s inode č. " << inode << " v souboru neobsahuje všechny fragmenty (jiné délky)!";

            throw ExceptionRuntimeError(ss.str());
        }

        delete [] decryptInputBuff;

        *length = len;
    }


    size_t hiddenFs::readBlock(hash_t hash, block_number_t block, bytestream_t** buff, size_t length, id_byte_t* idByte) {
        context_t* context = NULL;
        size_t ret;
        std::string filename;

        bytestream_t* buff2 = NULL;

        /* Přes parametr se dozvím pouze délku čistého obsahu, který chci načíst,
         * ale ve skutečnosti potřebuju načíst ještě také CRC a identifikační byte
         * což v součtu musí být naximálně BLOCK_MAX_LENGTH
         **/
        length += sizeof(checksum_t) + sizeof(id_byte_t);

        assert(length <= BLOCK_MAX_LENGTH);

        this->HT->find(hash, &filename);

        try {
            context = this->HT->getContext(hash);
        } catch(...) {
            context = this->createContext(filename);
        }

        buff2 = new bytestream_t[length];
        memset(buff2, '\0', length);
        *buff = NULL;

        try {
            ret = this->readBlock(context, block, buff2, length);
            //std::cout << ", readblock(context): ";
            //pBytes(buff2, 30);
        } catch (ExceptionBlockNotFound& bnf) {
            // uklidit po sobě...
            if(buff2 != NULL) {
                delete buff2;
            }

            // distribuovat výjimku výše, protože zde ještě nevíme jak s ní naložit
            throw bnf;
        } catch (ExceptionBlockNotUsed& bnu) {
            // uklidit po sobě...
            if(buff2 != NULL) {
                delete buff2;
            }

            // distribuovat výjimku výše, protože zde ještě nevíme jak s ní naložit
            throw bnu;
        }

        try {
            this->unpackData(buff2, length, buff, &ret, idByte);

            this->flushContext(context);
            //this->freeContext(context);
        } catch (ExceptionRuntimeError& e) {
            delete buff2;

            throw e;
        }

        return ret;     /// @todo nevadí, že požaduju přečíst 5 a ve skutečnosti načtu 4092bytů?
    }

    void hiddenFs::writeBlock(hash_t hash, block_number_t block, bytestream_t* buff, size_t length, id_byte_t idByte) {
        context_t* context;
        std::string filename;
        bytestream_t* buff2 = NULL;
        size_t length2;

        try {
            this->HT->find(hash, &filename);
            std::cout << "Zapisuju blok=" << block << ", len=" << length << " do: " << filename << std::endl;

            try {
                context = this->HT->getContext(hash);
            } catch(...) {
                context = this->createContext(filename);
            }

            this->packData(buff, length, &buff2, &length2, idByte);

            if(length2 > BLOCK_MAX_LENGTH) {
                throw ExceptionRuntimeError("Pokus o zapsání bloku dat, který je delší, než maximální povolená délka.");
            }

            this->writeBlock(context, block, buff2, length2);

            this->flushContext(context);
            //this->freeContext(context);
        } catch (ExceptionRuntimeError& e) {
            // uklidit po sobě...
            if(buff2 != NULL) {
                delete buff2;
            }

            // distribuovat výjimku výše, protože zde ještě nevíme jak s ní naložit
            std::cerr << "" << e.what() << std::endl;
            throw e;
        }
    }

    void hiddenFs::removeBlock(hash_t hash, block_number_t block) {
        context_t* context;
        std::string filename;

        this->HT->find(hash, &filename);

        try {
            context = this->HT->getContext(hash);
        } catch(...) {
            context = this->createContext(filename);
        }

        this->removeBlock(context, block);

        this->flushContext(context);
        //this->freeContext(context);
    }

    void hiddenFs::findUnusedHash() {
        this->findHashByAuxList(this->HT->auxList_unused, HT_UNUSED_CHUNK);
    };

    void hiddenFs::findPartlyUsedHash() {
        this->findHashByAuxList(this->HT->auxList_partlyUsed, HT_PARTUSED_CHUNK);
    };

    void hiddenFs::findHashByAuxList(std::set<hash_t>& list, const unsigned int limit) {
        context_t* context;
        hashTable::table_t_constiterator ti;

        // nemá smysl hledat v prázdné hashTable
        assert(this->HT->begin() != this->HT->end());

        /* Pokud ještě pořád existuje nějaký soubor/hash, který není
         * vůbec obsazen, nevyhledávat další. */
        if(list.size() > 0) {
            return;
        }

        if(this->HT->firstIndexing) {
            ti = this->HT->begin();
            this->HT->firstIndexing = false;
        } else {
            ti = this->HT->find(this->HT->lastIndexed);
        }

        for(; ti != this->HT->end(); ti++) {
            try {
                this->HT->getContext(ti->first);

                continue;
            } catch(ExceptionRuntimeError& e) {
                context = this->createContext(ti->second.filename);
                this->HT->setContext(ti->first, context);

                /* Pokud jsme už dostatečně naplnili seznam zcela neobsazených
                 * souborů/hash, pro tuto chvíli skončíme s dalším hledáním. */
                if(list.size() >= limit) {
                    break;
                }
            }
        }

        /*
        std::cout << "#.";
        std::cout.flush();
        */

        // poznamenat si do příště, kde jsme skončili
        if(ti != this->HT->end()) {
            this->HT->lastIndexed = ti->first;
        } else {
            // prošli jsme již celou HT, co s tím?
            /**
             * @todo Jak se zachovat? Indexovat znova, nebo to už nechat být?
             * if(contextCache.size() == this->HT->size() return
             * else this->HT->firstIndexing = true
             */
        }
    }
}