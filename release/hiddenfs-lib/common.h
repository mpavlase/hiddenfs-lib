/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 24. březen 2012
 */

#ifndef COMMON_H
#define	COMMON_H

#include <assert.h>
#include <cryptopp/hex.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <string>
#include <vector>


#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/hex.h>

#include "exceptions.h"
#include "types.h"

namespace HiddenFS {
    #define _D(f) std::cerr << "DEBUG: " << f << "\n";

    std::string print_vFile(vFile* f);

    std::string print_vBlock(vBlock* b);

    /**
     * Převod struktury vBlock na blok bytů
     * @param input zpracovávaný vBlock
     * @param output výstupní buffer, metoda sama alokuje dostatečný prostor
     * @param size délka výstupního bufferu
     */
    void serialize_vBlock(vBlock* input, bytestream_t** output, size_t* size);

    /**
     * Provádí převod binárního bloku dat zpět na strukturu vBlock
     * @param input vstupní buffer
     * @param size délka vstupního bufferu
     * @param output výstupní rekonstruovaný vBlock, metoda sama alokuje tento ukazatel
     */
    void unserialize_vBlock(bytestream_t* input, size_t size, vBlock** output);

    /**
     * Objekt zastřešující výpočet kontrolního CRC součtu
     * Implementace převzata z RFC 1952
     * @see http://tools.ietf.org/html/rfc1952#section-8
     */
    class CRC {
    public:
        typedef __uint32_t CRC_t;

        /* Return the CRC of the bytes buf[0..len-1]. */
        CRC_t calculate(__u_char *buf, int len);

        CRC();

    private:
        /* Table of CRCs of all 8-bit messages. */
        unsigned long crc_table[256];

        /* Make the table for a fast CRC. */
        void make_crc_table();

        /*
         Update a running crc with the bytes buf[0..len-1] and return
        the updated crc. The crc should be initialized to zero. Pre- and
        post-conditioning (one's complement) is performed within this
        function so it shouldn't be done by the caller. Usage example:

         unsigned long crc = 0L;

         while (read_buffer(buffer, length) != EOF) {
           crc = update_crc(crc, buffer, length);
         }
         if (crc != original_crc) error();
        */
        CRC_t update_crc(CRC_t crc, unsigned char *buf, int len);
    };

    class IEncryption {
    public:
        virtual ~IEncryption() {
            delete this->key;
        };
        /**
         * Uložení šifrovacího klíče do nitra objektu
         */
        virtual void setKey(bytestream_t* key, size_t keySize) {
            std::cout << "IEncryption::setKey = " << key << "\n" << std::flush;
            this->key = new bytestream_t[keySize];
            this->keySize = keySize;
            memcpy(this->key, key, this->keySize);
        };

        /**
         * Obecné rozhraní pro šifrování bloku dat
         * @param input vstupní buffer
         * @param inputSize délka vstupního bufferu
         * @param output výstupní buffer, metoda si sama alokuje dostatečný prostor
         * @param outputSize délka výstupního bufferu
         */
        virtual void encrypt(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize) = 0;

        /**
         * Obecné rozhraní pro dešifrování bloku dat
         * @param input zašifrovaný blok dat
         * @param inputSize délka bufferu input
         * @param output dešifrovaný vstup, metoda si sama alokuje dostatečný prostor
         * @param outputSize délka výstupního bufferu
         */
        virtual void decrypt(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize) = 0;

    protected :
        bytestream_t* key;
        size_t keySize;
    };

    class DefaultEncryption : public IEncryption {
    public:
        byte encKey[CryptoPP::AES::DEFAULT_KEYLENGTH];
        byte encIv[CryptoPP::AES::BLOCKSIZE];

        DefaultEncryption() : IEncryption() {
            assert((unsigned size_t)this->keySize <= (unsigned size_t)CryptoPP::AES::DEFAULT_KEYLENGTH);

            memset(this->encKey, '\0', sizeof(this->encKey));
            memcpy(this->encKey, this->key, this->keySize);
            //prng.GenerateBlock(key, sizeof(key));

            memset(this->encIv, '\0', sizeof(this->encIv));
            //prng.GenerateBlock(iv, sizeof(iv));
        }

