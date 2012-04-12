/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */


#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string>

#include "types.h"
#include "contentTable.h"
#include "structTable.h"
#include "hashTable.h"

#define GET_INSTANCE (hiddenFs*) fuse_get_context()->private_data

#define BLOCK_MAX_LENGTH (1 << 12)
//#define BLOCK_LENGH (1 << 10)

using namespace std;
//struct fuse_file_info;

/**
 * Dohlíží nad alokací bloků
 */
/*
class allocatorEngine {
public:

    allocatorEngine(hiddenFs* hfs);



private:
    contentTable* CT;
    structTable* ST;
    hashTable* HT;
    hiddenFs* hfs;
};
 */

class hiddenFs {
public:
    hiddenFs();
    virtual ~hiddenFs();

    typedef int checksum_t;
    struct blockContent {
        checksum_t checksum;
        char* content[BLOCK_MAX_LENGTH];
    };

    /** maximální využití každého fyzického souboru (více virtuálních souborů v rámci jednoho fyzického) */
    static const char ALLOCATOR_SPEED = 1 << 0;

    /** zamezit míchání více bloků virtuálních souborů v rámci jednoho skutečného */
    static const char ALLOCATOR_SECURITY = 1 << 1;

    /** uložení jednoho bloku se provede několikanásobně do různých fyzických souborů */
    static const char ALLOCATOR_REDUNDANCY = 1 << 7;

    /**
     * Nastaví preferovanou strategii pro alokování nových bloků
     * @param strategy typ použité strategie (enum)
     * @param redundancy počet kopií stejného fragmentu
     * @todo přes bitové operace nastavit další vlastnosti (redundancy)
     */
    void allocatorSetStrategy(unsigned char strategy, unsigned int redundancy) throw (ExceptionNotImplemented);

    /**
     * Vrací aktuálně použitou strategii alokace
     * @deprecated
     * @return hodnotu z enumu ALLOC_METHOD
     */
    //char allocatorGetStrategy();

    /**
     * Hlavní výkonná metoda (wrapper nad fuse_main)
     * @param argc počet argumentů z příkazové řádky
     * @param argv pole argumentů z příkazové řádky
     * @return návratová hodnota z FUSE
     */
    int run(int argc, char* argv[]);

    /**
     * true - uchovávání HashTable v metadatech spolu se StructTable atd.
     * false - při každém mount zaindexovat adresář znovu
     * @param enable/disable
     */
    void enableCacheHashTable(bool);

protected:

    /**
     * Vyhledá (=naalokuje) vhodný počet bloků a zapíše do nich obsah jednotlivých fragmentů
     * @param inode inode ukládaného souboru
     * @param buffer obsah souboru
     * @param length délka bufferu
     * @param offset offset
     * @return počet skutečně zapsaných bytů
     */
    size_t allocatorAllocate(inode_t inode, const char* buffer, size_t length, off_t offset);

    /**
     * Skrze parametr block vrací právě jeden volný blok na základě kritéra:
     * Blok se musí nacházet v souboru určeném hashem. Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionBlockNotFound,
     * @param block metoda naplní údaje o nalezeném bloku
     * @param hash nový blok hledat v tomto souboru
     */
    void allocatorFindFreeBlock_sameHash(vBlock* block, T_HASH hash);

    /**
     * Skrze parametr block vrací právě jeden volný blok na základě kritéra:
     * Blok se nesmí nacházet v souboru určeném hashem. Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionBlockNotFound,
     * @param block metoda naplní údaje o nalezeném bloku
     * @param exclude při vyhledávání vhodného bloku přeskočit tento soubor
     */
    void allocatorFindFreeBlock_differentHash(vBlock* block, T_HASH exclude);

    /**
     * Skrze parametr block vrací právě jeden volný blok (jakýkoli volný blok).
     * Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionDiscFull.
     * @param block metoda naplní údaje o nalezeném bloku
     */
    void allocatorFindFreeBlock_any(vBlock* block);

    /**
     * Skrze parametr block vrací právě jeden volný blok. Pokusí se nejprve
     * vyhledat v preferovaném souboru (hash) a pokud neuspěje, vyhledá jakýkoli jiný volný blok
     * @param block metoda naplní údaje o nalezeném bloku
     * @param prefered hledání se spouští nejprve na souboru určeném tímto hash
     */
    void allocatorFindFreeBlock_any(vBlock* block, T_HASH prefered);

    /**
     * Vyhledá volný blok v částečně obsazeném fyzickém souboru.
     * Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionDiscFull.
     * @param block metoda naplní údaje o nalezeném bloku
     */
    void allocatorFindFreeBlock_partUsed(vBlock* block);

    /**
     * Vyhledá volný blok ve fyzickém souboru, který ještě neobsahuje žádný obsazený blok.
     * Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionDiscFull.
     * @param block metoda naplní údaje o nalezeném bloku
     */
    void allocatorFindFreeBlock_unused(vBlock* block);

