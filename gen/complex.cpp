#include "gen/llvm.h"

#include "mtype.h"
#include "declaration.h"

#include "gen/complex.h"
#include "gen/tollvm.h"
#include "gen/llvmhelpers.h"
#include "gen/irstate.h"
#include "gen/dvalue.h"
#include "gen/logger.h"

//////////////////////////////////////////////////////////////////////////////////////////

llvm::StructType* DtoComplexType(Type* type)
{
    Type* t = type->toBasetype();
    LLType* base = DtoComplexBaseType(t);
    llvm::SmallVector<LLType*, 2> types;
    types.push_back(base);
    types.push_back(base);
    return llvm::StructType::get(gIR->context(), types);
}

LLType* DtoComplexBaseType(Type* t)
{
    TY ty = t->toBasetype()->ty;
    if (ty == Tcomplex32) {
        return LLType::getFloatTy(gIR->context());
    }
    else if (ty == Tcomplex64) {
        return LLType::getDoubleTy(gIR->context());
    }
    else if (ty == Tcomplex80) {
        if ((global.params.cpu == ARCHx86) || (global.params.cpu == ARCHx86_64))
            return LLType::getX86_FP80Ty(gIR->context());
        else
            return LLType::getDoubleTy(gIR->context());
    }
    else {
        assert(0);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

LLConstant* DtoConstComplex(Type* _ty, long double re, long double im)
{
    TY ty = _ty->toBasetype()->ty;

    Type* base = 0;
    if (ty == Tcomplex32) {
        base = Type::tfloat32;
    }
    else if (ty == Tcomplex64) {
        base = Type::tfloat64;
    }
    else if (ty == Tcomplex80) {
        base = Type::tfloat80;
    }

    std::vector<LLConstant*> inits;
    inits.push_back(DtoConstFP(base, re));
    inits.push_back(DtoConstFP(base, im));

    return llvm::ConstantStruct::get(DtoComplexType(_ty), inits);
}

//////////////////////////////////////////////////////////////////////////////////////////

LLValue* DtoRealPart(DValue* val)
{
    assert(0);
    return gIR->ir->CreateExtractElement(val->getRVal(), DtoConstUint(0), "tmp");
}

//////////////////////////////////////////////////////////////////////////////////////////

LLValue* DtoImagPart(DValue* val)
{
    assert(0);
    return gIR->ir->CreateExtractElement(val->getRVal(), DtoConstUint(1), "tmp");
}

//////////////////////////////////////////////////////////////////////////////////////////

DValue* DtoComplex(Loc& loc, Type* to, DValue* val)
{
    LLType* complexTy = DtoType(to);

    Type* baserety;
    Type* baseimty;
    TY ty = to->toBasetype()->ty;
    if (ty == Tcomplex32) {
        baserety = Type::tfloat32;
        baseimty = Type::timaginary32;
    } else if (ty == Tcomplex64) {
        baserety = Type::tfloat64;
        baseimty = Type::timaginary64;
    } else if (ty == Tcomplex80) {
        baserety = Type::tfloat80;
        baseimty = Type::timaginary80;
    }
    else {
        assert(0);
    }

    LLValue *re, *im;
    DtoGetComplexParts(loc, to, val, re, im);

    if(!re)
        re = LLConstant::getNullValue(DtoType(baserety));
    if(!im)
        im = LLConstant::getNullValue(DtoType(baseimty));

    LLValue* res = DtoAggrPair(complexTy, re, im);

    return new DImValue(to, res);
}

//////////////////////////////////////////////////////////////////////////////////////////

void DtoComplexSet(LLValue* c, LLValue* re, LLValue* im)
{
    DtoStore(re, DtoGEPi(c,0,0,"tmp"));
    DtoStore(im, DtoGEPi(c,0,1,"tmp"));
}

//////////////////////////////////////////////////////////////////////////////////////////

void DtoGetComplexParts(Loc& loc, Type* to, DValue* val, DValue*& re, DValue*& im)
{
    Type* baserety;
    Type* baseimty;
    TY ty = to->toBasetype()->ty;
    if (ty == Tcomplex32) {
        baserety = Type::tfloat32;
        baseimty = Type::timaginary32;
    } else if (ty == Tcomplex64) {
        baserety = Type::tfloat64;
        baseimty = Type::timaginary64;
    } else if (ty == Tcomplex80) {
        baserety = Type::tfloat80;
        baseimty = Type::timaginary80;
    }
    else {
        assert(0);
    }

    Type* t = val->getType()->toBasetype();

    if (t->iscomplex()) {
        DValue* v = DtoCastComplex(loc, val, to);
        if (to->iscomplex()) {
            if (v->isLVal()) {
                LLValue *reVal = DtoGEP(v->getLVal(), DtoConstInt(0), DtoConstInt(0), ".re_part");
                LLValue *imVal = DtoGEP(v->getLVal(), DtoConstInt(0), DtoConstInt(1), ".im_part");
                re = new DVarValue(baserety, reVal);
                im = new DVarValue(baseimty, imVal);
            } else {
                LLValue *reVal = gIR->ir->CreateExtractValue(v->getRVal(), 0, ".re_part");
                LLValue *imVal = gIR->ir->CreateExtractValue(v->getRVal(), 1, ".im_part");
                re = new DImValue(baserety, reVal);
                im = new DImValue(baseimty, imVal);
            }
        } else
            DtoGetComplexParts(loc, to, v, re, im);
    }
    else if (t->isimaginary()) {
        re = NULL;
        im = DtoCastFloat(loc, val, baseimty);
    }
    else if (t->isfloating()) {
        re = DtoCastFloat(loc, val, baserety);
        im = NULL;
    }
    else if (t->isintegral()) {
        re = DtoCastInt(loc, val, baserety);
        im = NULL;
    }
    else {
        assert(0);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

void DtoGetComplexParts(Loc& loc, Type* to, DValue* val, LLValue*& re, LLValue*& im)
{
    DValue *dre, *dim;
    DtoGetComplexParts(loc, to, val, dre, dim);
    re = dre ? dre->getRVal() : 0;
    im = dim ? dim->getRVal() : 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

DValue* DtoComplexAdd(Loc& loc, Type* type, DValue* lhs, DValue* rhs)
{
    llvm::Value *lhs_re, *lhs_im, *rhs_re, *rhs_im, *res_re, *res_im;

    // lhs values
    DtoGetComplexParts(loc, type, lhs, lhs_re, lhs_im);
    // rhs values
    DtoGetComplexParts(loc, type, rhs, rhs_re, rhs_im);

    // add up
    if(lhs_re && rhs_re)
        res_re = gIR->ir->CreateFAdd(lhs_re, rhs_re, "tmp");
    else if(lhs_re)
        res_re = lhs_re;
    else // either rhs_re or no re at all (then use any)
        res_re = rhs_re;
    
    if(lhs_im && rhs_im)
        res_im = gIR->ir->CreateFAdd(lhs_im, rhs_im, "tmp");
    else if(lhs_im)
        res_im = lhs_im;
    else // either rhs_im or no im at all (then use any)
        res_im = rhs_im;

    LLValue* res = DtoAggrPair(DtoType(type), res_re, res_im);
    return new DImValue(type, res);
}

//////////////////////////////////////////////////////////////////////////////////////////

DValue* DtoComplexSub(Loc& loc, Type* type, DValue* lhs, DValue* rhs)
{
    llvm::Value *lhs_re, *lhs_im, *rhs_re, *rhs_im, *res_re, *res_im;

    // lhs values
    DtoGetComplexParts(loc, type, lhs, lhs_re, lhs_im);
    // rhs values
    DtoGetComplexParts(loc, type, rhs, rhs_re, rhs_im);

    // add up
    if(lhs_re && rhs_re)
        res_re = gIR->ir->CreateFSub(lhs_re, rhs_re, "tmp");
    else if(lhs_re)
        res_re = lhs_re;
    else // either rhs_re or no re at all (then use any)
        res_re = gIR->ir->CreateFNeg(rhs_re, "neg");
    
    if(lhs_im && rhs_im)
        res_im = gIR->ir->CreateFSub(lhs_im, rhs_im, "tmp");
    else if(lhs_im)
        res_im = lhs_im;
    else // either rhs_im or no im at all (then use any)
        res_im = gIR->ir->CreateFNeg(rhs_im, "neg");

    LLValue* res = DtoAggrPair(DtoType(type), res_re, res_im);
    return new DImValue(type, res);
}

//////////////////////////////////////////////////////////////////////////////////////////

DValue* DtoComplexMul(Loc& loc, Type* type, DValue* lhs, DValue* rhs)
{
    llvm::Value *lhs_re, *lhs_im, *rhs_re, *rhs_im, *res_re, *res_im;

    // lhs values
    DtoGetComplexParts(loc, type, lhs, lhs_re, lhs_im);
    // rhs values
    DtoGetComplexParts(loc, type, rhs, rhs_re, rhs_im);

    // mul up
    llvm::Value *rere = NULL;
    llvm::Value *reim = NULL;
    llvm::Value *imre = NULL;
    llvm::Value *imim = NULL;

    if(lhs_re && rhs_re)
        rere = gIR->ir->CreateFMul(lhs_re, rhs_re, "rere_mul");
    if(lhs_re && rhs_im)
        reim = gIR->ir->CreateFMul(lhs_re, rhs_im, "reim_mul");
    if(lhs_im && rhs_re)
        imre = gIR->ir->CreateFMul(lhs_im, rhs_re, "imre_mul");
    if(lhs_im && rhs_im)
        imim = gIR->ir->CreateFMul(lhs_im, rhs_im, "imim_mul");

    if(rere && imim)
        res_re = gIR->ir->CreateFSub(rere, imim, "rere_imim_sub");
    else if(rere)
        res_re = rere;
    else if(imim)
        res_re = gIR->ir->CreateFNeg(imim, "imim_neg");
    else
        res_re = lhs_re ? rhs_re : lhs_re; // null!

    if(reim && imre)
        res_im = gIR->ir->CreateFAdd(reim, imre, "reim_imre_add");
    else if(reim)
        res_im = reim;
    else if(imre)
        res_im = imre;
    else
        res_im = lhs_re ? rhs_im : lhs_re; // null!

    LLValue* res = DtoAggrPair(DtoType(type), res_re, res_im);
    return new DImValue(type, res);
}

//////////////////////////////////////////////////////////////////////////////////////////

DValue* DtoComplexDiv(Loc& loc, Type* type, DValue* lhs, DValue* rhs)
{
    llvm::Value *lhs_re, *lhs_im, *rhs_re, *rhs_im, *res_re, *res_im;

    // lhs values
    DtoGetComplexParts(loc, type, lhs, lhs_re, lhs_im);
    // rhs values
    DtoGetComplexParts(loc, type, rhs, rhs_re, rhs_im);

    // if divisor is only real, division is simple
    if(rhs_re && !rhs_im) {
        if(lhs_re)
            res_re = gIR->ir->CreateFDiv(lhs_re, rhs_re, "re_divby_re");
        else
            res_re = lhs_re;
        if(lhs_im)
            res_im = gIR->ir->CreateFDiv(lhs_im, rhs_re, "im_divby_re");
        else
            res_im = lhs_im;
    }
    // if divisor is only imaginary, division is simple too
    else if(!rhs_re && rhs_im) {
        if(lhs_re)
            res_im = gIR->ir->CreateFNeg(gIR->ir->CreateFDiv(lhs_re, rhs_im, "re_divby_im"), "neg");
        else
            res_im = lhs_re;
        if(lhs_im)
            res_re = gIR->ir->CreateFDiv(lhs_im, rhs_im, "im_divby_im");
        else
            res_re = lhs_im;
    }
    // full division
    else {
        llvm::Value *tmp1, *tmp2, *denom;

        if(lhs_re && lhs_im) {
            tmp1 = gIR->ir->CreateFMul(lhs_re, rhs_re, "rere");
            tmp2 = gIR->ir->CreateFMul(lhs_im, rhs_im, "imim");
            res_re = gIR->ir->CreateFAdd(tmp1, tmp2, "rere_plus_imim");

            tmp1 = gIR->ir->CreateFMul(lhs_re, rhs_im, "reim");
            tmp2 = gIR->ir->CreateFMul(lhs_im, rhs_re, "imre");
            res_im = gIR->ir->CreateFSub(tmp2, tmp1, "imre_sub_reim");
        }
        else if(lhs_re) {
            res_re = gIR->ir->CreateFMul(lhs_re, rhs_re, "rere");

            res_im = gIR->ir->CreateFMul(lhs_re, rhs_im, "reim");
            res_im = gIR->ir->CreateFNeg(res_im);
        }
        else if(lhs_im) {
            res_re = gIR->ir->CreateFMul(lhs_im, rhs_im, "imim");
            res_im = gIR->ir->CreateFMul(lhs_im, rhs_re, "imre");
        }
        else
            assert(0 && "lhs has neither real nor imaginary part");

        tmp1 = gIR->ir->CreateFMul(rhs_re, rhs_re, "rhs_resq");
        tmp2 = gIR->ir->CreateFMul(rhs_im, rhs_im, "rhs_imsq");
        denom = gIR->ir->CreateFAdd(tmp1, tmp2, "denom");

        res_re = gIR->ir->CreateFDiv(res_re, denom, "res_re");
        res_im = gIR->ir->CreateFDiv(res_im, denom, "res_im");
    }

    LLValue* res = DtoAggrPair(DtoType(type), res_re, res_im);
    return new DImValue(type, res);
}

//////////////////////////////////////////////////////////////////////////////////////////

DValue* DtoComplexRem(Loc& loc, Type* type, DValue* lhs, DValue* rhs)
{
    llvm::Value *lhs_re, *lhs_im, *rhs_re, *rhs_im, *res_re, *res_im, *divisor;

    // lhs values
    DtoGetComplexParts(loc, type, lhs, lhs_re, lhs_im);
    // rhs values
    DtoGetComplexParts(loc, type, rhs, rhs_re, rhs_im);

    // Divisor can be real or imaginary but not complex
    assert((rhs_re != 0) ^ (rhs_im != 0));

    divisor = rhs_re ? rhs_re : rhs_im;
    res_re = lhs_re ? gIR->ir->CreateFRem(lhs_re, divisor, "tmp") : lhs_re;
    res_im = lhs_re ? gIR->ir->CreateFRem(lhs_im, divisor, "tmp") : lhs_im;

    LLValue* res = DtoAggrPair(DtoType(type), res_re, res_im);
    return new DImValue(type, res);
}

//////////////////////////////////////////////////////////////////////////////////////////

DValue* DtoComplexNeg(Loc& loc, Type* type, DValue* val)
{
    llvm::Value *a, *b, *re, *im;

    // values
    DtoGetComplexParts(loc, type, val, a, b);

    // neg up
    assert(a && b);
    re = gIR->ir->CreateFNeg(a, "tmp");
    im = gIR->ir->CreateFNeg(b, "tmp");

    LLValue* res = DtoAggrPair(DtoType(type), re, im);
    return new DImValue(type, res);
}

//////////////////////////////////////////////////////////////////////////////////////////

LLValue* DtoComplexEquals(Loc& loc, TOK op, DValue* lhs, DValue* rhs)
{
    Type* type = lhs->getType();

    DValue *lhs_re, *lhs_im, *rhs_re, *rhs_im;

    // lhs values
    DtoGetComplexParts(loc, type, lhs, lhs_re, lhs_im);
    // rhs values
    DtoGetComplexParts(loc, type, rhs, rhs_re, rhs_im);

    // (l.re==r.re && l.im==r.im) or (l.re!=r.re || l.im!=r.im)
    LLValue* b1 = DtoBinFloatsEquals(loc, lhs_re, rhs_re, op);
    LLValue* b2 = DtoBinFloatsEquals(loc, lhs_im, rhs_im, op);

    if (op == TOKequal)
        return gIR->ir->CreateAnd(b1,b2,"tmp");
    else
        return gIR->ir->CreateOr(b1,b2,"tmp");
}

//////////////////////////////////////////////////////////////////////////////////////////

DValue* DtoCastComplex(Loc& loc, DValue* val, Type* _to)
{
    Type* to = _to->toBasetype();
    Type* vty = val->getType()->toBasetype();
    if (to->iscomplex()) {
        if (vty->size() == to->size())
            return val;

        llvm::Value *re, *im;
        DtoGetComplexParts(loc, val->getType(), val, re, im);
        LLType* toty = DtoComplexBaseType(to);

        if (to->size() < vty->size()) {
            re = gIR->ir->CreateFPTrunc(re, toty, "tmp");
            im = gIR->ir->CreateFPTrunc(im, toty, "tmp");
        }
        else {
            re = gIR->ir->CreateFPExt(re, toty, "tmp");
            im = gIR->ir->CreateFPExt(im, toty, "tmp");
        }

        LLValue* pair = DtoAggrPair(DtoType(_to), re, im);
        return new DImValue(_to, pair);
    }
    else if (to->isimaginary()) {
        // FIXME: this loads both values, even when we only need one
        LLValue* v = val->getRVal();
        LLValue* impart = gIR->ir->CreateExtractValue(v, 1, ".im_part");
        Type *extractty;
        if (vty->ty == Tcomplex32) {
            extractty = Type::timaginary32;
        } else if (vty->ty == Tcomplex64) {
            extractty = Type::timaginary64;
        } else if (vty->ty == Tcomplex80) {
            extractty = Type::timaginary80;
        }
        DImValue* im = new DImValue(extractty, impart);
        return DtoCastFloat(loc, im, to);
    }
    else if (to->ty == Tbool) {
        return new DImValue(_to, DtoComplexEquals(loc, TOKnotequal, val, DtoNullValue(vty)));
    }
    else if (to->isfloating() || to->isintegral()) {
        // FIXME: this loads both values, even when we only need one
        LLValue* v = val->getRVal();
        LLValue* repart = gIR->ir->CreateExtractValue(v, 0, ".re_part");
        Type *extractty;
        if (vty->ty == Tcomplex32) {
            extractty = Type::tfloat32;
        } else if (vty->ty == Tcomplex64) {
            extractty = Type::tfloat64;
        } else if (vty->ty == Tcomplex80) {
            extractty = Type::tfloat80;
        }
        DImValue* re = new DImValue(extractty, repart);
        return DtoCastFloat(loc, re, to);
    }
    else {
        error(loc, "Don't know how to cast %s to %s", vty->toChars(), to->toChars());
        fatal();
    }
}

