/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 14. březen 2012
 */


#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string>

#include "common.h"
#include "contentTable.h"
#include "hashTable.h"
#include "structTable.h"
#include "superBlock.h"
#include "types.h"

#define GET_INSTANCE (hiddenFs*) fuse_get_context()->private_data

//#define BLOCK_LENGH (1 << 10)

namespace HiddenFS {

    class hiddenFs {
    public:
        /**
         * Umožňuje nastavit vlastní implementaci šifrování
         * @param instance instance objektu implementující rozhraní IEncryption
         */
        hiddenFs(IEncryption* instance);
        virtual ~hiddenFs();

        /** datový typ kontrolního součtu */
        typedef CRC::CRC_t checksum_t;

        typedef bytestream_t blockContent[BLOCK_MAX_LENGTH];
        static const size_t BLOCK_USABLE_LENGTH = BLOCK_MAX_LENGTH - sizeof(checksum_t) - sizeof(id_byte_t);

        block_number_t FIRST_BLOCK_NO;

        /** maximální využití každého fyzického souboru (více virtuálních souborů v rámci jednoho fyzického) */
        static const char ALLOCATOR_SPEED = 1 << 0;

        /** zamezit míchání více bloků virtuálních souborů v rámci jednoho skutečného */
        static const char ALLOCATOR_SECURITY = 1 << 1;

        /** uložení jednoho bloku se provede několikanásobně do různých fyzických souborů */
        static const char ALLOCATOR_REDUNDANCY = 1 << 7;

        //smartConst<size_t> hash_t_sizeof;


        /**
         * Nastaví preferovanou strategii pro alokování nových bloků
         * @param strategy typ použité strategie (enum)
         * @param redundancy počet kopií stejného fragmentu
         * @todo přes bitové operace nastavit další vlastnosti (redundancy)
         */
        void allocatorSetStrategy(unsigned char strategy, unsigned int redundancy);

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
        contentTable* CT;
        structTable* ST;
        hashTable* HT;
        superBlock* SB;
        unsigned char allocatorFlags;

        /** množství duplikace dat */
        unsigned int allocatorRedundancy;

        /** instance objektu pro výpočet kontrolního součtu */
        CRC* checksum;

        IEncryption* encryption;

        /**
         * @deprecated
         * Při true se knihovna pokusí nejdříve načíst hashTable ze superbloku,
         * ale pokud selže, provede se indexace zcela nová. Je to začarovaný kruh:
         * Pro nalezení cacheHT je třeba znát pozici SB. Pro nalezení SB je nuzné znát obsah HT.
         *  */
        bool cacheHashTable;

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
        void allocatorFindFreeBlock_sameHash(vBlock* block, hash_t hash);

        /**
         * Skrze parametr block vrací právě jeden volný blok na základě kritéra:
         * Blok se nesmí nacházet v souboru určeném hashem. Pokud takový blok neexistuje, vyhazuje se výjimka ExceptionBlockNotFound,
         * @param block metoda naplní údaje o nalezeném bloku
         * @param exclude při vyhledávání vhodného bloku přeskočit tento soubor
         */
        void allocatorFindFreeBlock_differentHash(vBlock* block, hash_t exclude);

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
        void allocatorFindFreeBlock_any(vBlock* block, hash_t prefered);

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
        virtual void* createContext(std::string filename) = 0;

        /**
         * Naplní parametr blocks seznamem čísel bloků, které je možné použít pro nové bloky
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         */
        virtual void listAvaliableBlocks(void* context, std::set<block_number_t>* blocks) const = 0;

        /**
         * Pokusí se zcela odstranit zadaný blok z fyzického souboru
         * @param hash jednoznačná identifikace souboru, ve kterém se nachází
         * blok pro mazání
         * @param block číslo bloku ke smazání
         */
        void removeBlock(hash_t hash, block_number_t block);

