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

#define _D(f) std::cerr << "DEBUG: " << f << "\n";

std::string print_vFile(vFile* f);

std::string print_vBlock(vBlock* b);

#endif	/* COMMON_H */

