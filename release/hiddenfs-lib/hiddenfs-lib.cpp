/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 18. březen 2012
 */


#include <cstring>
#include <cryptopp/hex.h>
#include <exception>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include <fuse_opt.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "hiddenfs-lib.h"

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
    bool flagCreateNewFS;
    bool flagFuseDebug;
    bool flagFuseRun;

    // hotovo
    void hiddenFs::allocatorFindFreeBlock_sameHash(vBlock*& block, hash_ascii_t hash) {
        hashTable::table_t_constiterator cit = this->HT->find(hash);

        assert(cit != this->HT->end());

        if(cit->second.context->avaliableBlocks.size() > 0) {
            this->allocatorFillFreeBlockByHash(hash, block);
        } else {
            throw ExceptionBlockNotFound();
        }
    }

    void hiddenFs::allocatorFindFreeBlock_differentHash(vBlock*& block, std::set<hash_ascii_t>& excluded) {
        /// @todo doimplementovat!
        throw ExceptionNotImplemented("hiddenFs::allocatorFindFreeBlock_differentHash");
    }

    // hotovo
    void hiddenFs::allocatorFindFreeBlock_any(vBlock*& block) {
        std::set<hash_ascii_t> empty;
        this->allocatorFindFreeBlock_partUsedPreferred(block, empty);
    }

    // hotovo
    void hiddenFs::allocatorFindFreeBlock_any(vBlock*& block, hash_ascii_t prefered, std::set<hash_ascii_t>& excluded) {
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

        hash_ascii_t hash = *(this->HT->auxList_unused.begin());

        this->allocatorFillFreeBlockByHash(hash, block);
    }

    void hiddenFs::allocatorFindFreeBlock_unusedPreferred(vBlock*& block) {
        try {
            this->allocatorFindFreeBlock_unused(block);
        } catch(ExceptionBlockNotFound) {
            this->allocatorFindFreeBlock_any(block);
        }
    }

    void hiddenFs::allocatorFillFreeBlockByHash(hash_ascii_t hash, vBlock*& block) {
        assert(this->HT->find(hash)->second.context->avaliableBlocks.size() > 0);

        block = new vBlock;

        block->block = *(this->HT->find(hash)->second.context->avaliableBlocks.begin());
        block->fragment = 0;    // libovolná hodnota
        block->hash = hash;
        block->length = 0;      // libovolná hodnota
        block->used = false;
    }

    void hiddenFs::allocatorFindFreeBlock_partUsed(vBlock*& block, std::set<hash_ascii_t>& excluded) {
        /* Taktika pro výběr se zkouší jedna po druhé a pokud některá uspěje,
         * vrátí výsledek a metoda končí. Pořadí:
         * 1) zkusit vybrat některý již z obsazených souborů
         * 2) zkusit některý z volných hash
         * 3) pokud nejsou žádné volné, zkusit zaindexovat další soubory
         * 4) pokud ani tak nebudou existovat žádné volné, vyhazuje se výjimka ExceptionDiscFull
         * **/
        hash_ascii_t hash;

        if(this->HT->auxList_partlyUsed.empty()) {
            this->findPartlyUsedHash();

            if(this->HT->auxList_partlyUsed.empty()) {
                throw ExceptionBlockNotFound();
            }
        }

        hash = *(this->HT->auxList_partlyUsed.begin());

        this->allocatorFillFreeBlockByHash(hash, block);
    }

    void hiddenFs::allocatorFindFreeBlock_partUsedPreferred(vBlock*& block, std::set<hash_ascii_t>& excluded) {
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
        //bytestream_t* buffToWrite;
        vBlock* block;
        std::set<hash_ascii_t> excludedHash;

        contentTable::tableItem cTableItem;
        /*
        std::map<int, std::vector<vBlock*> >::iterator mapit;
        std::vector<vBlock*>::iterator vectit;
        */

        this->ST->findFileByInode(inode, file);

        /// @todo smazat následující řádek!
        //length = file->size;

        fragmentsCount = ceil(1.0 * lengthParam / BLOCK_USABLE_LENGTH);

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
        unsigned int maxReservedLength = round(ALLOCATOR_RESERVED_QUANTITY / 100.0 * lengthParam);

        this->CT->getMetadata(inode, cTableItem);

        //assert(cTableItem.content.size() >= fragmentsCount);
        offset = 0;

        // iterace přes všechny fragmenty souboru

        //for(std::map<fragment_t, std::vector<vBlock*> >::iterator i = cTableItem.content.begin(); i != cTableItem.content.end(); i++) {}
        for(fragment_t f = FRAGMENT_FIRST; f < FRAGMENT_FIRST + fragmentsCount; f++) {
            //std::cout << "Zápis fragmentu #" << f << ", ";
            // výpočet zbývající délky bloku namísto výchozí BLOCK_USABLE_LENGTH, pokud je to nutné
            // záporná hodnota blockLength signalizuje přebytek naalokovaných bloků
            if(offset + BLOCK_USABLE_LENGTH < lengthParam) {
                // Offset ještě není takové délky, abychom museli zapsat jinou (=menší) délku, než je BLOCK_USABLE_LENGTH
                blockLength = BLOCK_USABLE_LENGTH;
            } else {
                /* Zapisujeme buď:
                 * a) poslední blok ze souboru, takže obsah je zbytek do BLOCK_USABLE_LENGTH
                 * b) zapsali jsme již všechny bloky skutečným obsahem; nyní už jen označujeme zbývající fragmenty jako rezervované
                 * **/
                blockLength = lengthParam - offset;
            }

            redundancyFinished = 0;

            // uložení všech redundantních kopii jednoho fragmentu
           // std::cout << "cTableItem.content[f].size() = " << cTableItem.content[f].size() << std::endl;
            for(std::vector<vBlock*>::iterator j = cTableItem.content[f].begin() ; j != cTableItem.content[f].end(); j++) {
                /* Soubor má naalokováno příliš mnoho bloků, takže všechny ty,
                 * do kterých nebyl aktuálně proveden zápis označíme jako rezervované */
                if(offset > lengthParam) {
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
                        std::cout << "";
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

        file->size = lengthParam;

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
        // uvolnění kontextu hashTable
        context_t* context;

        for(hashTable::table_t_constiterator i = this->HT->begin(); i != this->HT->end(); i++) {
            try {
                context = this->HT->getContext(i->first);
                ///@todo po odladění odkomentovat
                std::cout << " -------------------------------------------------- " << (long) context << std::endl;
                //this->freeContext()
                //this->freeContext(context);
            } catch (ExceptionRuntimeError&) {
                continue;
            } catch(...) {
                continue;
            }
        }

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
        stbuf->st_blksize = 1 << 14;

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
        //return size;


        // 2) načíst obsah
        bytestream_t* bufferContent;
        size_t sizeContent;

        hfs->getContent(file->inode, &bufferContent, &sizeContent);

        // 3) naplnění obsahu
        if(offset > sizeContent) {
            size = 0;
        } else {
            // ochrana proti čtení "za soubor"
            if(size + offset > sizeContent) {
                size = sizeContent - offset;
            }

            memcpy(buffer, bufferContent + offset, size);
            delete [] bufferContent;
        }

        return size;
    }

    int hiddenFs::fuse_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_i) {
        inode_t inode;
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;
        std::map<inode_t, std::set<inode_t> >::iterator it;

        /// @todo přidání následujících dvou řádek způsobuje SEGFAULT - jak je to možně?
        filler(buffer, ".", NULL, 0);
        filler(buffer, "..", NULL, 0);

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

        return 0;
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

    int hiddenFs::fuse_truncate(const char* path, off_t length) {
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< \n";
        std::cout << " << <<<<<<<<<" << path << ", length = " << length << "<<<<<<<<<< \n";
        std::cout << " <<  <<<<<<<<<<<<<<<<<<<<<<<< \n";
        throw "hiddenFs::fuse_truncate ještě není implementované!";

        hiddenFs* hfs = GET_INSTANCE;
        bytestream_t* buffer;
        inode_t inode;
        vFile* file;
        size_t size;

        inode = hfs->ST->pathToInode(path);
        hfs->ST->findFileByInode(inode, file);

        hfs->getContent(file->inode, &buffer, &size);
        hfs->allocatorAllocate(file->inode, buffer, length);

        return 0;
    }

    int hiddenFs::fuse_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_i) {
        vFile* file;
        hiddenFs* hfs = GET_INSTANCE;
        size_t sizeWritten;
        size_t sizeNew;
        size_t sizeOriginal;
        size_t sizeNewEncrypted;

        bytestream_t* contentNew;
        bytestream_t* contentOriginal;
        bytestream_t* contentNewEncrypted;

        hfs->ST->pathToInode(path, &file);

        ///@todo odkomentovat!!
        //assert(file->size >= offset);

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
            //sizeNew = offset + file->size;
            //sizeNew = size;
            sizeNew = offset + size;
            contentNew = new bytestream_t[sizeNew];
            memset(contentNew, '\0', sizeNew);

            // do nového obsahu zkopírujeme tu část původního obsahu, která nebude přepsána
            memcpy(contentNew, contentOriginal, sizeOriginal);
            memcpy(contentNew + offset, buffer, size);
        }

        delete [] contentOriginal;

        try {
            hfs->encryption->encrypt(contentNew, sizeNew, &contentNewEncrypted, &sizeNewEncrypted);
            sizeWritten = hfs->allocatorAllocate(file->inode, contentNewEncrypted, sizeNewEncrypted);

            delete [] contentNew;
            delete [] contentNewEncrypted;

        } catch(ExceptionDiscFull) {
            if(contentNew != NULL) {
                delete [] contentNew;
            }

            if(contentNewEncrypted != NULL) {
                delete [] contentNewEncrypted;
            }

            return -ENOSPC;
        } catch(...) {
            /// @todo ošetřit i jiné druhy chyb?
        }

        return size;
    }

    static int fuse_opt_processing(void* data, const char* arg, int key, fuse_args* outargs) {
        //struct options *hopts = (struct options*) data;

        switch(key) {
            case hiddenFs::KEY_CREATE : {
                HiddenFS::flagCreateNewFS = true;
                break;
            }
            case hiddenFs::KEY_FUSE_DEBUG : {
                HiddenFS::flagFuseDebug = true;
                fuse_opt_add_arg(outargs, "-d");
                break;
            }
            case hiddenFs::KEY_HELP : {
                HiddenFS::flagFuseRun = false;

                return 1;
            }
        }

        return 0;
    }

    void hiddenFs::setConsoleEcho(bool state) {
        struct termios tty;
        int fd = STDIN_FILENO;

        tcgetattr(fd, &tty);

        if(state) {
            tty.c_lflag |= ECHO;
        } else {
            tty.c_lflag &= ~ECHO;
        }

        tcsetattr(fd, TCSANOW, &tty);
    }

    int hiddenFs::run(int argc, char* argv[]) {
        std::string path;
        path.clear();

        HiddenFS::flagCreateNewFS = false;
        HiddenFS::flagFuseRun = true;
        HiddenFS::flagFuseDebug = false;

        #define HFS_OPT_KEY(t, p, v) { t, offsetof(struct optionsStruct, p), v }

        static struct fuse_opt hfs_opts[] = {
            HFS_OPT_KEY("--storage=%s", storagePath, 0),
            HFS_OPT_KEY("-s %s", storagePath, 0),

            HFS_OPT_KEY("-p %s", password, 0),
            HFS_OPT_KEY("--password=%s", password, 0),

            HFS_OPT_KEY("-m %s", mountpoint, 0),

            FUSE_OPT_KEY("-c",             KEY_CREATE),
            FUSE_OPT_KEY("--create",       KEY_CREATE),
            FUSE_OPT_KEY("-d",       KEY_FUSE_DEBUG),
            FUSE_OPT_KEY("-h",             KEY_HELP),
            FUSE_OPT_KEY("--help",         KEY_HELP),
            FUSE_OPT_END
        };

        struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
        memset(&(this->options), 0, sizeof(struct optionsStruct));

        if(fuse_opt_parse(&args, &(this->options), hfs_opts, fuse_opt_processing) == -1) {
            return EXIT_FAILURE;
        }

        // zakázat spouštění ve více vláknech
        fuse_opt_add_arg(&args, "-s");

        // kontrola existence umístění úložiště
        if(this->options.storagePath == NULL) {
            std::cerr << "Nebyla zadána cesta k adresáři úložiště.\n";
            this->usage(std::cerr);
            return EXIT_FAILURE;
        }

        if(this->options.mountpoint == NULL) {
            std::cerr << "Nebyla zadána cesta k přípojnému bodu (mountpoint).\n";
            this->usage(std::cerr);
            return EXIT_FAILURE;
        }

        fuse_opt_add_arg(&args, options.mountpoint);
        path = this->options.storagePath;

        try {
            this->storageRefreshIndex(path);
        } catch(ExceptionRuntimeError& e) {
            std::cerr << e.what();
            std::cerr << "\n";

            return EXIT_FAILURE;
        }

        this->HT->print();

        if(this->HT->size() == 0) {
            std::cerr << "Úložiště neobsahuje žádné dostupné soubory.\n";

            return EXIT_FAILURE;
        }


        // 1) načteme heslo od uživatele
        char pass1[PASSWORD_MAX_LENGTH];
        char pass2[PASSWORD_MAX_LENGTH];

        if(HiddenFS::flagCreateNewFS && options.password == NULL) {
            // zakládáme nový systém souborů, heslo získáváme z stdin
            bool success;

            std::cout << "Vytváření nového souborového systému\n" << std::flush;
            std::cout << "Poznámka: Maximální délka uchovávaného hesla je " << PASSWORD_MAX_LENGTH << " znaků.\n" << std::flush;

            success = false;

            for(unsigned int i = 0; i < PASSWORD_MAX_ATTEMPTS && !success; i++) {
                memset(pass1, '\0', PASSWORD_MAX_LENGTH);
                memset(pass2, '\0', PASSWORD_MAX_LENGTH);

                this->setConsoleEcho(false);

                std::cout << "Zadejte nové heslo: " << std::flush;
                std::cin.getline(pass1, PASSWORD_MAX_LENGTH);
                std::cout << std::endl;

                std::cout << "Zadejte znovu stejné heslo pro kontrolu: " << std::flush;
                std::cin.getline(pass2, PASSWORD_MAX_LENGTH);
                std::cout << std::endl;

                if(strncmp(pass1, pass2, PASSWORD_MAX_LENGTH) == 0) {
                    success = true;
                    this->encryption->setKey((bytestream_t*)pass1, PASSWORD_MAX_LENGTH);
                    this->setConsoleEcho(true);

                    break;
                } else {
                    std::cout << "Zadaná hesla nebyla zadána stejná, opakujte zadání.\n" << std::flush;
                }
            }

            if(!success) {
                this->setConsoleEcho(true);
                return EXIT_FAILURE;
            }
        } else if(options.password == NULL) {
            // použijeme stávající superblock, heslo z stdin
            memset(pass1, '\0', PASSWORD_MAX_LENGTH);
            //std::cout << "Zadejte heslo: " << std::flush;
            this->setConsoleEcho(false);
            std::cin.getline(pass1, PASSWORD_MAX_LENGTH);
            this->setConsoleEcho(true);

            ///@todo najít FS, který se dá dešifrovat tímto heslem
            this->encryption->setKey((bytestream_t*)pass1, PASSWORD_MAX_LENGTH);
        } else {
            // použijeme stávající superblock, heslo z parametru příkazové řádky
            memset(pass1, '\0', PASSWORD_MAX_LENGTH);
            strcpy(pass1, options.password);
            this->encryption->setKey((bytestream_t*)pass1, PASSWORD_MAX_LENGTH);
        }

        // Zadali jsme nesprávné heslo, konec.
        if(!HiddenFS::flagCreateNewFS && !this->SB->hasKnownItems()) {
            std::cout << "Nesprávné heslo.\n";

            return EXIT_FAILURE;
        }

        vBlock* sbBlock;
        bytestream_t sbBuffer[BLOCK_USABLE_LENGTH];
        memset(sbBuffer, '\0', sizeof(sbBuffer));

        this->findPartlyUsedHash();
        this->findUnusedHash();

        id_byte_t idByte;
        bytestream_t* bufferBlock;
        size_t bufferMaxLen = BLOCK_USABLE_LENGTH;


        // 2) pokusíme se najít všechny kopie superbloků
        for(hashTable::table_t_constiterator i = this->HT->begin(); i != this->HT->end(); i++) {
            std::cout << "V souboru: " << i->second.filename << " hledám superblock... ";
            try {
                this->readBlock(i->first, FIRST_BLOCK_NO, &bufferBlock, bufferMaxLen, &idByte);

                if(! idByteIsSuperBlock(idByte)) {
                    std::cout << "nemá idByte superbloku (" << (int) idByte << ")\n";
                    continue;
                }

                std::cout << "našel jsem! :-) :-) :-) :-)";

                if(!this->SB->isLoaded()) {
                    // načtu obsah SB do vnitřních struktur

                    std::cout << " SB ještě nebyl naplněn - toto je první plnění.\n";
                    this->SB->deserialize(bufferBlock, bufferMaxLen);

                    this->tableRestoreFromSB();
                }

                std::cout << "Našel jsem už všechny kopie? " << (this->superBlockLocations.size() >= SUPERBLOCK_REDUNDANCY_AMOUNT) << std::endl;
                this->superBlockLocations.insert(i->first);

                if(this->superBlockLocations.size() >= SUPERBLOCK_REDUNDANCY_AMOUNT) {
                    // našel jsem všechny kopie superbloků, nemá proto smysl hledat dále
                    break;
                }
            } catch (ExceptionBlockNotUsed&) {
                std::cout << " ExceptionBlockNotUsed... continue.\n";
                continue;
            } catch (ExceptionBlockNotFound&) {
                std::cout << " ExceptionBlockNotFound... continue.\n";
                continue;
            } catch (std::exception& e) {
                std::cout << " Exception" << e.what() << "... continue.\n";
                continue;
            }

            // uvolnění prostředků
            /*
            hashTable::table_t_constiterator htci = this->HT->find(i->first);
            if(htci != this->HT->end()) {
                if(htci->second.context != NULL) {
                    this->freeContext(htci->second.context);
                    this->HT->clearContext(i->first);
                }
            }
            */
        }

        // v úložišti dosud neexistuje jediná kopie superbloku
        if(HiddenFS::flagCreateNewFS && this->superBlockLocations.empty()) {
            if(this->superBlockLocations.empty()) {
                // vybrat první neobsazené bloky a zapsat do nich libovolný obsah - bu
                for(unsigned int i = 0; i < SUPERBLOCK_REDUNDANCY_AMOUNT; i++) {
                    this->allocatorFindFreeBlock_unused(sbBlock);
                    assert(sbBlock->block == FIRST_BLOCK_NO);
                    idByte = idByteGenSuperBlock();
                    this->writeBlock(sbBlock->hash, sbBlock->block, sbBuffer, sizeof(sbBuffer), idByte);
                    this->superBlockLocations.insert(sbBlock->hash);
                }
            }
        }

        if(this->superBlockLocations.empty()) {
            std::cout << "V úložišti nejsou dostatečně volné jednotky pro vytvoření superbloku.\n";

            return EXIT_FAILURE;
        }

        for(std::set<hash_ascii_t>::iterator i = this->superBlockLocations.begin(); i != this->superBlockLocations.end(); i++) {
            std::cout << "Známý superblok: " << *i << std::endl;
        }

        // způsobí případné douložení chybějících kopií
        //this->superBlockSave();


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
        //fsOperations.truncate  = this->fuse_truncate;

        /*
        try {
        */
        // private data = pointer to actual instance
        int fuse_ret = EXIT_SUCCESS;
        if(HiddenFS::flagFuseRun) {
            fuse_ret = fuse_main(args.argc, args.argv, &fsOperations, this);
        } else {
            fuse_ret = EXIT_FAILURE;
        }

        this->superBlockSave();
        /*
        } catch(...) {
            std::cout << "fuse_main způsobilo vyhození výjimky\n".
        }
        */

        return fuse_ret;
    }

    void hiddenFs::tablesSaveToSB() {
        // uložit obsah všech tabulek (CT, ST)
        chainList_t chainCT;
        chainList_t chainST;

        // převod tabulek do podoby řetězce entit
        this->CT->serialize(&chainCT);
        this->ST->serialize(&chainST);

        // pole pro uskladnění všech kopií řetězů - podstatný modifikátor 'static'
        static std::set<vBlock*> vectCT;
        static std::set<vBlock*> vectST;

        if(this->SB->isLoaded()) {
            this->SB->readKnownItems(vectCT, superBlock::TABLE_CONTENT_TABLE);
            this->SB->readKnownItems(vectST, superBlock::TABLE_STRUCT_TABLE);
        }

        this->tableSave(chainCT, vectCT);
        this->tableSave(chainST, vectST);

        this->SB->clearKnownItems();
        this->SB->addKnownItem(vectCT, superBlock::TABLE_CONTENT_TABLE);
        this->SB->addKnownItem(vectST, superBlock::TABLE_STRUCT_TABLE);

        this->SB->setLoaded();
    }



    void hiddenFs::tableSave(chainList_t& content, std::set<vBlock*>& copies){
        vBlock* block = NULL;

        if(copies.size() < TABLES_REDUNDANCY_AMOUNT) {
            /* Aktuální vektor má málo redundantních kopií, proto doalokujeme
             * zbývající do počtu */
            unsigned int count = copies.size();

            for(unsigned int i = count; i < TABLES_REDUNDANCY_AMOUNT; i++) {
                try {
                    std::cout << "hiddenFs::tableSave zkouším doalokovávat kopii #" << i << " z " << TABLES_REDUNDANCY_AMOUNT << std::endl;
                    block = this->chainListCompleteSave(content, NULL);
                    std::cout << "hiddenFs::tableSave, chainListCompleteSave do " << print_vBlock(block) << std::endl;
                    copies.insert(block);
                } catch(ExceptionDiscFull&) {
                    break;
                }
            }
        } else {
            /* Redundantních kopií je na vstupu dostatek, */
            assert(copies.size() == TABLES_REDUNDANCY_AMOUNT);

            for(std::set<vBlock*>::iterator i = copies.begin(); i != copies.end(); i++) {
                try {
                    std::cout << "ukládám do známé kopie #??? z " << TABLES_REDUNDANCY_AMOUNT << std::endl;
                    this->chainListCompleteSave(content, *i);
                } catch(ExceptionDiscFull&) {
                    break;
                }
            }
        }
    }

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


    size_t hiddenFs::readBlock(hash_ascii_t hash, block_number_t block, bytestream_t** buff, size_t length, id_byte_t* idByte) {
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

        return ret;     /// @todo nevadí, že požaduju přečíst 5 a ve skutečnosti načtu 4091bytů?
    }

    void hiddenFs::writeBlock(hash_ascii_t hash, block_number_t block, bytestream_t* buff, size_t length, id_byte_t idByte) {
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
            this->HT->auxUse(hash, block);

            delete [] buff2;
            this->flushContext(context);
            //this->freeContext(context);


        } catch (ExceptionRuntimeError& e) {
            // uklidit po sobě...
            if(buff2 != NULL) {
                delete [] buff2;
            }

            // distribuovat výjimku výše, protože zde ještě nevíme jak s ní naložit
            std::cerr << "" << e.what() << std::endl;
            throw e;
        }
    }

    void hiddenFs::removeBlock(hash_ascii_t hash, block_number_t block) {
        context_t* context;
        std::string filename;

        this->HT->find(hash, &filename);

        try {
            context = this->HT->getContext(hash);
        } catch(...) {
            context = this->createContext(filename);
        }

        this->removeBlock(context, block);
        this->HT->auxFree(hash, block);

        this->flushContext(context);
        //this->freeContext(context);
    }

    void hiddenFs::findUnusedHash() {
        this->findHashByAuxList(this->HT->auxList_unused, HT_UNUSED_CHUNK);
    };

    void hiddenFs::findPartlyUsedHash() {
        this->findHashByAuxList(this->HT->auxList_partlyUsed, HT_PARTUSED_CHUNK);
    };

    void hiddenFs::findHashByAuxList(std::set<hash_ascii_t>& list, const unsigned int limit) {
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

    void hiddenFs::chainListRestore(vBlock* location, vBlock*& next, chainList_t& chain) {
        bytestream_t* buff;
        bytestream_t* buffRaw;
        bytestream_t buffEmpty[SIZEOF_vBlock];
        size_t buffLen;
        size_t buffLenRaw = BLOCK_USABLE_LENGTH;
        size_t offset;
        chain_count_t count, i;
        id_byte_t idByte;
        static unsigned int poc = 0;

        /** jeden článek řetězu */
        chainItem link;
        memset(buffEmpty, '\0', SIZEOF_vBlock);

        try {
            this->readBlock(location->hash, location->block, &buffRaw, buffLenRaw, &idByte);
            assert(idByteIsDataBlock(idByte));
            this->encryption->decrypt(buffRaw, buffLenRaw, &buff, &buffLen);
        } catch (...) {
            throw ExceptionRuntimeError("hiddenFs::chainListRestore - načtení bloku s entitami nebylo úspěšné");
        }
        offset = 0;

        // rekonstrukce složky 'count'
        memcpy(&count, buff + offset, sizeof(count));
        offset += sizeof(count);
        //pBytes(buff, 100);

        // rekonstrukce složky 'vBlock'
        /* Poslední článek řetězu je identifikován jako oblast znaků '\0' v délce
         * serializované struktury vBlock */

        std::cout << "chainListRestore počítadlo = " << poc << std::endl;
        poc++;
        /*
        if(poc > 100) {
            assert(false);
        }
        */
        //pBytes(buff + offset, SIZEOF_vBlock);

        if(memcmp(buff + offset, buffEmpty, SIZEOF_vBlock) == 0) {
            next = NULL;
        } else {
            unserialize_vBlock(buff + offset, SIZEOF_vBlock, &next);
        }
        offset += SIZEOF_vBlock;

        std::cout << "act  = " << print_vBlock(location) << std::endl;
        std::cout << "next = ";
        if(next != NULL) std::cout << print_vBlock(next);
        std::cout.flush();

        // postupná rekonstrukce všech obsažených článků
        for(i = 0; i < count; i++) {
            memcpy(&(link.length), buff + offset, sizeof(link.length));
            offset += sizeof(link.length);

            assert(link.length < BLOCK_USABLE_LENGTH);

            link.content = new bytestream_t[link.length];

            memcpy(link.content, buff + offset, link.length);
            offset += link.length;

            chain.push_back(link);
        }

        assert(offset <= buffLen);
    }

    void hiddenFs::chainListCompleteRestore(vBlock* first, chainList_t& chain) {
        vBlock* act;
        vBlock* next;

        act = first;

        try {
            while(true) {
                this->chainListRestore(act, next, chain);

                if(next == NULL) {
                    break;
                }

                act = next;
            }
        } catch(...) {
            throw ExceptionRuntimeError("hiddenFs::chainListCompleteRestore řetěz nebyl načten kompleně");
        }
    }

    void hiddenFs::chainListSave(vBlock* location, vBlock* next, const chainList_t& chain) {
        bytestream_t buff[BLOCK_USABLE_LENGTH];
        bytestream_t* vBlockBuff;
        bytestream_t* encBuffer;
        size_t vBlockBuffLen;
        size_t encBufferLen;
        size_t offset = 0;
        chain_count_t count = chain.size();
        id_byte_t idByte;
        unsigned int countEntits = 0;

        memset(buff, '\0', sizeof(buff));

        // zápis složky 'count'
        memcpy(buff + offset, &count, sizeof(count));
        offset += sizeof(count);

        /* zápis složky 'next', poslední článek řetězu se ukládá jako souvislá
         *  oblast znaku '\0' v délce serializované struktury vBlock */
        if(next == NULL) {
            memset(buff + offset, '\0', SIZEOF_vBlock);
        } else {
            serialize_vBlock(next, &vBlockBuff, &vBlockBuffLen);
            assert(vBlockBuffLen == SIZEOF_vBlock);
            memcpy(buff + offset, vBlockBuff, vBlockBuffLen);

            delete [] vBlockBuff;
        }
        offset += SIZEOF_vBlock;

        // zápis všech entit
        for(chainList_t::const_iterator i = chain.begin(); i != chain.end(); i++) {
            // nejdříve délka entity
            memcpy(buff + offset, &(i->length), sizeof(i->length));
            offset += sizeof(i->length);

            // ... a následuje samotný obsah entity
            memcpy(buff + offset, i->content, i->length);
            offset += i->length;

            countEntits++;
        }

        assert(countEntits == count);

        // Nesmíme dovolit zapsat delší blok dat, než se vejde do jednoho bloku
        assert(offset <= BLOCK_USABLE_LENGTH);

        idByte = idByteGenDataBlock();
        this->encryption->encrypt(buff, BLOCK_USABLE_LENGTH, &encBuffer, &encBufferLen);

        this->writeBlock(location->hash, location->block, encBuffer, encBufferLen, idByte);
    }

    void hiddenFs::chainListAllocate(const hash_ascii_t& hash, vBlock*& block) {
        /** Pokus o vyhledání dalšího bloku pro uložení aktuální části řetězu.
         * Prohledávání se provádí podle následujícho scénáře, výběr končí
         * po prvním nalezeném bloku:
         * 1) blok ve stejném fyzickém souboru
         * 2) blok v dosud neobsazeném souboru (kvůli snížení fragmentace)
         * 3) vyhledání libovolného rezervovaného bloku (nezávisle na inode souboru,
         * kterému patří) a jeho označení za volný*/
        inode_t robbedInode;

        try {
            this->allocatorFindFreeBlock_sameHash(block, hash);
        } catch(ExceptionBlockNotFound&) {
            try {
                this->allocatorFindFreeBlock_unusedPreferred(block);
            } catch(ExceptionDiscFull&) {
                /* Pokud i allocatorStealReservedBlock() vyhodí výjimku
                 * ExceptionDiscFull, nelze uložit celý řetěz */
                this->allocatorStealReservedBlock(robbedInode, block);
            }
        }

        block->used = true;

        // block->length = BLOCK_USABLE_LENGTH;
        block->length = 101;
    }

    vBlock* hiddenFs::chainListCompleteSave(const chainList_t& chain, vBlock* firstChain) {
        vBlock* firstBlock = NULL;

        /** Umístění naposledy zapsaného bloku */
        vBlock* prevBlock = NULL;

        /** Umístění aktuálně ukládaného bloku */
        vBlock* actBlock = NULL;

        bool newBlock;
        size_t actSize;
        chainList_t actList;
        chainList_t prevList;
        hash_ascii_t hash;

        const size_t newBlockHeaderLen = sizeof(chain_count_t) + SIZEOF_vBlock;
        size_t maxLength = BLOCK_USABLE_LENGTH;

        /** @todo následující řádek po odladění smazat! */
        maxLength = 100;

        if(firstChain != NULL) {
            firstBlock = firstChain;
        } else {
            /* Pokud allocatorFindFreeBlock_unusedPreferred vyhodí výjimku
             * ExceptionDiscFull, je to pořádku ji zde ještě nezachytit,
             * protože ošetření tohoto stavu náleží metodě, která chainListSave volá. */
            this->allocatorFindFreeBlock_unusedPreferred(firstBlock);
        }

        /** @todo zkontrolovat podmínky kolem prevBlock */
        prevBlock = firstBlock;
        actBlock = firstBlock;
        newBlock = true;
        actSize = 0;

        for(chainList_t::const_iterator i = chain.begin(); i != chain.end(); i++) {
            if(newBlock) {
                actSize += newBlockHeaderLen;
            }

            if(actSize + sizeof(i->length) + i->length < maxLength) {
                newBlock = false;
                actList.push_back(*i);
                actSize += sizeof(i->length) + i->length;
            } else {
                /* Přidání další entity by způsobilo 'přetečení' délky bloku,
                 * proto se pokusím "naprázdno" uložit aktuální seznam a až budu úspěšný,
                 * přečtu předcházející kus, změním hodnotu 'next' a uložím jej. */

                std::cout << " === zápis aktuálního bloku ===" << std::endl;

                while(1) {
                    // V průběhu iterací while může dojít k ExceptionDiscFull, což je problém a
                    // 1) uložení aktuálního bloku (následující ještě neznáme, proto NULL)
                    try {
                            std::cout << "ukládám do actBlock = " << actBlock->block << ", next = " << "NULL, \\/" << std::endl;
                        this->chainListSave(actBlock, NULL, actList);
                    } catch(ExceptionDiscFull& e) {
                        throw e;
                    } catch(...) {
                        // vyhledej jiný actBlock
                            std::cout << "... zápis se nepodařil, zkouším další..." << std::endl;
                        hash = actBlock->hash;
                        this->chainListAllocate(hash, actBlock);

                        /** @todo pozor na zacyklení */
                        continue;
                    }

                    break;
                }

                /** @todo Jak se zachovat, pokud se nepodaří nový zápis do předešlého bloku? */
                /* Nové uložení předešlé části řetězu, protože už známe umístění
                 * následující části, netýká se prvního bloku. */

                std::cout << std::endl << " === aktualizace dat v předešlém bloku ===" << std::endl;
                std::cout << "možná budu ukládat do prevBlock=" << prevBlock->block << "], nastavuju jeho next na " << actBlock->block << std::endl;

                if(actBlock != prevBlock) {
                    std::cout << "... jo, uložím to." << std::endl;
                    this->chainListSave(prevBlock, actBlock, prevList);
                } else {
                    std::cout << "actBlock == prevBlock... to je na assert!!" << std::endl;
                }


                // vrátíme jeden blok načtený navíc zpět
                // může způsobit zacyklení, ale pouze pokud se do jedné části nevejde ani jedna entita
                i--;

                newBlock = true;
                actSize = 0;
                prevList = actList;
                prevBlock = actBlock;

                actList.clear();

                hash = actBlock->hash;
                    std::cout << "   před alokaci: prevBlock = " << prevBlock->block << "], actBlock = " << actBlock->block << std::endl;
                this->chainListAllocate(hash, actBlock);
                    std::cout << "     po alokaci: prevBlock = " << prevBlock->block << "], actBlock = " << actBlock->block << std::endl << std::endl;
            }
        }

        // ruční douložení posledního článku
        while(1) {
            // V průběhu iterací while může dojít k ExceptionDiscFull, což je problém a
            // 1) uložení aktuálního bloku (následující ještě neznáme, proto NULL)
            try {
                this->chainListSave(actBlock, NULL, actList);
            } catch(ExceptionDiscFull& e) {
                throw e;
            } catch(...) {
                // vyhledej jiný actBlock
                hash = actBlock->hash;
                this->chainListAllocate(hash, actBlock);

                /** @todo pozor na zacyklení */
                continue;
            }

            break;
        }

        // nové uložení předešlé části řetězu, protože už známe umístění následující části
        /** @todo Jak se zachovat, pokud se nepodaří nový zápis do předešlého bloku? */
            std::cout << "#ukládám do prevBlock[" << prevBlock->block << "], nastavuju next na " << actBlock->block << std::endl;
        if(actBlock != prevBlock) {
            this->chainListSave(prevBlock, actBlock, prevList);
        }

        return firstBlock;
    }

    void hiddenFs::superBlockSave() {
        bytestream_t* sbSerializedBuffer;
        bytestream_t* readBuffer;
        size_t sbSerializedBufferLen = 0;
        //size_t readBufferLen = 0;
        id_byte_t idByte;

        this->tablesSaveToSB();

        this->SB->serialize(&sbSerializedBuffer, &sbSerializedBufferLen);

        std::cout << "\n-- hiddenFs::superBlockSave --\n";
        std::cout << "zkouším uložit do známých superbloků\n";
        for(std::set<hash_ascii_t>::iterator i = this->superBlockLocations.begin(); i != this->superBlockLocations.end(); i++) {
            idByte = idByteGenSuperBlock();
            try {
                std::cout << " # " << *i << "\n";
                this->writeBlock(*i, FIRST_BLOCK_NO, sbSerializedBuffer, sbSerializedBufferLen, idByte);

                if(sbSerializedBufferLen > BLOCK_MAX_LENGTH) {
                    std::cerr << "Superblok se nevejde do jediného bloku! (3)\n";
                    return;
                }
            } catch(...) {
                std::cout << "... to výše bylo neúspěšné, mažu prvek.\n";
                this->superBlockLocations.erase(i);
            }
        }

        /* Super block zatím neexistuje, bude nutné jej vytvořit, nebo je k dispozici
         * málo kopií, takže se je pokusíme doalokovat. */
        assert(this->SB->isLoaded());
        if(this->superBlockLocations.size() < SUPERBLOCK_REDUNDANCY_AMOUNT) {
            // hlavní záznam uložit do prvního souboru (prvního z pohledu seřazených hash
            for(hashTable::table_t_constiterator i = this->HT->begin();
                    i != this->HT->end() && this->superBlockLocations.size() < SUPERBLOCK_REDUNDANCY_AMOUNT;
                    i++) {
                try {
                    this->readBlock(i->first, FIRST_BLOCK_NO, &readBuffer, BLOCK_USABLE_LENGTH, &idByte);
                    if(!idByteIsSuperBlock(idByte)) {
                        std::cout << i->first << " nemá příznak SuperBloku (1)\n";
                        continue;
                    }

                    delete [] readBuffer;

                    // našli jsme superblok, který ještě není 'zaindexovaný' v seznamu superBlockLocations
                    if(this->superBlockLocations.find(i->first) == this->superBlockLocations.end()) {
                        std::cout << "do " << i->first << " jsem ještě nezapisoval, ale je vhodný (2)\n";
                        this->writeBlock(i->first, FIRST_BLOCK_NO, sbSerializedBuffer, sbSerializedBufferLen, idByte);
                        this->superBlockLocations.insert(i->first);
                    }
                } catch (ExceptionBlockNotUsed&) {
                    std::cout << "do " << i->first << " je zcela prázdný (4)\n";
                    this->writeBlock(i->first, FIRST_BLOCK_NO, sbSerializedBuffer, sbSerializedBufferLen, idByte);
                    this->superBlockLocations.insert(i->first);
                }
            }
        }

        delete [] sbSerializedBuffer;
    }
}