    /**
     * Naalokuje zdroje pro práci se skutečným souborem - otevření souboru, různé inicializace...
     * @param filename adresa ke skutečnému souboru
     * @return ukazatel na kontext, který může být libovolného typu
     */
    virtual void* createContext(string filename) = 0;

    /**
     * Naplní parametr blocks seznamem čísel bloků, které je možné použít pro nové bloky
     * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
     */
    virtual void listAvaliableBlocks(void* context, std::set<T_BLOCK_NUMBER>* blocks) = 0;

    /**
     * Pokusí se zcela odstranit zadaný blok z fyzického souboru
     * @param filename adresa ke skutečnému souboru, odkud se má blok smazat
     * @param block číslo bloku ke smazání
     */
    virtual void removeBlock(string filename, T_BLOCK_NUMBER block) = 0;

    /**
     * Uvolní naalokované prostředky pro skutečný soubor
     * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
     */
    virtual void freeContext(void* context) = 0;

    /**
     * Zapíše změny do skutečného souboru
     * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
     */
    virtual void flushContext(void* context) = 0;

    /**
     * Provádí zápis bloku do skutečného souboru
     * @param filename adresa ke skutečnému souboru, ze kterého se má blok načíst
     * @param block pořadové číslo bloku v rámci souboru
     * @param buff obsah bloku
     * @param length maximální délka bloku
     * @return skutečně načtená délka bloku
     */
    size_t readBlock(string filename, T_BLOCK_NUMBER block, char* buff, size_t length);

    /**
     * Provádí zápis bloku do skutečného souboru
     * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
     * @param block pořadové číslo bloku v rámci souboru
     * @param buff obsah bloku
     * @param length maximální délka bloku
     * @return skutečně načtená délka bloku
     */
    virtual size_t readBlock(void* context, T_BLOCK_NUMBER block, char* buff, size_t length) = 0;

    /**
     * Provádí zápis bloku do skutečného souboru
     * @param filename adresa ke skutečnému souboru, do kterého se má blok schovat
     * @param block pořadové číslo bloku v rámci souboru
     * @param buff obsah bloku
     * @param length délka zapisovaného bloku
     */
    void writeBlock(string filename, T_BLOCK_NUMBER block, char* buff, size_t length);

    /**
     * Provádí zápis bloku do skutečného souboru
     * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
     * @param block pořadové číslo bloku v rámci souboru
     * @param buff obsah bloku
     * @param length délka zapisovaného bloku
     */
    virtual void writeBlock(void* context, T_BLOCK_NUMBER block, char* buff, size_t length) = 0;

    /**
     * Naplní hash tabulku (mapování cesty skutečného souboru na jednoznačný řetězec)
     * @param filename umístění adresáře určeného jako úložiště (storage)
     */
    virtual void storageRefreshIndex(string filename) = 0;

    /**
     * Přečte kompletní obsah virtuálního souboru ze všech fragmentů
     * @param inode inode souboru
     * @param buffer obsah souboru (metoda sama alokuje obsah)
     * @param length délka souboru v bytech
     */
    void getContent(inode_t inode, char** buffer, size_t* length);

    /**
     * Vypočítá kontrolní součet a vrací výsledek porovnání s referenčním
     * @param content zkoumaná data
     * @param lengthConten délka dat ke kontrole
     * @param checksum referenční kontrolní součet
     * @return blok je/není v pořádku
     */
    virtual bool checkSum(char* content, size_t contentLength, checksum_t checksum);

    /**
     * Provádí dump struktury vBlock do bufferu
     * @param block vstupní blok ke zpracování
     * @param buffer výsledek po dumpu
     * @param length délka bufferu
     */
    void dumpBlock(vBlock* block, char* buffer, size_t length);

    /**
     * Rekonstruuje vBlock z dumpu uloženém v bufferu. Metoda sama alokuje strukturu bloku.
     * @param block rekonstruovaný blok
     * @param buffer vstupní dump bloku
     * @param length délka bufferu
     */
    void reconstructBlock(vBlock** block, char* buffer, size_t length);

    contentTable* CT;
    structTable* ST;
    hashTable* HT;
    unsigned char allocatorFlags;
    unsigned int allocatorRedundancy;

    bool cacheHashTable;

    static int fuse_getattr(const char* path, struct stat* stbuf);
    static int fuse_open(const char* path, struct fuse_file_info* file_i);
    static int fuse_read(const char* path, char* buffer, size_t size, off_t length, struct fuse_file_info* file_i);
    static int fuse_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_i);
    // možná ještě fuse_release
    static int fuse_create(const char* path, mode_t mode, struct fuse_file_info* file_i);
    static int fuse_rename(const char* from, const char* to);
    static int fuse_mkdir(const char* path, mode_t mode);
    static int fuse_rmdir(const char* path);
    static int fuse_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_i);
};

