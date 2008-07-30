#include "gen/llvm.h"
#include "mtype.h"
#include "aggregate.h"
#include "ir/irstruct.h"
#include "gen/irstate.h"

IrInterface::IrInterface(BaseClass* b)
{
    base = b;
    decl = b->base;
    vtblTy = NULL;
    vtblInit = NULL;
    vtbl = NULL;
    infoTy = NULL;
    infoInit = NULL;
    info = NULL;

    index = -1;
}

IrInterface::~IrInterface()
{
    delete vtblTy;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

IrStruct::IrStruct(Type* t)
 : recty((t->ir.type) ? *t->ir.type : llvm::OpaqueType::get())
{
    type = t;
    defined = false;
    constinited = false;
    interfaceInfosTy = NULL;
    interfaceInfos = NULL;

    vtbl = NULL;
    constVtbl = NULL;
    init = NULL;
    constInit = NULL;
    classInfo = NULL;
    constClassInfo = NULL;
    hasUnions = false;
    dunion = NULL;

    classDeclared = false;
    classDefined = false;

    packed = false;

    dwarfComposite = NULL;
}

IrStruct::~IrStruct()
{
}
