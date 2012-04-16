/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 24. březen 2012
 */

#ifndef COMMON_H
#define	COMMON_H

#include <errno.h>
#include <iostream>
#include <string>

#include "exceptions.h"
#include "types.h"

namespace HiddenFS {
    #define _D(f) std::cerr << "DEBUG: " << f << "\n";

    std::string print_vFile(vFile* f);

    std::string print_vBlock(vBlock* b);

    /**
     * Objekt zastřešující výpočet kontrolního CRC součtu
     * Implementace převzata z RFC 1952
     * @see http://tools.ietf.org/html/rfc1952#section-8
     */
    class CRC {
    public:
        typedef unsigned long CRC_t;

        /* Return the CRC of the bytes buf[0..len-1]. */
        CRC_t calculate(unsigned char *buf, int len);

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
}

#endif	/* COMMON_H */

