#ifndef VSS_COMPILER_H
#define VSS_COMPILER_H

#include "ast.h"
#include "object.h"

ObjFunction *compile_program(Block program);

#endif
