#ifndef VSS_CLI_H
#define VSS_CLI_H

#include "object.h"

int run_cli(int argc, char **argv);

// Serialization helpers
bool serialize_function(ObjFunction *func, FILE *out);
ObjFunction *deserialize_function(FILE *in);

#endif
