/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 15. březen 2012
 */

#ifndef EXCEPTIONS_H
#define	EXCEPTIONS_H

#include <string>
#include <sstream>
#include "types.h"

using namespace std;

class ExceptionBase {
public:
    string toString() {
        return this->msg;
    }

protected:
    string msg;
};

class ExceptionFileNotFound: public ExceptionBase {
public:
    ExceptionFileNotFound(inode_t inode) {
        stringstream s;
        s << "Soubor " << inode << " nenalezen";

        this->msg = s.str();
    }
};

class ExceptionNotImplemented: public ExceptionBase {
public:
    ExceptionNotImplemented(string msg) {
        this->msg = msg;
    }
};

class ExceptionInodeExists: public ExceptionBase {
public:
    ExceptionInodeExists(inode_t inode) {
        stringstream s;
        s << "Inode " << inode << " již existuje!";

        this->msg = s.str();
    }
};

class ExceptionBlockOutOfRange: public ExceptionBase {
public:
    ExceptionBlockOutOfRange(T_BLOCK_NUMBER b) {
        stringstream s;
        s << "Blok " << b << " je mimo rozsah aktuálního souboru!";

        this->msg = s.str();
    }
};

#endif	/* EXCEPTIONS_H */

