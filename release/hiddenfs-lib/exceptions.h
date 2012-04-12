/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 15. březen 2012
 */

#ifndef EXCEPTIONS_H
#define	EXCEPTIONS_H

#include <string>
#include <sstream>
#include <exception>
#include "types.h"

using namespace std;

class ExceptionBase {
public:
    const char* what() {
        return this->msg.c_str();
    }
    //virtual ~ExceptionBase() throw() = 0;

protected:
    string msg;
};

class ExceptionFileNotFound: public ExceptionBase {
public:
    ExceptionFileNotFound(inode_t inode) {
        stringstream s;
        s << "Soubor s inode " << inode << " nenalezen";

        this->msg = s.str();
    }
    ExceptionFileNotFound(string filename) {
        stringstream s;
        s << "Soubor s názvem" << filename << " nenalezen";

        this->msg = s.str();
    }
};

class ExceptionNotImplemented: public ExceptionBase {
public:
    ExceptionNotImplemented(string msg) {
        this->msg = msg;
    }
};

class ExceptionBlockNotFound: public ExceptionBase {
public:
    ExceptionBlockNotFound(string msg) {
        this->msg = msg;
    }
    ExceptionBlockNotFound() {
        this->msg.clear();
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

class ExceptionRuntimeError: public ExceptionBase {
public:
    ExceptionRuntimeError(string str) {
        this->msg = str;
    }
};

class ExceptionDiscFull: public ExceptionBase {
public:
    ExceptionDiscFull(string str) {
        this->msg = str;
    }
    ExceptionDiscFull() {
        this->msg.clear();
    }
};



#endif	/* EXCEPTIONS_H */

