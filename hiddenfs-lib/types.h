/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 18. březen 2012, 19:48
 */

#ifndef TYPES_H
#define	TYPES_H

#include <bits/types.h>
#include <cryptopp/sha.h>
#include <set>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>


namespace HiddenFS {
    /** maximální délka obecně pro zápis dat do úložiště (implicitně 4kB) */
    #define BLOCK_MAX_LENGTH (1 << 12)

    /**
     * Objekt plní funkci chytré konstanty - lze ji číst bez omezení, ale nastavit právě jednou.
     * Pokud při čtení nebyla hodnota ještě nastavena, vyhazuje výjimku ExceptionRuntimeError
     * Pokud při zápisu už byla hodnota jednou nastavena, také vyhazuje výjimku ExceptionRuntimeError
     */
    template <class T>
    class smartConst {
    public:
        smartConst() {
            this->wasSet = false;
        }

        const T& get() const {
            if(! this->wasSet) {
                throw std::runtime_error("Hodnota ještě nebyla nastavena.");
            }

            return this->value;
        }

        void set(T const& val) {
            // zabránit vícenásobnému nastavení hodnoty
            if(this->wasSet) {
                throw std::runtime_error("Hodnotu nelze nastavit více, než právě jednou.");
            }

            this->value = val;
            this->wasSet = true;
        }
    private:
        bool wasSet;
        T value;
    };

    /** datový typ čísla bloku v rámci skutečného souboru */
    typedef __uint32_t block_number_t;
    static const size_t block_number_sizeof = sizeof(block_number_t);

    /** datový typ pro číslování souborů */
    typedef __uint32_t inode_t;
    static const size_t inode_t_sizeof = sizeof(inode_t);

    typedef __uint8_t flags_t;
    static const size_t flags_t_sizeof = sizeof(flags_t);

    /** datový typ pro ASCII reprezentaci binární podoby hash skutečného souboru */
    typedef std::string hash_ascii_t;

    /** datový typ "jednotky" pro manipulaci s binárním obsahem (unsigned char) */
    typedef __u_char bytestream_t;

    typedef bytestream_t hash_raw_t;
    static const size_t hash_raw_t_sizeof = CryptoPP::SHA1::DIGESTSIZE;

    //hash_t_sizeof_t hash_t_sizeof;
    //typedef smartConst<size_t> hash_t_sizeof_t;
    //hash_t_sizeof_t hash_t_sizeof;
    // poznámka: proměnná pro nastavení délky hash_t se nachází v hiddenfs-lib.cpp

    typedef __uint32_t fragment_t;
    static const size_t fragment_t_sizeof = sizeof(fragment_t);

    /** datový typ identifikačního byte pro rozlišení superbloku a běžného bloku (unsigned char) */
    typedef __u_char id_byte_t;

    /** datový typ pro uchování počtu entit v rámci jednoho bloku */
    typedef __uint32_t chain_count_t;

    /** čísla pod touto hodnotou (bez této hodnoty) jsou považovány za identifikátor
     * běžného datového bloku, hodnoty rovny nebo větší jsou považovány za superblok */
    static const id_byte_t ID_BYTE_BOUNDARIES[] = {127, 200};

    /** počet prázdných souborů, které se vyhledávají v jedné iteraci při
     * automatickém vytváření kontextů */
    static const unsigned int HT_UNUSED_CHUNK = 10;

    /** počet částečně obsazených souborů, které se vyhledávají v jedné iteraci při
     * automatickém vytváření kontextů */
    static const unsigned int HT_PARTUSED_CHUNK = HT_UNUSED_CHUNK;

    /** Procentuální množství automaticky rezervovaných bloků, výchozí hodnotou je: 10 (=10%) */
    static const unsigned int ALLOCATOR_RESERVED_QUANTITY = 10;

    /** První pořadové číslo fragmentu (číslování v rámci hiddenFS) */
    static const fragment_t FRAGMENT_FIRST = 0;