        void encrypt(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize) {
            std::string cipher;

            assert(input != NULL);
            assert(outputSize != NULL);

            try {
                CryptoPP::OFB_Mode< CryptoPP::AES >::Encryption e;
                e.SetKeyWithIV(this->encKey, sizeof(this->encKey), this->encIv);

                // OFB mode must not use padding. Specifying
                //  a scheme will result in an exception
                CryptoPP::StringSource((const byte*) input, inputSize, true,
                    new CryptoPP::StreamTransformationFilter(e,
                        new CryptoPP::StringSink(cipher)
                    ) // StreamTransformationFilter
                ); // StringSource

                *outputSize = cipher.length();
                *output = new bytestream_t[*outputSize];
                cipher.copy((char*)*output, *outputSize, 0);
                //memcpy(*output, cipher.data(), *outputSize);
            } catch(const CryptoPP::Exception& e) {
                std::cerr << e.what() << std::endl;
                exit(1);
            }
        };

        void decrypt(bytestream_t* input, size_t inputSize, bytestream_t** output, size_t* outputSize) {
            std::string recovered;

            try {
                CryptoPP::OFB_Mode< CryptoPP::AES >::Decryption d;
                d.SetKeyWithIV(this->encKey, sizeof(this->encKey), this->encIv);

                // The StreamTransformationFilter removes
                //  padding as required.
                CryptoPP::StringSource s((const byte*) input, inputSize, true,
                    new CryptoPP::StreamTransformationFilter(d,
                        new CryptoPP::StringSink(recovered)
                    ) // StreamTransformationFilter
                ); // StringSource

                *outputSize = recovered.length();
                *output = new bytestream_t[*outputSize];
                recovered.copy((char*)*output, *outputSize, 0);
                //memcpy(*output, recovered.data(), *outputSize);
            } catch(const CryptoPP::Exception& e) {
                std::cerr << e.what() << std::endl;
                exit(1);
            }
        };
    };

    /**
     * Jedna složka spojového seznamu pro uložení serializovaných entit (=řádků)
     * tabulek
     */
    struct chainOfEntities {
        /** Počet entit v aktuálním článku řetězu */
        __uint32_t count;

        /** Umístění následujícího článku řetězu, pokud je celý nastaven na '\0',
         * Aktuální článek je poslední v řadě. */
        vBlock next;

        /** Ukazatel na dump entit. Jejich délka nemusí být konstantní a je na metodě,
         * která řetěz zpracovává, aby obsah korektně interpretovala */
        __u_char* entities;
    };

    /**
     * Převod struktury vBlock na blok dat
     * @param input vBlock pro serializaci
     * @param output výstupní buffer, metoda sama alokuje dostatečnou délku
     * @param size délka výsledného bloku dat
     */
    void serialize_vBlock(vBlock* input, bytestream_t** output, size_t* size);


    /**
     * Metoda rekonstruuje strukturu vBlock ze serializovaného bloku dat
     * @param input blok dat pro rekonstrukci
     * @param size délka vstupního bufferu (složka input)
     * @param output rekonstruovaný vBlock, metoda sama alokuje tuto proměnnou
     */
    void unserialize_vBlock(bytestream_t* input, size_t size, vBlock** output);

    /**
     * Funkce testuje hodnotu parametru, zda je v rozsahu pro superblock
     */
    bool idByteIsSuperBlock(id_byte_t b);

    /**
     * Funkce testuje hodnotu parametru, zda je v rozsahu pro běžný datový blok
     */
    bool idByteIsDataBlock(id_byte_t b);

    /**
     * Vrací nejvyšší dosažitelnou hodnotu dle datového typu
     */
    id_byte_t idByteMaxValue();

    /**
     * Generuje hodnotu platnou pro běžný datový blok
     */
    id_byte_t idByteGenDataBlock();

    /**
     * Generuje hodnotu v rozsahu platném pro superblock
     */
    id_byte_t idByteGenSuperBlock();


    /**
     * User-friendly tisk (HEX i ASCII) bloku binárních dat
     * @param input vstupní data
     * @param len délka vstupních dat
     */
    void pBytes(bytestream_t* input, size_t len);

    /**
     * Převede hash souboru (může být binární) do podoby běžného ASCII.
     * Vhodným postupem je převod bytů do hexadecimální podoby.
     * @param output
     * @param input
     * @param inputLength délka bufferu input
     */
    void convertHashToAscii(hash_ascii_t& output, const hash_raw_t* input, size_t inputLength);

    /**
     * Převod ASCII řetězce reprezentující hash do jeho původní (binární) podoby.
     * @param output metoda vyžaduje naalokovaný obsah 'output' o délce konstanty hash_raw_t_sizeof
     * @param input
     */
    void convertAsciiToHash(hash_raw_t* output, hash_ascii_t& input);
}

#endif	/* COMMON_H */

