#ifndef COMPILER_H
#define COMPILER_H

#define AUTOMATIC
#define TYPEDEF
#define STATIC static

#define FUNC(rettype, memclass) rettype
#define P2VAR(ptrtype, memclass, ptrclass) ptrtype *
#define P2CONST(ptrtype, memclass, ptrclass) const ptrtype *
#define CONST(type, memclass) const type
#define VAR(type, memclass) type

#endif
