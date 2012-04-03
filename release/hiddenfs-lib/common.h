/**
 * @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
 *
 * Created on 24. březen 2012
 */

#ifndef COMMON_H
#define	COMMON_H

#include <string>
#include <iostream>
#include "types.h"
#include <errno.h>

using namespace std;
#define _D(f) cerr << "DEBUG: " << f << endl;

string print_vFile(vFile* f);

string print_vBlock(vBlock* b);

#endif	/* COMMON_H */