    /** První pořadové číslo bloku (číslování v rámci skutečných souborů) */
    static const block_number_t FIRST_BLOCK_NO = 10;

    /** Maximální délka pro názvy souborů */
    static const unsigned int FILENAME_MAX_LENGTH = 255;

    /** Počet kopií stejné tabulky */
    static const unsigned int TABLES_REDUNDANCY_AMOUNT = 2;

    /** Počet kopií superbloku */
    static const unsigned int SUPERBLOCK_REDUNDANCY_AMOUNT = 2;

    static const int BLOCK_IN_USE = true;
    static const int BLOCK_RESERVED = false;

    /** Maximální délka hesla použitého pro šifrování */
    static const unsigned int PASSWORD_MAX_LENGTH = 30;

    /** Počet pokusů při zadávní hesla */
    static const unsigned int PASSWORD_MAX_ATTEMPTS = 3;

    /** Velikost bloku při alokování bufferu pro zápis (implicitně 1MB) */
    static const unsigned int WRITE_BUFF_ALLOC_BLOCKSIZE = 1 << 20;

    /**
     * Reprezentace virtuálního souboru - skrývaného souboru
     */
    struct vFile {
        static const flags_t FLAG_NONE      = 0;
        static const flags_t FLAG_READ      = 1 << 0;
        static const flags_t FLAG_WRITE     = 1 << 1;
        static const flags_t FLAG_EXECUTE   = 1 << 2;
        static const flags_t FLAG_DIR       = 1 << 3;

        vFile() {
            flags = FLAG_NONE;
            size = 0;
            filename = "";
            inode = -1;
        }

        /** i-node identifikátor soboru */
        inode_t inode;

        /** číslo inode rodiče */
        inode_t parent;

        /** délka souboru v bytech */
        size_t size;

        /** název skrývaného souboru */
        std::string filename;

        /** příznaky souboru (D, RWX) */
        flags_t flags;
    };

    /** abstrakce datového bloku obsahující právě jeden fragment */
    struct vBlock {
        /** identifikace skutečného souboru */
        hash_ascii_t hash;

        /** pořadové číslo bloku ve skutečném souboru (nesmí se opakovat v rámci vBlock) */
        block_number_t block;

        /** číslo fragmentu virtuálního souboru (může se opakovat v rámci vFile) */
        fragment_t fragment;

        /** příznak, jestli je tento blok použitý (BLOCK_IN_USE), nebo jen rezervovaný (BLOCK_RESERVED) */
        bool used;

        /** délka využité části bloku (délka blockContent.content) */
        size_t length;
    };

    /** Součet jednotlivých délek složek struktury vBlock bez jakéhokoli zarovnávání,
     * proto hodnota SIZEOF_vBlock je menší než sizeof(vBlock).
     * Tato konstanta se používá pouze pro práci s dumpem struktury vBlock! */
    static const size_t SIZEOF_vBlock = \
        + sizeof(block_number_t) \
        + sizeof(fragment_t) \
        + sizeof(size_t)
        + hash_raw_t_sizeof \
        + sizeof(bool);

    /** Popis obsahu jedné entity (složky length a content*) */
    struct chainItem {
        size_t length;
        bytestream_t* content;
    };

    /**
     * Abstrakce spojového seznamu, který slouží pro manipulaci s entitami
     * (jakožto prvky obecné délky)
     * vector<chainItem>
     */
    typedef std::vector<chainItem> chainList_t;


    typedef struct {
        /** maximální počet bloků, které je povoleno do tohoto souboru umístit */
        unsigned int maxBlocks;

        /** seznam neobsazených bloků */
        std::set<HiddenFS::block_number_t> avaliableBlocks;

        /** seznam obsazených bloků */
        std::set<HiddenFS::block_number_t> usedBlocks;

        /** ukazatel pro obecně dále nespecifikované použití, pokud to uživatelská
         * implementace vyžaduje */
        void* userContext;
    } context_t;
}

#endif	/* TYPES_H */