        /**
         * Pokusí se zcela odstranit zadaný blok z fyzického souboru
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         * @param block číslo bloku ke smazání
         */
        virtual void removeBlock(void* context, block_number_t block) = 0;

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
         * Zabezpečuje přečtení bloku obsahující fragment souboru
         * @param hash identifikace souboru
         * @param block pořadové číslo bloku v rámci skutečného souboru
         * @param buff užitný obsah bloku, metoda sama alokuje dostatečnou délku
         * @param length maximální délka bloku, která se má načíst
         * @param idByte metoda naplní tento parametr hodnotou rozlišovacího byte
         * @return počet skutečně naštených dat
         */
        size_t readBlock(hash_t hash, block_number_t block, bytestream_t** buff, size_t length, id_byte_t* idByte);

        /**
         * Provádí zápis bloku do skutečného souboru,
         * jeho implementace je na uživateli knihovny
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         * @param block pořadové číslo bloku v rámci souboru
         * @param buff obsah bloku
         * @param length maximální délka bloku
         * @return skutečně načtená délka bloku
         */
        virtual size_t readBlock(void* context, block_number_t block, bytestream_t* buff, size_t length) const = 0;

        /**
         * Provádí zápis bloku do skutečného souboru
         * @param hash hash skutečného souboru, do kterého se má blok schovat
         * @param block pořadové číslo bloku v rámci souboru
         * @param buff obsah bloku
         * @param length délka zapisovaného bloku
         */
        void writeBlock(hash_t hash, block_number_t block, bytestream_t* buff, size_t length, id_byte_t idByte);

        /**
         * Provádí zápis bloku do skutečného souboru,
         * jeho implementace je na uživateli knihovny
         * @param context ukazatel na kontext, v rámci kterého se pracuje s právě jedním skutečným souborem
         * @param block pořadové číslo bloku v rámci souboru
         * @param buff obsah bloku
         * @param length délka zapisovaného bloku
         */
        virtual void writeBlock(void* context, block_number_t block, bytestream_t* buff, size_t length) = 0;

        /**
         * Naplní hash tabulku (mapování cesty skutečného souboru na jednoznačný řetězec)
         * @param filename umístění adresáře určeného jako úložiště (storage)
         */
        virtual void storageRefreshIndex(std::string filename) = 0;

        /**
         * Přečte kompletní obsah virtuálního souboru ze všech fragmentů
         * @param inode inode souboru
         * @param buffer obsah souboru (metoda sama alokuje obsah)
         * @param length délka souboru v bytech
         */
        void getContent(inode_t inode, bytestream_t** buffer, size_t* length);

        /**
         * Provede "zabalení" bloku dat a výpočet CRC
         * @param input vstupní buffer
         * @param inputSize délka vstupního bufferu
         * @param output výstupní buffer, metoda sama alokuje potřebnou délku
         * @param outputSize délka výstupního bufferu
         * @param id_byte identifikační byte (pro odlišení superbloku od běžného)
         */
        void packData(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize, id_byte_t id_byte);

        /**
         * Vstupní data rozdělí na CRC část a zbytek, porovná kontrolní součty
         * (v případě nesrovnalostí vyhazuje výjimku XYZ) a výsledek předá
         * do výstupného bufferu
         * @param input vstupní buffer
         * @param inputSize délka vstupního bufferu
         * @param output výstupní buffer, metoda sama alokuje potřebnou délku
         * @param outputSize délka výstupního bufferu
         * @param id_byte metoda naplní tento parametr hodnotou identifikačního byte
         * @throw ExceptionRuntimeError
         */
        void unpackData(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize, id_byte_t* id_byte);

        void chainAddEntity(bytestream_t* input, size_t inputLength);

        //HiddenFS::hash_t_sizeof_t& hash_t_sizeof;

      /*
    class chainManager {
    public:
        void addEntitiy(bytestream_t* input, size_t inputLength);


        typedef std::vector<chainItem> table_t;
    private:
        table_t table;

    };
    */

        // ---------------------------------------------------------------------
        // -----------------------     FUSE     --------------------------------
        // ---------------------------------------------------------------------

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
}
