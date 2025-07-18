/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * JS bytecode generation.
 */
#include "jsstddef.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsbit.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"

/* Allocation grain counts, must be powers of two in general. */
#define BYTECODE_GRAIN  256     /* code allocation increment */
#define SRCNOTE_GRAIN   64      /* initial srcnote allocation increment */
#define TRYNOTE_GRAIN   64      /* trynote allocation increment */

/* Macros to compute byte sizes from typed element counts. */
#define BYTECODE_SIZE(n)        ((n) * sizeof(jsbytecode))
#define SRCNOTE_SIZE(n)         ((n) * sizeof(jssrcnote))
#define TRYNOTE_SIZE(n)         ((n) * sizeof(JSTryNote))

JS_FRIEND_API(JSBool)
js_InitCodeGenerator(JSContext *cx, JSCodeGenerator *cg,
                     const char *filename, uintN lineno,
                     JSPrincipals *principals)
{
    memset(cg, 0, sizeof *cg);
    TREE_CONTEXT_INIT(&cg->treeContext);
    cg->treeContext.flags |= TCF_COMPILING;
    cg->codeMark = JS_ARENA_MARK(&cx->codePool);
    cg->noteMark = JS_ARENA_MARK(&cx->notePool);
    cg->tempMark = JS_ARENA_MARK(&cx->tempPool);
    cg->current = &cg->main;
    cg->filename = filename;
    cg->firstLine = cg->currentLine = lineno;
    cg->principals = principals;
    ATOM_LIST_INIT(&cg->atomList);
    cg->noteMask = SRCNOTE_GRAIN - 1;
    return JS_TRUE;
}

JS_FRIEND_API(void)
js_FinishCodeGenerator(JSContext *cx, JSCodeGenerator *cg)
{
    TREE_CONTEXT_FINISH(&cg->treeContext);
    JS_ARENA_RELEASE(&cx->codePool, cg->codeMark);
    JS_ARENA_RELEASE(&cx->notePool, cg->noteMark);
    JS_ARENA_RELEASE(&cx->tempPool, cg->tempMark);
}

static ptrdiff_t
EmitCheck(JSContext *cx, JSCodeGenerator *cg, JSOp op, ptrdiff_t delta)
{
    jsbytecode *base, *limit, *next;
    ptrdiff_t offset, length;
    size_t incr, size;

    base = CG_BASE(cg);
    next = CG_NEXT(cg);
    limit = CG_LIMIT(cg);
    offset = PTRDIFF(next, base, jsbytecode);
    if (next + delta > limit) {
        length = offset + delta;
        length = (length <= BYTECODE_GRAIN)
                 ? BYTECODE_GRAIN
                 : JS_BIT(JS_CeilingLog2(length));
        incr = BYTECODE_SIZE(length);
        if (!base) {
            JS_ARENA_ALLOCATE_CAST(base, jsbytecode *, &cx->codePool, incr);
        } else {
            size = BYTECODE_SIZE(PTRDIFF(limit, base, jsbytecode));
            incr -= size;
            JS_ARENA_GROW_CAST(base, jsbytecode *, &cx->codePool, size, incr);
        }
        if (!base) {
            JS_ReportOutOfMemory(cx);
            return -1;
        }
        CG_BASE(cg) = base;
        CG_LIMIT(cg) = base + length;
        CG_NEXT(cg) = base + offset;
    }
    return offset;
}

static void
UpdateDepth(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t target)
{
    jsbytecode *pc;
    JSCodeSpec *cs;
    intN nuses;

    pc = CG_CODE(cg, target);
    cs = &js_CodeSpec[pc[0]];
    nuses = cs->nuses;
    if (nuses < 0)
        nuses = 2 + GET_ARGC(pc);       /* stack: fun, this, [argc arguments] */
    cg->stackDepth -= nuses;
    if (cg->stackDepth < 0) {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%d", target);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_STACK_UNDERFLOW,
                             cg->filename ? cg->filename : "stdin", numBuf);
    }
    cg->stackDepth += cs->ndefs;
    if ((uintN)cg->stackDepth > cg->maxStackDepth)
        cg->maxStackDepth = cg->stackDepth;
}

ptrdiff_t
js_Emit1(JSContext *cx, JSCodeGenerator *cg, JSOp op)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 1);

    if (offset >= 0) {
        *CG_NEXT(cg)++ = (jsbytecode)op;
        UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_Emit2(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 2);

    if (offset >= 0) {
        jsbytecode *next = CG_NEXT(cg);
        next[0] = (jsbytecode)op;
        next[1] = op1;
        CG_NEXT(cg) = next + 2;
        UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_Emit3(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1,
         jsbytecode op2)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 3);

    if (offset >= 0) {
        jsbytecode *next = CG_NEXT(cg);
        next[0] = (jsbytecode)op;
        next[1] = op1;
        next[2] = op2;
        CG_NEXT(cg) = next + 3;
        UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_EmitN(JSContext *cx, JSCodeGenerator *cg, JSOp op, size_t extra)
{
    ptrdiff_t length = 1 + (ptrdiff_t)extra;
    ptrdiff_t offset = EmitCheck(cx, cg, op, length);

    if (offset >= 0) {
        jsbytecode *next = CG_NEXT(cg);
        *next = (jsbytecode)op;
        memset(next + 1, 0, BYTECODE_SIZE(extra));
        CG_NEXT(cg) = next + length;
        UpdateDepth(cx, cg, offset);
    }
    return offset;
}

/* XXX too many "... statement" L10N gaffes below -- fix via js.msg! */
const char js_with_statement_str[] = "with statement";

static const char *statementName[] = {
    "block",                 /* BLOCK */
    "label statement",       /* LABEL */
    "if statement",          /* IF */
    "else statement",        /* ELSE */
    "switch statement",      /* SWITCH */
    js_with_statement_str,   /* WITH */
    "try statement",         /* TRY */
    "catch block",           /* CATCH */
    "finally statement",     /* FINALLY */
    "do loop",               /* DO_LOOP */
    "for loop",              /* FOR_LOOP */
    "for/in loop",           /* FOR_IN_LOOP */
    "while loop",            /* WHILE_LOOP */
};

static const char *
StatementName(JSCodeGenerator *cg)
{
    if (!cg->treeContext.topStmt)
        return "script";
    return statementName[cg->treeContext.topStmt->type];
}

static void
ReportStatementTooLarge(JSContext *cx, JSCodeGenerator *cg)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEED_DIET,
                         StatementName(cg));
}

/**
  Span-dependent instructions in JS bytecode consist of the jump (JOF_JUMP)
  and switch (JOF_LOOKUPSWITCH, JOF_TABLESWITCH) format opcodes, subdivided
  into unconditional (gotos and gosubs), and conditional jumps or branches
  (which pop a value, test it, and jump depending on its value).  Most jumps
  have just one immediate operand, a signed offset from the jump opcode's pc
  to the target bytecode.  The lookup and table switch opcodes may contain
  many jump offsets.

  Mozilla bug #80981 (http://bugzilla.mozilla.org/show_bug.cgi?id=80981) was
  fixed by adding extended "X" counterparts to the opcodes/formats (NB: X is
  suffixed to prefer JSOP_ORX thereby avoiding a JSOP_XOR name collision for
  the extended form of the JSOP_OR branch opcode).  The unextended or short
  formats have 16-bit signed immediate offset operands, the extended or long
  formats have 32-bit signed immediates.  The span-dependency problem consists
  of selecting as few long instructions as possible, or about as few -- since
  jumps can span other jumps, extending one jump may cause another to need to
  be extended.

  Most JS scripts are short, so need no extended jumps.  We optimize for this
  case by generating short jumps until we know a long jump is needed.  After
  that point, we keep generating short jumps, but each jump's 16-bit immediate
  offset operand is actually an unsigned index into cg->spanDeps, an array of
  JSSpanDep structs.  Each struct tells the top offset in the script of the
  opcode, the "before" offset of the jump (which will be the same as top for
  simplex jumps, but which will index further into the bytecode array for a
  non-initial jump offset in a lookup or table switch), the after "offset"
  adjusted during span-dependent instruction selection (initially the same
  value as the "before" offset), and the jump target (more below).

  Since we generate cg->spanDeps lazily, from within js_SetJumpOffset, we must
  ensure that all bytecode generated so far can be inspected to discover where
  the jump offset immediate operands lie within CG_CODE(cg).  But the bonus is
  that we generate span-dependency records sorted by their offsets, so we can
  binary-search when trying to find a JSSpanDep for a given bytecode offset,
  or the nearest JSSpanDep at or above a given pc.

  To avoid limiting scripts to 64K jumps, if the cg->spanDeps index overflows
  65534, we store SPANDEP_INDEX_HUGE in the jump's immediate operand.  This
  tells us that we need to binary-search for the cg->spanDeps entry by the
  jump opcode's bytecode offset (sd->before).

  Jump targets need to be maintained in a data structure that lets us look
  up an already-known target by its address (jumps may have a common target),
  and that also lets us update the addresses (script-relative, a.k.a. absolute
  offsets) of targets that come after a jump target (for when a jump below
  that target needs to be extended).  We use an AVL tree, implemented using
  recursion, but with some tricky optimizations to its height-balancing code
  (see http://www.enteract.com/~bradapp/ftp/src/libs/C++/AvlTrees.html).

  A final wrinkle: backpatch chains are linked by jump-to-jump offsets with
  positive sign, even though they link "backward" (i.e., toward lower bytecode
  address).  We don't want to waste space and search time in the AVL tree for
  such temporary backpatch deltas, so we use a single-bit wildcard scheme to
  tag true JSJumpTarget pointers and encode untagged, signed (positive) deltas
  in JSSpanDep.target pointers, depending on whether the JSSpanDep has a known
  target, or is still awaiting backpatching.

  Note that backpatch chains would present a problem for BuildSpanDepTable,
  which inspects bytecode to build cg->spanDeps on demand, when the first
  short jump offset overflows.  To solve this temporary problem, we emit a
  proxy bytecode (JSOP_BACKPATCH; JSOP_BACKPATCH_PUSH for jumps that push a
  result on the interpreter's stack, namely JSOP_GOSUB; or JSOP_BACKPATCH_POP
  for branch ops) whose nuses/ndefs counts help keep the stack balanced, but
  whose opcode format distinguishes its backpatch delta immediate operand from
  a normal jump offset.
 */
static int
BalanceJumpTargets(JSJumpTarget **jtp)
{
    JSJumpTarget *jt, *jt2, *root;
    int dir, otherDir, heightChanged;
    JSBool doubleRotate;

    jt = *jtp;
    JS_ASSERT(jt->balance != 0);

    if (jt->balance < -1) {
        dir = JT_RIGHT;
        doubleRotate = (jt->kids[JT_LEFT]->balance > 0);
    } else if (jt->balance > 1) {
        dir = JT_LEFT;
        doubleRotate = (jt->kids[JT_RIGHT]->balance < 0);
    } else {
        return 0;
    }

    otherDir = JT_OTHER_DIR(dir);
    if (doubleRotate) {
        jt2 = jt->kids[otherDir];
        *jtp = root = jt2->kids[dir];

        jt->kids[otherDir] = root->kids[dir];
        root->kids[dir] = jt;

        jt2->kids[dir] = root->kids[otherDir];
        root->kids[otherDir] = jt2;

        heightChanged = 1;
        root->kids[JT_LEFT]->balance = -JS_MAX(root->balance, 0);
        root->kids[JT_RIGHT]->balance = -JS_MIN(root->balance, 0);
        root->balance = 0;
    } else {
        *jtp = root = jt->kids[otherDir];
        jt->kids[otherDir] = root->kids[dir];
        root->kids[dir] = jt;

        heightChanged = (root->balance != 0);
        jt->balance = -((dir == JT_LEFT) ? --root->balance : ++root->balance);
    }

    return heightChanged;
}

typedef struct AddJumpTargetArgs {
    JSContext           *cx;
    JSCodeGenerator     *cg;
    ptrdiff_t           offset;
    JSJumpTarget        *node;
} AddJumpTargetArgs;

static int
AddJumpTarget(AddJumpTargetArgs *args, JSJumpTarget **jtp)
{
    JSJumpTarget *jt;
    int balanceDelta;

    jt = *jtp;
    if (!jt) {
        JSCodeGenerator *cg = args->cg;

        jt = cg->jtFreeList;
        if (jt) {
            cg->jtFreeList = jt->kids[JT_LEFT];
        } else {
            JS_ARENA_ALLOCATE_CAST(jt, JSJumpTarget *, &args->cx->tempPool,
                                   sizeof *jt);
            if (!jt) {
                JS_ReportOutOfMemory(args->cx);
                return 0;
            }
        }
        jt->offset = args->offset;
        jt->balance = 0;
        jt->kids[JT_LEFT] = jt->kids[JT_RIGHT] = NULL;
        cg->numJumpTargets++;
        args->node = jt;
        *jtp = jt;
        return 1;
    }

    if (jt->offset == args->offset) {
        args->node = jt;
        return 0;
    }

    if (args->offset < jt->offset)
        balanceDelta = -AddJumpTarget(args, &jt->kids[JT_LEFT]);
    else
        balanceDelta = AddJumpTarget(args, &jt->kids[JT_RIGHT]);
    if (!args->node)
        return 0;

    jt->balance += balanceDelta;
    return (balanceDelta && jt->balance)
           ? 1 - BalanceJumpTargets(jtp)
           : 0;
}

#ifdef DEBUG_brendan
static int AVLCheck(JSJumpTarget *jt)
{
    int lh, rh;

    if (!jt) return 0;
    JS_ASSERT(-1 <= jt->balance && jt->balance <= 1);
    lh = AVLCheck(jt->kids[JT_LEFT]);
    rh = AVLCheck(jt->kids[JT_RIGHT]);
    JS_ASSERT(jt->balance == rh - lh);
    return 1 + JS_MAX(lh, rh);
}
#endif

static JSBool
SetSpanDepTarget(JSContext *cx, JSCodeGenerator *cg, JSSpanDep *sd,
                 ptrdiff_t off)
{
    AddJumpTargetArgs args;

    if (off < JUMPX_OFFSET_MIN || JUMPX_OFFSET_MAX < off) {
        ReportStatementTooLarge(cx, cg);
        return JS_FALSE;
    }

    args.cx = cx;
    args.cg = cg;
    args.offset = sd->top + off;
    args.node = NULL;
    AddJumpTarget(&args, &cg->jumpTargets);
    if (!args.node)
        return JS_FALSE;

#ifdef DEBUG_brendan
    AVLCheck(cg->jumpTargets);
#endif

    SD_SET_TARGET(sd, args.node);
    return JS_TRUE;
}

#define SPANDEPS_MIN            256
#define SPANDEPS_SIZE(n)        ((n) * sizeof(JSSpanDep))
#define SPANDEPS_SIZE_MIN       SPANDEPS_SIZE(SPANDEPS_MIN)

static JSBool
AddSpanDep(JSContext *cx, JSCodeGenerator *cg, jsbytecode *pc, jsbytecode *pc2,
           ptrdiff_t off)
{
    uintN index;
    JSSpanDep *sdbase, *sd;
    size_t size;

    index = cg->numSpanDeps;
    if (index + 1 == 0) {
        ReportStatementTooLarge(cx, cg);
        return JS_FALSE;
    }

    if ((index & (index - 1)) == 0 &&
        (!(sdbase = cg->spanDeps) || index >= SPANDEPS_MIN)) {
        if (!sdbase) {
            size = SPANDEPS_SIZE_MIN;
            JS_ARENA_ALLOCATE_CAST(sdbase, JSSpanDep *, &cx->tempPool, size);
        } else {
            size = SPANDEPS_SIZE(index);
            JS_ARENA_GROW_CAST(sdbase, JSSpanDep *, &cx->tempPool, size, size);
        }
        if (!sdbase)
            return JS_FALSE;
        cg->spanDeps = sdbase;
    }

    cg->numSpanDeps = index + 1;
    sd = cg->spanDeps + index;
    sd->top = PTRDIFF(pc, CG_BASE(cg), jsbytecode);
    sd->offset = sd->before = PTRDIFF(pc2, CG_BASE(cg), jsbytecode);

    if (js_CodeSpec[*pc].format & JOF_BACKPATCH) {
        /* Jump offset will be backpatched if off is a non-zero "bpdelta". */
        if (off != 0) {
            JS_ASSERT(off >= 1 + JUMP_OFFSET_LEN);
            if (off > BPDELTA_MAX) {
                ReportStatementTooLarge(cx, cg);
                return JS_FALSE;
            }
        }
        SD_SET_BPDELTA(sd, off);
    } else if (off == 0) {
        /* Jump offset will be patched directly, without backpatch chaining. */
        SD_SET_TARGET(sd, NULL);
    } else {
        /* The jump offset in off is non-zero, therefore it's already known. */
        if (!SetSpanDepTarget(cx, cg, sd, off))
            return JS_FALSE;
    }

    if (index > SPANDEP_INDEX_MAX)
        index = SPANDEP_INDEX_HUGE;
    SET_SPANDEP_INDEX(pc2, index);
    return JS_TRUE;
}

static JSBool
BuildSpanDepTable(JSContext *cx, JSCodeGenerator *cg)
{
    jsbytecode *pc, *end;
    JSOp op;
    JSCodeSpec *cs;
    ptrdiff_t len, off;

    pc = CG_BASE(cg);
    end = CG_NEXT(cg);
    while (pc < end) {
        op = (JSOp)*pc;
        cs = &js_CodeSpec[op];
        len = (ptrdiff_t)cs->length;

        switch (cs->format & JOF_TYPEMASK) {
          case JOF_JUMP:
            off = GET_JUMP_OFFSET(pc);
            if (!AddSpanDep(cx, cg, pc, pc, off))
                return JS_FALSE;
            break;

#if JS_HAS_SWITCH_STATEMENT
          case JOF_TABLESWITCH:
          {
            jsbytecode *pc2;
            jsint i, low, high;

            pc2 = pc;
            off = GET_JUMP_OFFSET(pc2);
            if (!AddSpanDep(cx, cg, pc, pc2, off))
                return JS_FALSE;
            pc2 += JUMP_OFFSET_LEN;
            low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            for (i = low; i <= high; i++) {
                off = GET_JUMP_OFFSET(pc2);
                if (!AddSpanDep(cx, cg, pc, pc2, off))
                    return JS_FALSE;
                pc2 += JUMP_OFFSET_LEN;
            }
            len = 1 + pc2 - pc;
            break;
          }

          case JOF_LOOKUPSWITCH:
          {
            jsbytecode *pc2;
            jsint npairs;

            pc2 = pc;
            off = GET_JUMP_OFFSET(pc2);
            if (!AddSpanDep(cx, cg, pc, pc2, off))
                return JS_FALSE;
            pc2 += JUMP_OFFSET_LEN;
            npairs = (jsint) GET_ATOM_INDEX(pc2);
            pc2 += ATOM_INDEX_LEN;
            while (npairs) {
                pc2 += ATOM_INDEX_LEN;
                off = GET_JUMP_OFFSET(pc2);
                if (!AddSpanDep(cx, cg, pc, pc2, off))
                    return JS_FALSE;
                pc2 += JUMP_OFFSET_LEN;
                npairs--;
            }
            len = 1 + pc2 - pc;
            break;
          }
#endif /* JS_HAS_SWITCH_STATEMENT */
        }

        pc += len;
    }

    return JS_TRUE;
}

static JSSpanDep *
GetSpanDep(JSCodeGenerator *cg, jsbytecode *pc)
{
    uintN index;
    ptrdiff_t offset;
    int lo, hi, mid;
    JSSpanDep *sd;

    index = GET_SPANDEP_INDEX(pc);
    if (index != SPANDEP_INDEX_HUGE)
        return cg->spanDeps + index;

    offset = PTRDIFF(pc, CG_BASE(cg), jsbytecode);
    lo = 0;
    hi = cg->numSpanDeps - 1;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        sd = cg->spanDeps + mid;
        if (sd->before == offset)
            return sd;
        if (sd->before < offset)
            lo = mid + 1;
        else
            hi = mid - 1;
    }

    JS_ASSERT(0);
    return NULL;
}

static JSBool
SetBackPatchDelta(JSContext *cx, JSCodeGenerator *cg, jsbytecode *pc,
                  ptrdiff_t delta)
{
    JSSpanDep *sd;

    JS_ASSERT(delta >= 1 + JUMP_OFFSET_LEN);
    if (!cg->spanDeps && delta < JUMP_OFFSET_MAX) {
        SET_JUMP_OFFSET(pc, delta);
        return JS_TRUE;
    }

    if (delta > BPDELTA_MAX) {
        ReportStatementTooLarge(cx, cg);
        return JS_FALSE;
    }

    if (!cg->spanDeps && !BuildSpanDepTable(cx, cg))
        return JS_FALSE;

    sd = GetSpanDep(cg, pc);
    JS_ASSERT(SD_GET_BPDELTA(sd) == 0);
    SD_SET_BPDELTA(sd, delta);
    return JS_TRUE;
}

static void
UpdateJumpTargets(JSJumpTarget *jt, ptrdiff_t pivot, ptrdiff_t delta)
{
    if (jt->offset > pivot) {
        jt->offset += delta;
        if (jt->kids[JT_LEFT])
            UpdateJumpTargets(jt->kids[JT_LEFT], pivot, delta);
    }
    if (jt->kids[JT_RIGHT])
        UpdateJumpTargets(jt->kids[JT_RIGHT], pivot, delta);
}

static JSSpanDep *
FindNearestSpanDep(JSCodeGenerator *cg, ptrdiff_t offset, int lo,
                   JSSpanDep *guard)
{
    int num, hi, mid;
    JSSpanDep *sdbase, *sd;

    num = cg->numSpanDeps;
    JS_ASSERT(num > 0);
    hi = num - 1;
    sdbase = cg->spanDeps;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        sd = sdbase + mid;
        if (sd->before == offset)
            return sd;
        if (sd->before < offset)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    if (lo == num)
        return guard;
    sd = sdbase + lo;
    JS_ASSERT(sd->before >= offset && (lo == 0 || sd[-1].before < offset));
    return sd;
}

static void
FreeJumpTargets(JSCodeGenerator *cg, JSJumpTarget *jt)
{
    if (jt->kids[JT_LEFT])
        FreeJumpTargets(cg, jt->kids[JT_LEFT]);
    if (jt->kids[JT_RIGHT])
        FreeJumpTargets(cg, jt->kids[JT_RIGHT]);
    jt->kids[JT_LEFT] = cg->jtFreeList;
    cg->jtFreeList = jt;
}

static JSBool
OptimizeSpanDeps(JSContext *cx, JSCodeGenerator *cg)
{
    jsbytecode *pc, *oldpc, *base, *limit, *next;
    JSSpanDep *sd, *sd2, *sdbase, *sdlimit, *sdtop, guard;
    ptrdiff_t offset, growth, delta, top, pivot, span, length, target;
    JSBool done;
    JSOp op;
    uint32 type;
    size_t size, incr;
    jssrcnote *sn, *snlimit;
    JSSrcNoteSpec *spec;
    uintN i, n, noteIndex;
    JSTryNote *tn, *tnlimit;
#ifdef DEBUG_brendan
    int passes = 0;
#endif

    base = CG_BASE(cg);
    sdbase = cg->spanDeps;
    sdlimit = sdbase + cg->numSpanDeps;
    offset = CG_OFFSET(cg);
    growth = 0;

    do {
        done = JS_TRUE;
        delta = 0;
        top = pivot = -1;
        sdtop = NULL;
        pc = NULL;
        op = JSOP_NOP;
        type = 0;
#ifdef DEBUG_brendan
        passes++;
#endif

        for (sd = sdbase; sd < sdlimit; sd++) {
            JS_ASSERT(JT_HAS_TAG(sd->target));
            sd->offset += delta;

            if (sd->top != top) {
                sdtop = sd;
                top = sd->top;
                JS_ASSERT(top == sd->before);
                pivot = sd->offset;
                pc = base + top;
                op = (JSOp) *pc;
                type = (js_CodeSpec[op].format & JOF_TYPEMASK);
                if (JOF_TYPE_IS_EXTENDED_JUMP(type)) {
                    /*
                     * We already extended all the jump offset operands for
                     * the opcode at sd->top.  Jumps and branches have only
                     * one jump offset operand, but switches have many, all
                     * of which are adjacent in cg->spanDeps.
                     */
                    continue;
                }

                JS_ASSERT(type == JOF_JUMP ||
                          type == JOF_TABLESWITCH ||
                          type == JOF_LOOKUPSWITCH);
            }

            if (!JOF_TYPE_IS_EXTENDED_JUMP(type)) {
                span = SD_TARGET_OFFSET(sd) - pivot;
                if (span < JUMP_OFFSET_MIN || JUMP_OFFSET_MAX < span) {
                    done = JS_FALSE;

                    switch (op) {
                      case JSOP_GOTO:         op = JSOP_GOTOX; break;
                      case JSOP_IFEQ:         op = JSOP_IFEQX; break;
                      case JSOP_IFNE:         op = JSOP_IFNEX; break;
                      case JSOP_OR:           op = JSOP_ORX; break;
                      case JSOP_AND:          op = JSOP_ANDX; break;
                      case JSOP_GOSUB:        op = JSOP_GOSUBX; break;
                      case JSOP_CASE:         op = JSOP_CASEX; break;
                      case JSOP_DEFAULT:      op = JSOP_DEFAULTX; break;
                      case JSOP_TABLESWITCH:  op = JSOP_TABLESWITCHX; break;
                      case JSOP_LOOKUPSWITCH: op = JSOP_LOOKUPSWITCHX; break;
                      default:                JS_ASSERT(0);
                    }
                    *pc = (jsbytecode) op;

                    for (sd2 = sdtop; sd2 < sdlimit && sd2->top == top; sd2++) {
                        if (sd2 > sd)
                            sd2->offset += delta;

                        delta += JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN;
                        UpdateJumpTargets(cg->jumpTargets, sd2->offset,
                                          JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN);
                    }
                    sd = sd2 - 1;
                }
            }
        }

        growth += delta;
    } while (!done);

    if (growth) {
#ifdef DEBUG_brendan
        printf("%s:%u: %u/%u jumps extended in %d passes (%d=%d+%d)\n",
               cg->filename ? cg->filename : "stdin", cg->firstLine,
               growth / (JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN), cg->numSpanDeps,
               passes, offset + growth, offset, growth);
#endif

        /*
         * Ensure that we have room for the extended jumps, but don't round up
         * to a power of two -- we're done generating code, so we cut to fit.
         */
        limit = CG_LIMIT(cg);
        length = offset + growth;
        next = base + length;
        if (next > limit) {
            JS_ASSERT(length > BYTECODE_GRAIN);
            size = BYTECODE_SIZE(PTRDIFF(limit, base, jsbytecode));
            incr = BYTECODE_SIZE(length) - size;
            JS_ARENA_GROW_CAST(base, jsbytecode *, &cx->codePool, size, incr);
            if (!base) {
                JS_ReportOutOfMemory(cx);
                return JS_FALSE;
            }
            CG_BASE(cg) = base;
            CG_LIMIT(cg) = next = base + length;
        }
        CG_NEXT(cg) = next;

        /*
         * Set up a fake span dependency record to guard the end of the code
         * being generated.  This guard record is returned as a fencepost by
         * FindNearestSpanDep if there is no real spandep at or above a given
         * unextended code offset.
         */
        guard.top = -1;
        guard.offset = offset + growth;
        guard.before = offset;
        guard.target = NULL;
    }

    /*
     * Now work backwards through the span dependencies, copying chunks of
     * bytecode between each extended jump toward the end of the grown code
     * space, and restoring immediate offset operands for all jump bytecodes.
     * The first chunk of bytecodes, starting at base and ending at the first
     * extended jump offset (NB: this chunk includes the operation bytecode
     * just before that immediate jump offset), doesn't need to be copied.
     */
    JS_ASSERT(sd == sdlimit);
    top = -1;
    while (--sd >= sdbase) {
        if (sd->top != top) {
            top = sd->top;
            op = (JSOp) base[top];
            type = (js_CodeSpec[op].format & JOF_TYPEMASK);

            for (sd2 = sd - 1; sd2 >= sdbase && sd2->top == top; sd2--)
                continue;
            sd2++;
            pivot = sd2->offset;
            JS_ASSERT(top == sd2->before);
        }

        oldpc = base + sd->before;
        span = SD_TARGET_OFFSET(sd) - pivot;

        /*
         * If this jump didn't need to be extended, restore its span immediate
         * offset operand now, overwriting the index of sd within cg->spanDeps
         * that was stored temporarily after *pc when BuildSpanDepTable ran.
         *
         * Note that span might fit in 16 bits even for an extended jump op,
         * if the op has multiple span operands, not all of which overflowed
         * (e.g. JSOP_LOOKUPSWITCH or JSOP_TABLESWITCH where some cases are in
         * range for a short jump, but others are not).
         */
        if (!JOF_TYPE_IS_EXTENDED_JUMP(type)) {
            JS_ASSERT(JUMP_OFFSET_MIN <= span && span <= JUMP_OFFSET_MAX);
            SET_JUMP_OFFSET(oldpc, span);
            continue;
        }

        /*
         * Set up parameters needed to copy the next run of bytecode starting
         * at offset (which is a cursor into the unextended, original bytecode
         * vector), down to sd->before (a cursor of the same scale as offset,
         * it's the index of the original jump pc).  Reuse delta to count the
         * nominal number of bytes to copy.
         */
        pc = base + sd->offset;
        delta = offset - sd->before;
        JS_ASSERT(delta >= 1 + JUMP_OFFSET_LEN);

        /*
         * Don't bother copying the jump offset we're about to reset, but do
         * copy the bytecode at oldpc (which comes just before its immediate
         * jump offset operand), on the next iteration through the loop, by
         * including it in offset's new value.
         */
        offset = sd->before + 1;
        size = BYTECODE_SIZE(delta - (1 + JUMP_OFFSET_LEN));
        if (size) {
            memmove(pc + 1 + JUMPX_OFFSET_LEN,
                    oldpc + 1 + JUMP_OFFSET_LEN,
                    size);
        }

        SET_JUMPX_OFFSET(pc, span);
    }

    if (growth) {
        /*
         * Fix source note deltas.  Don't hardwire the delta fixup adjustment,
         * even though currently it must be JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN
         * at each sd that moved.  The future may bring different offset sizes
         * for span-dependent instruction operands.
         */
        offset = growth = 0;
        sd = sdbase;
        for (sn = cg->notes, snlimit = sn + cg->noteCount;
             sn < snlimit;
             sn = SN_NEXT(sn)) {
            /*
             * Recall that the offset of a given note includes its delta, and
             * tells the offset of the annotated bytecode from the main entry
             * point of the script.
             */
            offset += SN_DELTA(sn);
            while (sd < sdlimit && sd->before < offset) {
                /*
                 * To compute the delta to add to sn, we need to look at the
                 * spandep after sd, whose offset - (before + growth) tells by
                 * how many bytes sd's instruction grew.
                 */
                sd2 = sd + 1;
                if (sd2 == sdlimit)
                    sd2 = &guard;
                delta = sd2->offset - (sd2->before + growth);
                if (delta > 0) {
                    JS_ASSERT(delta == JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN);
                    sn = js_AddToSrcNoteDelta(cx, cg, sn, delta);
                    if (!sn)
                        return JS_FALSE;
                    snlimit = cg->notes + cg->noteCount;
                    growth += delta;
                }
                sd++;
            }

            /*
             * If sn has span-dependent offset operands, check whether each
             * covers further span-dependencies, and increase those operands
             * accordingly.  Some source notes measure offset not from the
             * annotated pc, but from that pc plus some small bias.  NB: we
             * assume that spec->offsetBias can't itself span span-dependent
             * instructions!
             */
            spec = &js_SrcNoteSpec[SN_TYPE(sn)];
            if (spec->isSpanDep) {
                pivot = offset + spec->offsetBias;
                n = spec->arity;
                for (i = 0; i < n; i++) {
                    span = js_GetSrcNoteOffset(sn, i);
                    if (span == 0)
                        continue;
                    target = pivot + span * spec->isSpanDep;
                    sd2 = FindNearestSpanDep(cg, target,
                                             (target >= pivot)
                                             ? sd - sdbase
                                             : 0,
                                             &guard);

                    /*
                     * Increase target by sd2's before-vs-after offset delta,
                     * which is absolute (i.e., relative to start of script,
                     * as is target).  Recompute the span by subtracting its
                     * adjusted pivot from target.
                     */
                    target += sd2->offset - sd2->before;
                    span = target - (pivot + growth);
                    span *= spec->isSpanDep;
                    noteIndex = sn - cg->notes;
                    if (!js_SetSrcNoteOffset(cx, cg, noteIndex, i, span))
                        return JS_FALSE;
                    sn = cg->notes + noteIndex;
                    snlimit = cg->notes + cg->noteCount;
                }
            }
        }

        /*
         * Fix try/catch notes (O(numTryNotes * log2(numSpanDeps)), but it's
         * not clear how we can beat that).
         */
        for (tn = cg->tryBase, tnlimit = cg->tryNext; tn < tnlimit; tn++) {
            /*
             * First, look for the nearest span dependency at/above tn->start.
             * There may not be any such spandep, in which case the guard will
             * be returned.
             */
            offset = tn->start;
            sd = FindNearestSpanDep(cg, offset, 0, &guard);
            delta = sd->offset - sd->before;
            tn->start = offset + delta;

            /*
             * Next, find the nearest spandep at/above tn->start + tn->length.
             * Use its delta minus tn->start's delta to increase tn->length.
             */
            length = tn->length;
            sd2 = FindNearestSpanDep(cg, offset + length, sd - sdbase, &guard);
            if (sd2 != sd)
                tn->length = length + sd2->offset - sd2->before - delta;

            /*
             * Finally, adjust tn->catchStart upward only if it is non-zero,
             * and provided there are spandeps below it that grew.
             */
            offset = tn->catchStart;
            if (offset != 0) {
                sd = FindNearestSpanDep(cg, offset, sd2 - sdbase, &guard);
                tn->catchStart = offset + sd->offset - sd->before;
            }
        }
    }

#ifdef DEBUG_brendan
  {
    uintN bigspans = 0;
    top = -1;
    for (sd = sdbase; sd < sdlimit; sd++) {
        offset = sd->offset;

        /* NB: sd->top cursors into the original, unextended bytecode vector. */
        if (sd->top != top) {
            JS_ASSERT(top == -1 ||
                      !JOF_TYPE_IS_EXTENDED_JUMP(type) ||
                      bigspans != 0);
            bigspans = 0;
            top = sd->top;
            JS_ASSERT(top == sd->before);
            op = (JSOp) base[offset];
            type = (js_CodeSpec[op].format & JOF_TYPEMASK);
            JS_ASSERT(type == JOF_JUMP ||
                      type == JOF_JUMPX ||
                      type == JOF_TABLESWITCH ||
                      type == JOF_TABLESWITCHX ||
                      type == JOF_LOOKUPSWITCH ||
                      type == JOF_LOOKUPSWITCHX);
            pivot = offset;
        }

        pc = base + offset;
        if (JOF_TYPE_IS_EXTENDED_JUMP(type)) {
            span = GET_JUMPX_OFFSET(pc);
            if (span < JUMP_OFFSET_MIN || JUMP_OFFSET_MAX < span) {
                bigspans++;
            } else {
                JS_ASSERT(type == JOF_TABLESWITCHX ||
                          type == JOF_LOOKUPSWITCHX);
            }
        } else {
            span = GET_JUMP_OFFSET(pc);
        }
        JS_ASSERT(SD_TARGET_OFFSET(sd) == pivot + span);
    }
    JS_ASSERT(!JOF_TYPE_IS_EXTENDED_JUMP(type) || bigspans != 0);
  }
#endif

    /*
     * Reset so we optimize at most once -- cg may be used for further code
     * generation of successive, independent, top-level statements.  No jump
     * can span top-level statements, because JS lacks goto.
     */
    size = SPANDEPS_SIZE(JS_BIT(JS_CeilingLog2(cg->numSpanDeps)));
    JS_ArenaFreeAllocation(&cx->tempPool, cg->spanDeps,
                           JS_MAX(size, SPANDEPS_SIZE_MIN));
    cg->spanDeps = NULL;
    FreeJumpTargets(cg, cg->jumpTargets);
    cg->jumpTargets = NULL;
    cg->numSpanDeps = cg->numJumpTargets = 0;
    return JS_TRUE;
}

static JSBool
EmitJump(JSContext *cx, JSCodeGenerator *cg, JSOp op, ptrdiff_t off)
{
    ptrdiff_t jmp;
    jsbytecode *pc;

    if (off < JUMP_OFFSET_MIN || JUMP_OFFSET_MAX < off) {
        if (!cg->spanDeps && !BuildSpanDepTable(cx, cg))
            return JS_FALSE;
    }

    jmp = js_Emit3(cx, cg, op, JUMP_OFFSET_HI(off), JUMP_OFFSET_LO(off));
    if (jmp >= 0 && cg->spanDeps) {
        pc = CG_CODE(cg, jmp);
        if (!AddSpanDep(cx, cg, pc, pc, off))
            return JS_FALSE;
    }
    return jmp;
}

static ptrdiff_t
GetJumpOffset(JSCodeGenerator *cg, jsbytecode *pc)
{
    JSSpanDep *sd;
    JSJumpTarget *jt;
    ptrdiff_t top;

    if (!cg->spanDeps)
        return GET_JUMP_OFFSET(pc);

    sd = GetSpanDep(cg, pc);
    jt = sd->target;
    if (!JT_HAS_TAG(jt))
        return JT_TO_BPDELTA(jt);

    top = sd->top;
    while (--sd >= cg->spanDeps && sd->top == top)
        continue;
    sd++;
    return JT_CLR_TAG(jt)->offset - sd->offset;
}

JSBool
js_SetJumpOffset(JSContext *cx, JSCodeGenerator *cg, jsbytecode *pc,
                 ptrdiff_t off)
{
    if (!cg->spanDeps) {
        if (JUMP_OFFSET_MIN <= off && off <= JUMP_OFFSET_MAX) {
            SET_JUMP_OFFSET(pc, off);
            return JS_TRUE;
        }

        if (!BuildSpanDepTable(cx, cg))
            return JS_FALSE;
    }

    return SetSpanDepTarget(cx, cg, GetSpanDep(cg, pc), off);
}

JSBool
js_InWithStatement(JSTreeContext *tc)
{
    JSStmtInfo *stmt;

    for (stmt = tc->topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == STMT_WITH)
            return JS_TRUE;
    }
    return JS_FALSE;
}

JSBool
js_InCatchBlock(JSTreeContext *tc, JSAtom *atom)
{
    JSStmtInfo *stmt;

    for (stmt = tc->topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == STMT_CATCH && stmt->label == atom)
            return JS_TRUE;
    }
    return JS_FALSE;
}

void
js_PushStatement(JSTreeContext *tc, JSStmtInfo *stmt, JSStmtType type,
                 ptrdiff_t top)
{
    stmt->type = type;
    SET_STATEMENT_TOP(stmt, top);
    stmt->label = NULL;
    stmt->down = tc->topStmt;
    tc->topStmt = stmt;
}

/*
 * Emit a backpatch op with offset pointing to the previous jump of this type,
 * so that we can walk back up the chain fixing up the op and jump offset.
 */
#define EMIT_BACKPATCH_OP(cx, cg, last, op, jmp)                              \
    JS_BEGIN_MACRO                                                            \
        ptrdiff_t offset, delta;                                              \
        offset = CG_OFFSET(cg);                                               \
        delta = offset - (last);                                              \
        last = offset;                                                        \
        JS_ASSERT(delta > 0);                                                 \
        jmp = EmitJump((cx), (cg), (op), (delta));                            \
    JS_END_MACRO

/* Emit additional bytecode(s) for non-local jumps. */
static JSBool
EmitNonLocalJumpFixup(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *toStmt,
                      JSOp *returnop)
{
    intN depth;
    JSStmtInfo *stmt;
    ptrdiff_t jmp;

    /*
     * Return from within a try block that has a finally clause must be split
     * into two ops: JSOP_SETRVAL, to pop the r.v. and store it in fp->rval;
     * and JSOP_RETRVAL, which makes control flow go back to the caller, who
     * picks up fp->rval as usual.  Otherwise, the stack will be unbalanced
     * when executing the finally clause.
     *
     * We mutate *returnop once only if we find an enclosing try-block (viz,
     * STMT_FINALLY) to ensure that we emit just one JSOP_SETRVAL before one
     * or more JSOP_GOSUBs and other fixup opcodes emitted by this function.
     * Our caller (the TOK_RETURN case of js_EmitTree) then emits *returnop.
     * The fixup opcodes and gosubs must interleave in the proper order, from
     * inner statement to outer, so that finally clauses run at the correct
     * stack depth.
     */
    if (returnop) {
        JS_ASSERT(*returnop == JSOP_RETURN);
        for (stmt = cg->treeContext.topStmt; stmt != toStmt;
             stmt = stmt->down) {
            if (stmt->type == STMT_FINALLY) {
                if (js_Emit1(cx, cg, JSOP_SETRVAL) < 0)
                    return JS_FALSE;
                *returnop = JSOP_RETRVAL;
                break;
            }
        }

        /*
         * If there are no try-with-finally blocks open around this return
         * statement, we can generate a return forthwith and skip generating
         * any fixup code.
         */
        if (*returnop == JSOP_RETURN)
            return JS_TRUE;
    }

    /*
     * The non-local jump fixup we emit will unbalance cg->stackDepth, because
     * the fixup replicates balanced code such as JSOP_LEAVEWITH emitted at the
     * end of a with statement, so we save cg->stackDepth here and restore it
     * just before a successful return.
     */
    depth = cg->stackDepth;
    for (stmt = cg->treeContext.topStmt; stmt != toStmt; stmt = stmt->down) {
        switch (stmt->type) {
          case STMT_FINALLY:
            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                return JS_FALSE;
            EMIT_BACKPATCH_OP(cx, cg, stmt->gosub, JSOP_BACKPATCH_PUSH, jmp);
            if (jmp < 0)
                return JS_FALSE;
            break;

          case STMT_WITH:
          case STMT_CATCH:
            /* There's a With object on the stack that we need to pop. */
            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                return JS_FALSE;
            if (js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0)
                return JS_FALSE;
            break;

          case STMT_FOR_IN_LOOP:
            /*
             * The iterator and the object being iterated need to be popped.
             * JSOP_POP2 isn't decompiled, so it doesn't need to be HIDDEN.
             */
            if (js_Emit1(cx, cg, JSOP_POP2) < 0)
                return JS_FALSE;
            break;

          case STMT_SUBROUTINE:
            /* There's a retsub pc-offset on the stack that we need to pop. */
            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                return JS_FALSE;
            if (js_Emit1(cx, cg, JSOP_POP) < 0)
                return JS_FALSE;
            break;

          default:;
        }
    }

    cg->stackDepth = depth;
    return JS_TRUE;
}

static ptrdiff_t
EmitGoto(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *toStmt,
         ptrdiff_t *last, JSAtomListElement *label, JSSrcNoteType noteType)
{
    intN index;
    ptrdiff_t jmp;

    if (!EmitNonLocalJumpFixup(cx, cg, toStmt, NULL))
        return -1;

    if (label) {
        index = js_NewSrcNote(cx, cg, noteType);
        if (index < 0)
            return -1;
        if (!js_SetSrcNoteOffset(cx, cg, (uintN)index, 0,
                                 (ptrdiff_t) ALE_INDEX(label))) {
            return -1;
        }
    } else if (noteType != SRC_NULL) {
        if (js_NewSrcNote(cx, cg, noteType) < 0)
            return -1;
    }

    EMIT_BACKPATCH_OP(cx, cg, *last, JSOP_BACKPATCH, jmp);
    return jmp;
}

static JSBool
BackPatch(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t last,
          jsbytecode *target, jsbytecode op)
{
    jsbytecode *pc;
    ptrdiff_t delta, span;

    pc = CG_CODE(cg, last);
    while (pc != CG_CODE(cg, -1)) {
        delta = GetJumpOffset(cg, pc);
        span = PTRDIFF(target, pc, jsbytecode);
        CHECK_AND_SET_JUMP_OFFSET(cx, cg, pc, span);

        /*
         * Set *pc after jump offset in case bpdelta didn't overflow, but span
         * does (if so, CHECK_AND_SET_JUMP_OFFSET might call BuildSpanDepTable
         * and need to see the JSOP_BACKPATCH* op at *pc).
         */
        *pc = op;
        pc -= delta;
    }
    return JS_TRUE;
}

void
js_PopStatement(JSTreeContext *tc)
{
    tc->topStmt = tc->topStmt->down;
}

JSBool
js_PopStatementCG(JSContext *cx, JSCodeGenerator *cg)
{
    JSStmtInfo *stmt;

    stmt = cg->treeContext.topStmt;
    if (!BackPatch(cx, cg, stmt->breaks, CG_NEXT(cg), JSOP_GOTO) ||
        !BackPatch(cx, cg, stmt->continues, CG_CODE(cg, stmt->update),
                   JSOP_GOTO)) {
        return JS_FALSE;
    }
    js_PopStatement(&cg->treeContext);
    return JS_TRUE;
}

/*
 * Emit a bytecode and its 2-byte constant (atom) index immediate operand.
 * NB: We use cx and cg from our caller's lexical environment, and return
 * false on error.
 */
#define EMIT_ATOM_INDEX_OP(op, atomIndex)                                     \
    JS_BEGIN_MACRO                                                            \
        if (js_Emit3(cx, cg, op, ATOM_INDEX_HI(atomIndex),                    \
                                 ATOM_INDEX_LO(atomIndex)) < 0) {             \
            return JS_FALSE;                                                  \
        }                                                                     \
    JS_END_MACRO

static JSBool
EmitAtomOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSAtomListElement *ale;

    ale = js_IndexAtom(cx, pn->pn_atom, &cg->atomList);
    if (!ale)
        return JS_FALSE;
    EMIT_ATOM_INDEX_OP(op, ALE_INDEX(ale));
    return JS_TRUE;
}

/*
 * This routine tries to optimize name gets and sets to stack slot loads and
 * stores, given the variables object and scope chain in cx's top frame, the
 * compile-time context in tc, and a TOK_NAME node pn.  It returns false on
 * error, true on success.
 *
 * The caller can inspect pn->pn_slot for a non-negative slot number to tell
 * whether optimization occurred, in which case LookupArgOrVar also updated
 * pn->pn_op.  If pn->pn_slot is still -1 on return, pn->pn_op nevertheless
 * may have been optimized, e.g., from JSOP_NAME to JSOP_ARGUMENTS.  Whether
 * or not pn->pn_op was modified, if this function finds an argument or local
 * variable name, pn->pn_attrs will contain the property's attributes after a
 * successful return.
 */
static JSBool
LookupArgOrVar(JSContext *cx, JSTreeContext *tc, JSParseNode *pn)
{
    JSObject *obj, *pobj;
    JSClass *clasp;
    JSAtom *atom;
    JSScopeProperty *sprop;
    JSOp op;

    JS_ASSERT(pn->pn_type == TOK_NAME);
    if (pn->pn_slot >= 0 || pn->pn_op == JSOP_ARGUMENTS)
        return JS_TRUE;

    /*
     * We can't optimize if var and closure (a local function not in a larger
     * expression and not at top-level within another's body) collide.
     * XXX suboptimal: keep track of colliding names and deoptimize only those
     */
    if (tc->flags & TCF_FUN_CLOSURE_VS_VAR)
        return JS_TRUE;

    /*
     * We can't optimize if we're not compiling a function body, whether via
     * eval, or directly when compiling a function statement or expression.
     */
    obj = cx->fp->varobj;
    clasp = OBJ_GET_CLASS(cx, obj);
    if (clasp != &js_FunctionClass && clasp != &js_CallClass)
        return JS_TRUE;

    /*
     * We can't optimize if we're in an eval called inside a with statement,
     * or we're compiling a with statement and its body, or we're in a catch
     * block whose exception variable has the same name as pn.
     */
    atom = pn->pn_atom;
    if (cx->fp->scopeChain != obj ||
        js_InWithStatement(tc) ||
        js_InCatchBlock(tc, atom)) {
        return JS_TRUE;
    }

    /*
     * Ok, we may be able to optimize name to stack slot. Look for an argument
     * or variable property in the function, or its call object, not found in
     * any prototype object.  Rewrite pn_op and update pn accordingly.  NB: We
     * know that JSOP_DELNAME on an argument or variable must evaluate to
     * false, due to JSPROP_PERMANENT.
     */
    if (!js_LookupProperty(cx, obj, (jsid)atom, &pobj, (JSProperty **)&sprop))
        return JS_FALSE;
    op = pn->pn_op;
    if (sprop) {
        if (pobj == obj) {
            JSPropertyOp getter = sprop->getter;

            if (getter == js_GetArgument) {
                switch (op) {
                  case JSOP_NAME:     op = JSOP_GETARG; break;
                  case JSOP_SETNAME:  op = JSOP_SETARG; break;
                  case JSOP_INCNAME:  op = JSOP_INCARG; break;
                  case JSOP_NAMEINC:  op = JSOP_ARGINC; break;
                  case JSOP_DECNAME:  op = JSOP_DECARG; break;
                  case JSOP_NAMEDEC:  op = JSOP_ARGDEC; break;
                  case JSOP_FORNAME:  op = JSOP_FORARG; break;
                  case JSOP_DELNAME:  op = JSOP_FALSE; break;
                  default: JS_ASSERT(0);
                }
            } else if (getter == js_GetLocalVariable ||
                       getter == js_GetCallVariable)
            {
                switch (op) {
                  case JSOP_NAME:     op = JSOP_GETVAR; break;
                  case JSOP_SETNAME:  op = JSOP_SETVAR; break;
                  case JSOP_SETCONST: op = JSOP_SETVAR; break;
                  case JSOP_INCNAME:  op = JSOP_INCVAR; break;
                  case JSOP_NAMEINC:  op = JSOP_VARINC; break;
                  case JSOP_DECNAME:  op = JSOP_DECVAR; break;
                  case JSOP_NAMEDEC:  op = JSOP_VARDEC; break;
                  case JSOP_FORNAME:  op = JSOP_FORVAR; break;
                  case JSOP_DELNAME:  op = JSOP_FALSE; break;
                  default: JS_ASSERT(0);
                }
            }
            if (op != pn->pn_op) {
                pn->pn_op = op;
                pn->pn_slot = sprop->shortid;
            }
            pn->pn_attrs = sprop->attrs;
        }
        OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
    }

    if (pn->pn_slot < 0) {
        /*
         * We couldn't optimize it, so it's not an arg or local var name.  Now
         * we must check for the predefined arguments variable.  It may be
         * overridden by assignment, in which case the function is heavyweight
         * and the interpreter will look up 'arguments' in the function's call
         * object.
         */
        if (pn->pn_op == JSOP_NAME &&
            atom == cx->runtime->atomState.argumentsAtom) {
            pn->pn_op = JSOP_ARGUMENTS;
            return JS_TRUE;
        }

        tc->flags |= TCF_FUN_USES_NONLOCALS;
    }
    return JS_TRUE;
}

/*
 * If pn contains a useful expression, return true with *answer set to true.
 * If pn contains a useless expression, return true with *answer set to false.
 * Return false on error.
 *
 * The caller should initialize *answer to false and invoke this function on
 * an expression statement or similar subtree to decide whether the tree could
 * produce code that has any side effects.  For an expression statement, we
 * define useless code as code with no side effects, because the main effect,
 * the value left on the stack after the code executes, will be discarded by a
 * pop bytecode.
 */
static JSBool
CheckSideEffects(JSContext *cx, JSTreeContext *tc, JSParseNode *pn,
                 JSBool *answer)
{
    JSBool ok;
    JSParseNode *pn2;

    ok = JS_TRUE;
    if (!pn || *answer)
        return ok;

    switch (pn->pn_arity) {
      case PN_FUNC:
        /*
         * A named function is presumed useful: we can't yet know that it is
         * not called.  The side effects are the creation of a scope object
         * to parent this function object, and the binding of the function's
         * name in that scope object.  See comments at case JSOP_NAMEDFUNOBJ:
         * in jsinterp.c.
         */
        if (pn->pn_fun->atom)
            *answer = JS_TRUE;
        break;

      case PN_LIST:
        if (pn->pn_type == TOK_NEW || pn->pn_type == TOK_LP) {
            /*
             * All invocation operations (construct, call) are presumed to be
             * useful, because they may have side effects even if their main
             * effect (their return value) is discarded.
             */
            *answer = JS_TRUE;
        } else {
            for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next)
                ok &= CheckSideEffects(cx, tc, pn2, answer);
        }
        break;

      case PN_TERNARY:
        ok = CheckSideEffects(cx, tc, pn->pn_kid1, answer) &&
             CheckSideEffects(cx, tc, pn->pn_kid2, answer) &&
             CheckSideEffects(cx, tc, pn->pn_kid3, answer);
        break;

      case PN_BINARY:
        if (pn->pn_type == TOK_ASSIGN) {
            /*
             * Assignment is presumed to be useful, even if the next operation
             * is another assignment overwriting this one's ostensible effect,
             * because the left operand may be a property with a setter that
             * has side effects.
             */
            *answer = JS_TRUE;
        } else {
            if (pn->pn_type == TOK_LB) {
                pn2 = pn->pn_left;
                if (pn2->pn_type == TOK_NAME && !LookupArgOrVar(cx, tc, pn2))
                    return JS_FALSE;
                if (pn2->pn_op != JSOP_ARGUMENTS) {
                    /*
                     * Any indexed property reference could call a getter with
                     * side effects, except for arguments[i] where arguments is
                     * unambiguous.
                     */
                    *answer = JS_TRUE;
                }
            }
            ok = CheckSideEffects(cx, tc, pn->pn_left, answer) &&
                 CheckSideEffects(cx, tc, pn->pn_right, answer);
        }
        break;

      case PN_UNARY:
        if (pn->pn_type == TOK_INC || pn->pn_type == TOK_DEC ||
            pn->pn_type == TOK_DELETE ||
            pn->pn_type == TOK_THROW ||
            pn->pn_type == TOK_DEFSHARP) {
            /* All these operations have effects that we must commit. */
            *answer = JS_TRUE;
        } else {
            ok = CheckSideEffects(cx, tc, pn->pn_kid, answer);
        }
        break;

      case PN_NAME:
        if (pn->pn_type == TOK_NAME) {
            if (!LookupArgOrVar(cx, tc, pn))
                return JS_FALSE;
            if (pn->pn_slot < 0 && pn->pn_op != JSOP_ARGUMENTS) {
                /*
                 * Not an argument or local variable use, so this expression
                 * could invoke a getter that has side effects.
                 */
                *answer = JS_TRUE;
            }
        }
        pn2 = pn->pn_expr;
        if (pn->pn_type == TOK_DOT && pn2->pn_type == TOK_NAME) {
            if (!LookupArgOrVar(cx, tc, pn2))
                return JS_FALSE;
            if (!(pn2->pn_op == JSOP_ARGUMENTS &&
                  pn->pn_atom == cx->runtime->atomState.lengthAtom)) {
                /*
                 * Any dotted property reference could call a getter, except
                 * for arguments.length where arguments is unambiguous.
                 */
                *answer = JS_TRUE;
            }
        }
        ok = CheckSideEffects(cx, tc, pn2, answer);
        break;

      case PN_NULLARY:
        if (pn->pn_type == TOK_DEBUGGER)
            *answer = JS_TRUE;
        break;
    }
    return ok;
}

static JSBool
EmitPropOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSParseNode *pn2;
    JSAtomListElement *ale;

    pn2 = pn->pn_expr;
    if (op == JSOP_GETPROP &&
        pn->pn_type == TOK_DOT &&
        pn2->pn_type == TOK_NAME) {
        /* Try to optimize arguments.length into JSOP_ARGCNT. */
        if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
            return JS_FALSE;
        if (pn2->pn_op == JSOP_ARGUMENTS &&
            pn->pn_atom == cx->runtime->atomState.lengthAtom) {
            return js_Emit1(cx, cg, JSOP_ARGCNT) >= 0;
        }
    }
    if (!js_EmitTree(cx, cg, pn2))
        return JS_FALSE;
    if (js_NewSrcNote2(cx, cg, SRC_PCBASE, CG_OFFSET(cg) - pn2->pn_offset) < 0)
        return JS_FALSE;
    if (!pn->pn_atom) {
        JS_ASSERT(op == JSOP_IMPORTALL);
        if (js_Emit1(cx, cg, op) < 0)
            return JS_FALSE;
    } else {
        ale = js_IndexAtom(cx, pn->pn_atom, &cg->atomList);
        if (!ale)
            return JS_FALSE;
        EMIT_ATOM_INDEX_OP(op, ALE_INDEX(ale));
    }
    return JS_TRUE;
}

static JSBool
EmitElemOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSParseNode *left, *right;
    jsint slot;

    left = pn->pn_left;
    right = pn->pn_right;
    if (op == JSOP_GETELEM &&
        left->pn_type == TOK_NAME &&
        right->pn_type == TOK_NUMBER) {
        /* Try to optimize arguments[0] into JSOP_ARGSUB. */
        if (!LookupArgOrVar(cx, &cg->treeContext, left))
            return JS_FALSE;
        if (left->pn_op == JSOP_ARGUMENTS &&
            JSDOUBLE_IS_INT(right->pn_dval, slot) &&
            slot >= 0) {
            EMIT_ATOM_INDEX_OP(JSOP_ARGSUB, (jsatomid)slot);
            return JS_TRUE;
        }
    }
    if (!js_EmitTree(cx, cg, left) || !js_EmitTree(cx, cg, right))
        return JS_FALSE;
    if (js_NewSrcNote2(cx, cg, SRC_PCBASE, CG_OFFSET(cg) - left->pn_offset) < 0)
        return JS_FALSE;
    return js_Emit1(cx, cg, op) >= 0;
}

static JSBool
EmitNumberOp(JSContext *cx, jsdouble dval, JSCodeGenerator *cg)
{
    jsint ival;
    jsatomid atomIndex;
    JSAtom *atom;
    JSAtomListElement *ale;

    if (JSDOUBLE_IS_INT(dval, ival) && INT_FITS_IN_JSVAL(ival)) {
        if (ival == 0)
            return js_Emit1(cx, cg, JSOP_ZERO) >= 0;
        if (ival == 1)
            return js_Emit1(cx, cg, JSOP_ONE) >= 0;
        if ((jsuint)ival < (jsuint)ATOM_INDEX_LIMIT) {
            atomIndex = (jsatomid)ival;
            EMIT_ATOM_INDEX_OP(JSOP_UINT16, atomIndex);
            return JS_TRUE;
        }
        atom = js_AtomizeInt(cx, ival, 0);
    } else {
        atom = js_AtomizeDouble(cx, dval, 0);
    }
    if (!atom)
        return JS_FALSE;
    ale = js_IndexAtom(cx, atom, &cg->atomList);
    if (!ale)
        return JS_FALSE;
    EMIT_ATOM_INDEX_OP(JSOP_NUMBER, ALE_INDEX(ale));
    return JS_TRUE;
}

JSBool
js_EmitFunctionBody(JSContext *cx, JSCodeGenerator *cg, JSParseNode *body,
                    JSFunction *fun)
{
    JSStackFrame *fp, frame;
    JSObject *funobj;
    JSBool ok;

    if (!js_AllocTryNotes(cx, cg))
        return JS_FALSE;

    fp = cx->fp;
    funobj = fun->object;
    if (!fp || fp->fun != fun || fp->varobj != funobj ||
        fp->scopeChain != funobj) {
        memset(&frame, 0, sizeof frame);
        frame.fun = fun;
        frame.varobj = frame.scopeChain = funobj;
        frame.down = fp;
        cx->fp = &frame;
    }
    ok = js_EmitTree(cx, cg, body);
    cx->fp = fp;
    if (!ok)
        return JS_FALSE;

    fun->script = js_NewScriptFromCG(cx, cg, fun);
    if (!fun->script)
        return JS_FALSE;
    if (cg->treeContext.flags & TCF_FUN_HEAVYWEIGHT)
        fun->flags |= JSFUN_HEAVYWEIGHT;
    return JS_TRUE;
}

/* A macro for inlining at the top of js_EmitTree (whence it came). */
#define UPDATE_LINENO_NOTES(cx, cg, pn)                                       \
    JS_BEGIN_MACRO                                                            \
        uintN line_ = (pn)->pn_pos.begin.lineno;                              \
        uintN delta_ = line_ - (cg)->currentLine;                             \
        if (delta_ != 0) {                                                    \
            /*                                                                \
             * Encode any change in the current source line number by using   \
             * either several SRC_NEWLINE notes or just one SRC_SETLINE note, \
             * whichever consumes less space.                                 \
             *                                                                \
             * NB: We handle backward line number deltas (possible with for   \
             * loops where the update part is emitted after the body, but its \
             * line number is <= any line number in the body) here by letting \
             * unsigned delta_ wrap to a very large number, which triggers a  \
             * SRC_SETLINE.                                                   \
             */                                                               \
            (cg)->currentLine = line_;                                        \
            if (delta_ >= (uintN)(2 + ((line_ > SN_3BYTE_OFFSET_MASK)<<1))) { \
                if (js_NewSrcNote2(cx, cg, SRC_SETLINE, (ptrdiff_t)line_) < 0)\
                    return JS_FALSE;                                          \
            } else {                                                          \
                do {                                                          \
                    if (js_NewSrcNote(cx, cg, SRC_NEWLINE) < 0)               \
                        return JS_FALSE;                                      \
                } while (--delta_ != 0);                                      \
            }                                                                 \
        }                                                                     \
    JS_END_MACRO

/* A function, so that we avoid macro-bloating all the other callsites. */
static JSBool
UpdateLinenoNotes(JSContext *cx, JSCodeGenerator *cg, JSParseNode *pn)
{
    UPDATE_LINENO_NOTES(cx, cg, pn);
    return JS_TRUE;
}

JSBool
js_EmitTree(JSContext *cx, JSCodeGenerator *cg, JSParseNode *pn)
{
    JSBool ok, useful;
    JSCodeGenerator cg2;
    JSStmtInfo *stmt, stmtInfo;
    ptrdiff_t top, off, tmp, beq, jmp;
    JSParseNode *pn2, *pn3, *pn4;
    JSAtom *atom;
    JSAtomListElement *ale;
    jsatomid atomIndex;
    intN noteIndex;
    JSSrcNoteType noteType;
    JSOp op;
    uint32 argc;

    ok = JS_TRUE;
    cg->emitLevel++;
    pn->pn_offset = top = CG_OFFSET(cg);

    /* Emit notes to tell the current bytecode's source line number. */
    UPDATE_LINENO_NOTES(cx, cg, pn);

    switch (pn->pn_type) {
      case TOK_FUNCTION:
      {
        JSFunction *fun;

        /* Generate code for the function's body. */
        pn2 = pn->pn_body;
        if (!js_InitCodeGenerator(cx, &cg2, cg->filename,
                                  pn->pn_pos.begin.lineno,
                                  cg->principals)) {
            return JS_FALSE;
        }
        cg2.treeContext.flags = pn->pn_flags | TCF_IN_FUNCTION;
        cg2.treeContext.tryCount = pn->pn_tryCount;
        fun = pn->pn_fun;
        if (!js_EmitFunctionBody(cx, &cg2, pn2, fun))
            return JS_FALSE;

        /*
         * We need an activation object if an inner peeks out, or if such
         * inner-peeking caused one of our inners to become heavyweight.
         */
        if (cg2.treeContext.flags &
            (TCF_FUN_USES_NONLOCALS | TCF_FUN_HEAVYWEIGHT)) {
            cg->treeContext.flags |= TCF_FUN_HEAVYWEIGHT;
        }
        js_FinishCodeGenerator(cx, &cg2);

        /* Make the function object a literal in the outer script's pool. */
        atom = js_AtomizeObject(cx, fun->object, 0);
        if (!atom)
            return JS_FALSE;
        ale = js_IndexAtom(cx, atom, &cg->atomList);
        if (!ale)
            return JS_FALSE;
        atomIndex = ALE_INDEX(ale);

#if JS_HAS_LEXICAL_CLOSURE
        /* Emit a bytecode pointing to the closure object in its immediate. */
        if (pn->pn_op != JSOP_NOP) {
            EMIT_ATOM_INDEX_OP(pn->pn_op, atomIndex);
            break;
        }
#endif

        /* Top-level named functions need a nop for decompilation. */
        noteIndex = js_NewSrcNote2(cx, cg, SRC_FUNCDEF, (ptrdiff_t)atomIndex);
        if (noteIndex < 0 ||
            js_Emit1(cx, cg, JSOP_NOP) < 0) {
            return JS_FALSE;
        }

        /*
         * Top-levels also need a prolog op to predefine their names in the
         * variable object, or if local, to fill their stack slots.
         */
        CG_SWITCH_TO_PROLOG(cg);
#if JS_HAS_LEXICAL_CLOSURE
        if (cg->treeContext.flags & TCF_IN_FUNCTION) {
            JSObject *obj, *pobj;
            JSScopeProperty *sprop;
            uintN slot;
            jsbytecode *pc;

            obj = OBJ_GET_PARENT(cx, pn->pn_fun->object);
            if (!js_LookupProperty(cx, obj, (jsid)fun->atom, &pobj,
                                   (JSProperty **)&sprop)) {
                return JS_FALSE;
            }
            JS_ASSERT(sprop && pobj == obj);
            slot = sprop->shortid;
            OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);

            /* Emit [JSOP_DEFLOCALFUN, local variable slot, atomIndex]. */
            off = js_EmitN(cx, cg, JSOP_DEFLOCALFUN, VARNO_LEN+ATOM_INDEX_LEN);
            if (off < 0)
                return JS_FALSE;
            pc = CG_CODE(cg, off);
            SET_VARNO(pc, slot);
            pc += VARNO_LEN;
            SET_ATOM_INDEX(pc, atomIndex);
        } else
#endif
            EMIT_ATOM_INDEX_OP(JSOP_DEFFUN, atomIndex);
        CG_SWITCH_TO_MAIN(cg);

        break;
      }

#if JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
        pn2 = pn->pn_head;
        if (pn2->pn_type == TOK_STAR) {
            /*
             * 'export *' must have no other elements in the list (what would
             * be the point?).
             */
            if (js_Emit1(cx, cg, JSOP_EXPORTALL) < 0)
                return JS_FALSE;
        } else {
            /*
             * If not 'export *', the list consists of NAME nodes identifying
             * properties of the variables object to flag as exported.
             */
            do {
                ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                EMIT_ATOM_INDEX_OP(JSOP_EXPORTNAME, ALE_INDEX(ale));
            } while ((pn2 = pn2->pn_next) != NULL);
        }
        break;

      case TOK_IMPORT:
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            /*
             * Each subtree on an import list is rooted by a DOT or LB node.
             * A DOT may have a null pn_atom member, in which case pn_op must
             * be JSOP_IMPORTALL -- see EmitPropOp above.
             */
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
        }
        break;
#endif /* JS_HAS_EXPORT_IMPORT */

      case TOK_IF:
        /* Emit code for the condition before pushing stmtInfo. */
        if (!js_EmitTree(cx, cg, pn->pn_kid1))
            return JS_FALSE;
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_IF, CG_OFFSET(cg));

        /* Emit an annotated branch-if-false around the then part. */
        pn3 = pn->pn_kid3;
        noteIndex = js_NewSrcNote(cx, cg, pn3 ? SRC_IF_ELSE : SRC_IF);
        if (noteIndex < 0)
            return JS_FALSE;
        beq = EmitJump(cx, cg, JSOP_IFEQ, 0);
        if (beq < 0)
            return JS_FALSE;

        /* Emit code for the then and optional else parts. */
        if (!js_EmitTree(cx, cg, pn->pn_kid2))
            return JS_FALSE;
        if (pn3) {
            /* Modify stmtInfo so we know we're in the else part. */
            stmtInfo.type = STMT_ELSE;

            /* Jump at end of then part around the else part. */
            jmp = EmitJump(cx, cg, JSOP_GOTO, 0);
            if (jmp < 0)
                return JS_FALSE;

            /* Ensure the branch-if-false comes here, then emit the else. */
            CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
            if (!js_EmitTree(cx, cg, pn3))
                return JS_FALSE;

            /* Fixup the jump around the else part. */
            CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);

            /*
             * Annotate SRC_IF_ELSE with the offset from branch to jump, for
             * the decompiler's benefit.  We can't just "back up" from the pc
             * of the else clause, because we don't know whether an extended
             * jump was required to leap from the end of the then clause over
             * the else clause.
             */
            if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 0, jmp - beq))
                return JS_FALSE;
        } else {
            /* No else part, fixup the branch-if-false to come here. */
            CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
        }
        ok = js_PopStatementCG(cx, cg);
        break;

#if JS_HAS_SWITCH_STATEMENT
      case TOK_SWITCH:
      {
        JSOp switchop;
        uint32 ncases, tablen = 0;
        JSScript *script;
        jsint i, low, high;
        jsdouble d;
        size_t switchsize, tablesize;
        JSParseNode **table;
        jsbytecode *pc;
        JSBool hasDefault = JS_FALSE;
        JSBool isEcmaSwitch = cx->version == JSVERSION_DEFAULT ||
                              cx->version >= JSVERSION_1_4;
        ptrdiff_t defaultOffset = -1;

        /* Try for most optimal, fall back if not dense ints, and per ECMAv2. */
        switchop = JSOP_TABLESWITCH;

        /* Emit code for the discriminant first. */
        if (!js_EmitTree(cx, cg, pn->pn_kid1))
            return JS_FALSE;

        /* Switch bytecodes run from here till end of final case. */
        top = CG_OFFSET(cg);
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_SWITCH, top);

        pn2 = pn->pn_kid2;
        ncases = pn2->pn_count;

        if (ncases == 0 ||
            (ncases == 1 &&
             (hasDefault = (pn2->pn_head->pn_type == TOK_DEFAULT)))) {
            ncases = 0;
            low = 0;
            high = -1;
        } else {
#define INTMAP_LENGTH   256
            jsbitmap intmap_space[INTMAP_LENGTH];
            jsbitmap *intmap = NULL;
            int32 intmap_bitlen = 0;

            low  = JSVAL_INT_MAX;
            high = JSVAL_INT_MIN;
            cg2.current = NULL;

            for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                if (pn3->pn_type == TOK_DEFAULT) {
                    hasDefault = JS_TRUE;
                    ncases--;   /* one of the "cases" was the default */
                    continue;
                }
                JS_ASSERT(pn3->pn_type == TOK_CASE);
                pn4 = pn3->pn_left;
                if (isEcmaSwitch) {
                    if (switchop == JSOP_CONDSWITCH)
                        continue;
                    switch (pn4->pn_type) {
                      case TOK_NUMBER:
                        d = pn4->pn_dval;
                        if (JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i)) {
                            pn3->pn_val = INT_TO_JSVAL(i);
                        } else {
                            atom = js_AtomizeDouble(cx, d, 0);
                            if (!atom) {
                                ok = JS_FALSE;
                                goto release;
                            }
                            pn3->pn_val = ATOM_KEY(atom);
                        }
                        break;
                      case TOK_STRING:
                        pn3->pn_val = ATOM_KEY(pn4->pn_atom);
                        break;
                      case TOK_PRIMARY:
                        if (pn4->pn_op == JSOP_TRUE) {
                            pn3->pn_val = JSVAL_TRUE;
                            break;
                        }
                        if (pn4->pn_op == JSOP_FALSE) {
                            pn3->pn_val = JSVAL_FALSE;
                            break;
                        }
                        /* FALL THROUGH */
                      default:
                        switchop = JSOP_CONDSWITCH;
                        continue;
                    }
                } else {
                    /* Pre-ECMAv2 switch evals case exprs at compile time. */
                    ok = js_InitCodeGenerator(cx, &cg2, cg->filename,
                                              pn3->pn_pos.begin.lineno,
                                              cg->principals);
                    if (!ok)
                        goto release;
                    cg2.currentLine = pn4->pn_pos.begin.lineno;
                    ok = js_EmitTree(cx, &cg2, pn4);
                    if (!ok)
                        goto release;
                    if (js_Emit1(cx, &cg2, JSOP_POPV) < 0) {
                        ok = JS_FALSE;
                        goto release;
                    }
                    script = js_NewScriptFromCG(cx, &cg2, NULL);
                    if (!script) {
                        ok = JS_FALSE;
                        goto release;
                    }
                    ok = js_Execute(cx, cx->fp->scopeChain, script, cx->fp, 0,
                                    &pn3->pn_val);
                    js_DestroyScript(cx, script);
                    if (!ok)
                        goto release;
                }

                if (!JSVAL_IS_NUMBER(pn3->pn_val) &&
                    !JSVAL_IS_STRING(pn3->pn_val) &&
                    !JSVAL_IS_BOOLEAN(pn3->pn_val)) {
                    cg->currentLine = pn3->pn_pos.begin.lineno;
                    js_ReportCompileErrorNumber(cx, NULL, cg, JSREPORT_ERROR,
                                                JSMSG_BAD_CASE);
                    ok = JS_FALSE;
                    goto release;
                }

                if (switchop != JSOP_TABLESWITCH)
                    continue;
                if (!JSVAL_IS_INT(pn3->pn_val)) {
                    switchop = JSOP_LOOKUPSWITCH;
                    continue;
                }
                i = JSVAL_TO_INT(pn3->pn_val);
                if ((jsuint)(i + (jsint)JS_BIT(15)) >= (jsuint)JS_BIT(16)) {
                    switchop = JSOP_LOOKUPSWITCH;
                    continue;
                }
                if (i < low)
                    low = i;
                if (high < i)
                    high = i;

                /*
                 * Check for duplicates, which require a JSOP_LOOKUPSWITCH.
                 * We bias i by 65536 if it's negative, and hope that's a rare
                 * case (because it requires a malloc'd bitmap).
                 */
                if (i < 0)
                    i += JS_BIT(16);
                if (i >= intmap_bitlen) {
                    if (!intmap &&
                        i < (INTMAP_LENGTH << JS_BITS_PER_WORD_LOG2)) {
                        intmap = intmap_space;
                        intmap_bitlen = INTMAP_LENGTH << JS_BITS_PER_WORD_LOG2;
                    } else {
                        /* Just grab 8K for the worst-case bitmap. */
                        intmap_bitlen = JS_BIT(16);
                        intmap = (jsbitmap *)
                            JS_malloc(cx,
                                      (JS_BIT(16) >> JS_BITS_PER_WORD_LOG2)
                                      * sizeof(jsbitmap));
                        if (!intmap) {
                            JS_ReportOutOfMemory(cx);
                            ok = JS_FALSE;
                            goto release;
                        }
                    }
                    memset(intmap, 0, intmap_bitlen >> JS_BITS_PER_BYTE_LOG2);
                }
                if (JS_TEST_BIT(intmap, i)) {
                    switchop = JSOP_LOOKUPSWITCH;
                    continue;
                }
                JS_SET_BIT(intmap, i);
            }

          release:
            if (intmap && intmap != intmap_space)
                JS_free(cx, intmap);
            if (!ok)
                return JS_FALSE;

            if (switchop == JSOP_CONDSWITCH) {
                JS_ASSERT(!cg2.current);
            } else {
                if (cg2.current)
                    js_FinishCodeGenerator(cx, &cg2);
                if (switchop == JSOP_TABLESWITCH) {
                    tablen = (uint32)(high - low + 1);
                    if (tablen >= JS_BIT(16) || tablen > 2 * ncases)
                        switchop = JSOP_LOOKUPSWITCH;
                }
            }
        }

        /*
         * Emit a note with two offsets: first tells total switch code length,
         * second tells offset to first JSOP_CASE if condswitch.
         */
        noteIndex = js_NewSrcNote3(cx, cg, SRC_SWITCH, 0, 0);
        if (noteIndex < 0)
            return JS_FALSE;

        if (switchop == JSOP_CONDSWITCH) {
            /*
             * 0 bytes of immediate for unoptimized ECMAv2 switch.
             */
            switchsize = 0;
        } else if (switchop == JSOP_TABLESWITCH) {
            /*
             * 3 offsets (len, low, high) before the table, 1 per entry.
             */
            switchsize = (size_t)(JUMP_OFFSET_LEN * (3 + tablen));
        } else {
            /*
             * JSOP_LOOKUPSWITCH:
             * 1 offset (len) and 1 atom index (npairs) before the table,
             * 1 atom index and 1 jump offset per entry.
             */
            switchsize = (size_t)(JUMP_OFFSET_LEN + ATOM_INDEX_LEN +
                                  (ATOM_INDEX_LEN + JUMP_OFFSET_LEN) * ncases);
        }

        /*
         * Emit switchop followed by switchsize bytes of jump or lookup table.
         *
         * If switchop is JSOP_LOOKUPSWITCH or JSOP_TABLESWITCH, it is crucial
         * to emit the immediate operand(s) by which bytecode readers such as
         * BuildSpanDepTable discover the length of the switch opcode *before*
         * calling js_SetJumpOffset (which may call BuildSpanDepTable).  It's
         * also important to zero all unknown jump offset immediate operands,
         * so they can be converted to span dependencies with null targets to
         * be computed later (js_EmitN zeros switchsize bytes after switchop).
         */
        if (js_EmitN(cx, cg, switchop, switchsize) < 0)
            return JS_FALSE;

        off = -1;
        if (switchop == JSOP_CONDSWITCH) {
            intN caseNoteIndex = -1;

            /* Emit code for evaluating cases and jumping to case statements. */
            for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                pn4 = pn3->pn_left;
                if (pn4 && !js_EmitTree(cx, cg, pn4))
                    return JS_FALSE;
                if (caseNoteIndex >= 0) {
                    /* off is the previous JSOP_CASE's bytecode offset. */
                    if (!js_SetSrcNoteOffset(cx, cg, (uintN)caseNoteIndex, 0,
                                             CG_OFFSET(cg) - off)) {
                        return JS_FALSE;
                    }
                }
                if (pn3->pn_type == TOK_DEFAULT)
                    continue;
                caseNoteIndex = js_NewSrcNote2(cx, cg, SRC_PCDELTA, 0);
                if (caseNoteIndex < 0)
                    return JS_FALSE;
                off = EmitJump(cx, cg, JSOP_CASE, 0);
                if (off < 0)
                    return JS_FALSE;
                pn3->pn_offset = off;
                if (pn3 == pn2->pn_head) {
                    /* Switch note's second offset is to first JSOP_CASE. */
                    if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 1,
                                             off - top)) {
                        return JS_FALSE;
                    }
                }
            }

            /* Emit default even if no explicit default statement. */
            defaultOffset = EmitJump(cx, cg, JSOP_DEFAULT, 0);
            if (defaultOffset < 0)
                return JS_FALSE;
        } else if (switchop == JSOP_TABLESWITCH) {
            /* Fill in switch bounds, which we know fit in 16-bit offsets. */
            pc = CG_CODE(cg, top + JUMP_OFFSET_LEN);
            SET_JUMP_OFFSET(pc, low);
            pc += JUMP_OFFSET_LEN;
            SET_JUMP_OFFSET(pc, high);
            pc += JUMP_OFFSET_LEN;
        } else {
            JS_ASSERT(switchop == JSOP_LOOKUPSWITCH);

            /* Fill in the number of cases. */
            pc = CG_CODE(cg, top + JUMP_OFFSET_LEN);
            SET_ATOM_INDEX(pc, ncases);
        }

        /* Emit code for each case's statements, copying pn_offset up to pn3. */
        for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            if (switchop == JSOP_CONDSWITCH && pn3->pn_type != TOK_DEFAULT) {
                pn3->pn_val = INT_TO_JSVAL(pn3->pn_offset - top);
                CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, pn3->pn_offset);
            }
            pn4 = pn3->pn_right;
            if (!js_EmitTree(cx, cg, pn4))
                return JS_FALSE;
            pn3->pn_offset = pn4->pn_offset;
            if (pn3->pn_type == TOK_DEFAULT)
                off = pn3->pn_offset - top;
        }

        if (!hasDefault) {
            /* If no default case, offset for default is to end of switch. */
            off = CG_OFFSET(cg) - top;
        }

        /* We better have set "off" by now. */
        JS_ASSERT(off != -1);

        /* Set the default offset (to end of switch if no default). */
        pc = NULL;
        if (switchop == JSOP_CONDSWITCH) {
            JS_ASSERT(defaultOffset != -1);
            if (!js_SetJumpOffset(cx, cg, CG_CODE(cg, defaultOffset),
                                  off - (defaultOffset - top))) {
                return JS_FALSE;
            }
        } else {
            pc = CG_CODE(cg, top);
            if (!js_SetJumpOffset(cx, cg, pc, off))
                return JS_FALSE;
            pc += JUMP_OFFSET_LEN;
        }

        /* Set the SRC_SWITCH note's offset operand to tell end of switch. */
        off = CG_OFFSET(cg) - top;
        if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, off))
            return JS_FALSE;

        if (switchop == JSOP_TABLESWITCH) {
            /* Skip over the already-initialized switch bounds. */
            pc += 2 * JUMP_OFFSET_LEN;

            /* Fill in the jump table, if there is one. */
            if (tablen) {
                /* Avoid bloat for a compilation unit with many switches. */
                tablesize = (size_t)tablen * sizeof *table;
                table = (JSParseNode **) JS_malloc(cx, tablesize);
                if (!table) {
                    JS_ReportOutOfMemory(cx);
                    return JS_FALSE;
                }
                memset(table, 0, tablesize);
                for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                    if (pn3->pn_type == TOK_DEFAULT)
                        continue;
                    i = JSVAL_TO_INT(pn3->pn_val);
                    i -= low;
                    JS_ASSERT((uint32)i < tablen);
                    table[i] = pn3;
                }
                for (i = 0; i < (jsint)tablen; i++) {
                    pn3 = table[i];
                    off = pn3 ? pn3->pn_offset - top : 0;
                    ok = js_SetJumpOffset(cx, cg, pc, off);
                    if (!ok)
                        break;
                    pc += JUMP_OFFSET_LEN;
                }
                JS_free(cx, table);
                if (!ok)
                    return JS_FALSE;
            }
        } else if (switchop == JSOP_LOOKUPSWITCH) {
            /* Skip over the already-initialized number of cases. */
            pc += ATOM_INDEX_LEN;

            for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                if (pn3->pn_type == TOK_DEFAULT)
                    continue;
                atom = js_AtomizeValue(cx, pn3->pn_val, 0);
                if (!atom)
                    return JS_FALSE;
                ale = js_IndexAtom(cx, atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                SET_ATOM_INDEX(pc, ALE_INDEX(ale));
                pc += ATOM_INDEX_LEN;

                off = pn3->pn_offset - top;
                if (!js_SetJumpOffset(cx, cg, pc, off))
                    return JS_FALSE;
                pc += JUMP_OFFSET_LEN;
            }
        }

        ok = js_PopStatementCG(cx, cg);
        break;
      }
#endif /* JS_HAS_SWITCH_STATEMENT */

      case TOK_WHILE:
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_WHILE_LOOP, top);
        if (!js_EmitTree(cx, cg, pn->pn_left))
            return JS_FALSE;
        noteIndex = js_NewSrcNote(cx, cg, SRC_WHILE);
        if (noteIndex < 0)
            return JS_FALSE;
        beq = EmitJump(cx, cg, JSOP_IFEQ, 0);
        if (beq < 0)
            return JS_FALSE;
        if (!js_EmitTree(cx, cg, pn->pn_right))
            return JS_FALSE;
        jmp = EmitJump(cx, cg, JSOP_GOTO, top - CG_OFFSET(cg));
        if (jmp < 0)
            return JS_FALSE;
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
        if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 0, jmp - beq))
            return JS_FALSE;
        ok = js_PopStatementCG(cx, cg);
        break;

#if JS_HAS_DO_WHILE_LOOP
      case TOK_DO:
        /* Emit an annotated nop so we know to decompile a 'do' keyword. */
        if (js_NewSrcNote(cx, cg, SRC_WHILE) < 0 ||
            js_Emit1(cx, cg, JSOP_NOP) < 0) {
            return JS_FALSE;
        }

        /* Compile the loop body. */
        top = CG_OFFSET(cg);
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_DO_LOOP, top);
        if (!js_EmitTree(cx, cg, pn->pn_left))
            return JS_FALSE;

        /* Set loop and enclosing label update offsets, for continue. */
        stmt = &stmtInfo;
        do {
            stmt->update = CG_OFFSET(cg);
        } while ((stmt = stmt->down) != NULL && stmt->type == STMT_LABEL);

        /* Compile the loop condition, now that continues know where to go. */
        if (!js_EmitTree(cx, cg, pn->pn_right))
            return JS_FALSE;

        /*
         * No source note needed, because JSOP_IFNE is used only for do-while.
         * If we ever use JSOP_IFNE for other purposes, we can still avoid yet
         * another note here, by storing (jmp - top) in the SRC_WHILE note's
         * offset, and fetching that delta in order to decompile recursively.
         */
        if (EmitJump(cx, cg, JSOP_IFNE, top - CG_OFFSET(cg)) < 0)
            return JS_FALSE;
        ok = js_PopStatementCG(cx, cg);
        break;
#endif /* JS_HAS_DO_WHILE_LOOP */

      case TOK_FOR:
        beq = 0;                /* suppress gcc warnings */
        pn2 = pn->pn_left;
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_FOR_LOOP, top);

        if (pn2->pn_type == TOK_IN) {
            /* If the left part is var x = i, bind x, evaluate i, and pop. */
            pn3 = pn2->pn_left;
            if (pn3->pn_type == TOK_VAR && pn3->pn_head->pn_expr) {
                if (!js_EmitTree(cx, cg, pn3))
                    return JS_FALSE;
                /* Set pn3 to the variable name, to avoid another var note. */
                pn3 = pn3->pn_head;
                JS_ASSERT(pn3->pn_type == TOK_NAME);
            }

            /* Fix stmtInfo and emit a push to allocate the iterator. */
            stmtInfo.type = STMT_FOR_IN_LOOP;
            noteIndex = -1;
            if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
                return JS_FALSE;

            /* Compile the object expression to the right of 'in'. */
            if (!js_EmitTree(cx, cg, pn2->pn_right))
                return JS_FALSE;
            if (js_Emit1(cx, cg, JSOP_TOOBJECT) < 0)
                return JS_FALSE;

            top = CG_OFFSET(cg);
            SET_STATEMENT_TOP(&stmtInfo, top);

            /* Compile a JSOP_FOR* bytecode based on the left hand side. */
            switch (pn3->pn_type) {
              case TOK_VAR:
                pn3 = pn3->pn_head;
                if (js_NewSrcNote(cx, cg, SRC_VAR) < 0)
                    return JS_FALSE;
                /* FALL THROUGH */
              case TOK_NAME:
                pn3->pn_op = JSOP_FORNAME;
                if (!LookupArgOrVar(cx, &cg->treeContext, pn3))
                    return JS_FALSE;
                op = pn3->pn_op;
                if (pn3->pn_slot >= 0) {
                    if (pn3->pn_attrs & JSPROP_READONLY)
                        op = JSOP_GETVAR;
                    atomIndex = (jsatomid) pn3->pn_slot;
                    EMIT_ATOM_INDEX_OP(op, atomIndex);
                } else {
                    if (!EmitAtomOp(cx, pn3, op, cg))
                        return JS_FALSE;
                }
                break;
              case TOK_DOT:
                if (!EmitPropOp(cx, pn3, JSOP_FORPROP, cg))
                    return JS_FALSE;
                break;
              case TOK_LB:
                /*
                 * We separate the first/next bytecode from the enumerator
                 * variable binding to avoid any side-effects in the index
                 * expression (e.g., for (x[i++] in {}) should not bind x[i]
                 * or increment i at all).
                 */
                if (!js_Emit1(cx, cg, JSOP_FORELEM))
                    return JS_FALSE;
                /*
                 * Emit a SRC_WHILE note with offset telling the distance to
                 * the loop-closing jump (we can't reckon from the branch at
                 * the top of the loop, because the loop-closing jump might be
                 * need to be an extended jump, independent of whether the
                 * branch is short or long).
                 */
                noteIndex = js_NewSrcNote(cx, cg, SRC_WHILE);
                if (noteIndex < 0)
                    return JS_FALSE;
                beq = EmitJump(cx, cg, JSOP_IFEQ, 0);
                if (beq < 0)
                    return JS_FALSE;

                /* Now that we're safely past the IFEQ, commit side effects. */
                if (!EmitElemOp(cx, pn3, JSOP_ENUMELEM, cg))
                    return JS_FALSE;
                break;
              default:
                JS_ASSERT(0);
            }
            if (pn3->pn_type != TOK_LB) {
                /* Annotate so the decompiler can find the loop-closing jump. */
                noteIndex = js_NewSrcNote(cx, cg, SRC_WHILE);
                if (noteIndex < 0)
                    return JS_FALSE;

                /* Pop and test the loop condition generated by JSOP_FOR*. */
                beq = EmitJump(cx, cg, JSOP_IFEQ, 0);
                if (beq < 0)
                    return JS_FALSE;
            }
        } else {
            if (!pn2->pn_kid1) {
                /* No initializer: emit an annotated nop for the decompiler. */
                op = JSOP_NOP;
            } else {
                if (!js_EmitTree(cx, cg, pn2->pn_kid1))
                    return JS_FALSE;
                op = JSOP_POP;
            }
            noteIndex = js_NewSrcNote(cx, cg, SRC_FOR);
            if (noteIndex < 0 ||
                js_Emit1(cx, cg, op) < 0) {
                return JS_FALSE;
            }

            top = CG_OFFSET(cg);
            SET_STATEMENT_TOP(&stmtInfo, top);
            if (!pn2->pn_kid2) {
                /* No loop condition: flag this fact in the source notes. */
                if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, 0))
                    return JS_FALSE;
            } else {
                if (!js_EmitTree(cx, cg, pn2->pn_kid2))
                    return JS_FALSE;
                if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0,
                                         CG_OFFSET(cg) - top)) {
                    return JS_FALSE;
                }
                beq = EmitJump(cx, cg, JSOP_IFEQ, 0);
                if (beq < 0)
                    return JS_FALSE;
            }

            /* Set pn3 (used below) here to avoid spurious gcc warnings. */
            pn3 = pn2->pn_kid3;
        }

        /* Emit code for the loop body. */
        if (!js_EmitTree(cx, cg, pn->pn_right))
            return JS_FALSE;

        if (pn2->pn_type != TOK_IN) {
            /* Set the second note offset so we can find the update part. */
            JS_ASSERT(noteIndex != -1);
            if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 1,
                                     CG_OFFSET(cg) - top)) {
                return JS_FALSE;
            }

            if (pn3) {
                /* Set loop and enclosing "update" offsets, for continue. */
                stmt = &stmtInfo;
                do {
                    stmt->update = CG_OFFSET(cg);
                } while ((stmt = stmt->down) != NULL &&
                         stmt->type == STMT_LABEL);

                if (!js_EmitTree(cx, cg, pn3))
                    return JS_FALSE;
                if (js_Emit1(cx, cg, JSOP_POP) < 0)
                    return JS_FALSE;

                /* Restore the absolute line number for source note readers. */
                off = (ptrdiff_t) pn->pn_pos.end.lineno;
                if (cg->currentLine != (uintN) off) {
                    if (js_NewSrcNote2(cx, cg, SRC_SETLINE, off) < 0)
                        return JS_FALSE;
                    cg->currentLine = (uintN) off;
                }
            }

            /* The third note offset helps us find the loop-closing jump. */
            if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 2,
                                     CG_OFFSET(cg) - top)) {
                return JS_FALSE;
            }
        }

        /* Emit the loop-closing jump and fixup all jump offsets. */
        jmp = EmitJump(cx, cg, JSOP_GOTO, top - CG_OFFSET(cg));
        if (jmp < 0)
            return JS_FALSE;
        if (beq > 0)
            CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
        if (pn2->pn_type == TOK_IN) {
            /* Set the SRC_WHILE note offset so we can find the closing jump. */
            JS_ASSERT(noteIndex != -1);
            if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, jmp - beq))
                return JS_FALSE;
        }

        /* Now fixup all breaks and continues (before for/in's final POP2). */
        if (!js_PopStatementCG(cx, cg))
            return JS_FALSE;

        if (pn2->pn_type == TOK_IN) {
            /*
             * Generate the object and iterator pop opcodes after popping the
             * stmtInfo stack, so breaks will go to this pop bytecode.
             */
            if (pn3->pn_type != TOK_LB) {
                if (js_Emit1(cx, cg, JSOP_POP2) < 0)
                    return JS_FALSE;
            } else {
                /*
                 * With 'for(x[i]...)', there's only the object on the stack,
                 * so we need to hide the pop.
                 */
                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                    return JS_FALSE;
                if (js_Emit1(cx, cg, JSOP_POP) < 0)
                    return JS_FALSE;
            }
        }
        break;

      case TOK_BREAK:
        stmt = cg->treeContext.topStmt;
        atom = pn->pn_atom;
        if (atom) {
            ale = js_IndexAtom(cx, atom, &cg->atomList);
            if (!ale)
                return JS_FALSE;
            while (stmt->type != STMT_LABEL || stmt->label != atom)
                stmt = stmt->down;
            noteType = SRC_BREAK2LABEL;
        } else {
            ale = NULL;
            while (!STMT_IS_LOOP(stmt) && stmt->type != STMT_SWITCH)
                stmt = stmt->down;
            noteType = SRC_NULL;
        }

        if (EmitGoto(cx, cg, stmt, &stmt->breaks, ale, noteType) < 0)
            return JS_FALSE;
        break;

      case TOK_CONTINUE:
        stmt = cg->treeContext.topStmt;
        atom = pn->pn_atom;
        if (atom) {
            /* Find the loop statement enclosed by the matching label. */
            JSStmtInfo *loop = NULL;
            ale = js_IndexAtom(cx, atom, &cg->atomList);
            if (!ale)
                return JS_FALSE;
            while (stmt->type != STMT_LABEL || stmt->label != atom) {
                if (STMT_IS_LOOP(stmt))
                    loop = stmt;
                stmt = stmt->down;
            }
            stmt = loop;
            noteType = SRC_CONT2LABEL;
        } else {
            ale = NULL;
            while (!STMT_IS_LOOP(stmt))
                stmt = stmt->down;
            noteType = SRC_CONTINUE;
        }

        if (EmitGoto(cx, cg, stmt, &stmt->continues, ale, noteType) < 0)
            return JS_FALSE;
        break;

      case TOK_WITH:
        if (!js_EmitTree(cx, cg, pn->pn_left))
            return JS_FALSE;
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_WITH, CG_OFFSET(cg));
        if (js_Emit1(cx, cg, JSOP_ENTERWITH) < 0)
            return JS_FALSE;
        if (!js_EmitTree(cx, cg, pn->pn_right))
            return JS_FALSE;
        if (js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0)
            return JS_FALSE;
        ok = js_PopStatementCG(cx, cg);
        break;

#if JS_HAS_EXCEPTIONS

      case TOK_TRY: {
        ptrdiff_t start, end, catchStart, finallyCatch, catchJump;
        JSParseNode *iter;
        intN depth;

        /* Quell GCC overwarnings. */
        end = catchStart = finallyCatch = catchJump = -1;

/* Emit JSOP_GOTO that points to the first op after the catch/finally blocks */
#define EMIT_CATCH_GOTO(cx, cg, jmp)                                          \
    EMIT_BACKPATCH_OP(cx, cg, stmtInfo.catchJump, JSOP_BACKPATCH, jmp)

/* Emit JSOP_GOSUB that points to the finally block. */
#define EMIT_FINALLY_GOSUB(cx, cg, jmp)                                       \
    EMIT_BACKPATCH_OP(cx, cg, stmtInfo.gosub, JSOP_BACKPATCH_PUSH, jmp)

        /*
         * Push stmtInfo to track jumps-over-catches and gosubs-to-finally
         * for later fixup.
         *
         * When a finally block is `active' (STMT_FINALLY on the treeContext),
         * non-local jumps (including jumps-over-catches) result in a GOSUB
         * being written into the bytecode stream and fixed-up later (c.f.
         * EMIT_BACKPATCH_OP and BackPatch).
         */
        js_PushStatement(&cg->treeContext, &stmtInfo,
                         pn->pn_kid3 ? STMT_FINALLY : STMT_BLOCK,
                         CG_OFFSET(cg));

        /*
         * About JSOP_SETSP: an exception can be thrown while the stack is in
         * an unbalanced state, and this imbalance causes problems with things
         * like function invocation later on.
         *
         * To fix this, we compute the `balanced' stack depth upon try entry,
         * and then restore the stack to this depth when we hit the first catch
         * or finally block.  We can't just zero the stack, because things like
         * for/in and with that are active upon entry to the block keep state
         * variables on the stack.
         */
        depth = cg->stackDepth;

        /* Mark try location for decompilation, then emit try block. */
        if (js_Emit1(cx, cg, JSOP_TRY) < 0)
            return JS_FALSE;
        start = CG_OFFSET(cg);
        if (!js_EmitTree(cx, cg, pn->pn_kid1))
            return JS_FALSE;

        /* GOSUB to finally, if present. */
        if (pn->pn_kid3) {
            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                return JS_FALSE;
            EMIT_FINALLY_GOSUB(cx, cg, jmp);
            if (jmp < 0)
                return JS_FALSE;
        }

        /* Emit (hidden) jump over catch and/or finally. */
        if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
            return JS_FALSE;
        EMIT_CATCH_GOTO(cx, cg, jmp);
        if (jmp < 0)
            return JS_FALSE;

        end = CG_OFFSET(cg);

        /* If this try has a catch block, emit it. */
        iter = pn->pn_kid2;
        if (iter) {
            catchStart = end;

            /*
             * The emitted code for a catch block looks like:
             *
             * [ popscope ]                        only if 2nd+ catch block
             * name Object
             * pushobj
             * newinit
             * exception
             * initcatchvar <atom>
             * enterwith
             * [< catchguard code >]               if there's a catchguard
             * [ifeq <offset to next catch block>]         " "
             * < catch block contents >
             * leavewith
             * goto <end of catch blocks>          non-local; finally applies
             *
             * If there's no catch block without a catchguard, the last
             * <offset to next catch block> points to rethrow code.  This
             * code will GOSUB to the finally code if appropriate, and is
             * also used for the catch-all trynote for capturing exceptions
             * thrown from catch{} blocks.
             */
            for (;;) {
                JSStmtInfo stmtInfo2;
                JSParseNode *disc;
                ptrdiff_t guardnote;

                if (!UpdateLinenoNotes(cx, cg, iter))
                    return JS_FALSE;

                if (catchJump != -1) {
                    /* Fix up and clean up previous catch block. */
                    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, catchJump);

                    /* Compensate for the [leavewith]. */
                    cg->stackDepth++;
                    JS_ASSERT((uintN) cg->stackDepth <= cg->maxStackDepth);

                    if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                        js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0) {
                        return JS_FALSE;
                    }
                } else {
                    /* Set stack to original depth (see SETSP comment above). */
                    EMIT_ATOM_INDEX_OP(JSOP_SETSP, (jsatomid)depth);
                    cg->stackDepth = depth;
                }

                /* Non-negative guardnote offset is length of catchguard. */
                guardnote = js_NewSrcNote2(cx, cg, SRC_CATCH, 0);
                if (guardnote < 0 ||
                    js_Emit1(cx, cg, JSOP_NOP) < 0) {
                    return JS_FALSE;
                }

                /* Construct the scope holder and push it on. */
                ale = js_IndexAtom(cx, cx->runtime->atomState.ObjectAtom,
                                   &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                EMIT_ATOM_INDEX_OP(JSOP_NAME, ALE_INDEX(ale));

                if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0 ||
                    js_Emit1(cx, cg, JSOP_NEWINIT) < 0 ||
                    js_Emit1(cx, cg, JSOP_EXCEPTION) < 0) {
                    return JS_FALSE;
                }

                /* initcatchvar <atomIndex> */
                disc = iter->pn_kid1;
                ale = js_IndexAtom(cx, disc->pn_atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;

                EMIT_ATOM_INDEX_OP(JSOP_INITCATCHVAR, ALE_INDEX(ale));
                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                    js_Emit1(cx, cg, JSOP_ENTERWITH) < 0) {
                    return JS_FALSE;
                }

                /* boolean_expr */
                if (disc->pn_expr) {
                    ptrdiff_t guardstart = CG_OFFSET(cg);
                    if (!js_EmitTree(cx, cg, disc->pn_expr))
                        return JS_FALSE;
                    if (!js_SetSrcNoteOffset(cx, cg, guardnote, 0,
                                             CG_OFFSET(cg) - guardstart)) {
                        return JS_FALSE;
                    }
                    /* ifeq <next block> */
                    catchJump = EmitJump(cx, cg, JSOP_IFEQ, 0);
                    if (catchJump < 0)
                        return JS_FALSE;
                }

                /* Emit catch block. */
                js_PushStatement(&cg->treeContext, &stmtInfo2, STMT_CATCH,
                                 CG_OFFSET(cg));
                stmtInfo2.label = disc->pn_atom;
                if (!js_EmitTree(cx, cg, iter->pn_kid3))
                    return JS_FALSE;
                js_PopStatementCG(cx, cg);

                /*
                 * Jump over the remaining catch blocks.
                 * This counts as a non-local jump, so do the finally thing.
                 */

                /* popscope */
                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                    js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0) {
                    return JS_FALSE;
                }

                /* gosub <finally>, if required */
                if (pn->pn_kid3) {
                    EMIT_FINALLY_GOSUB(cx, cg, jmp);
                    if (jmp < 0)
                        return JS_FALSE;
                }

                /* This will get fixed up to jump to after catch/finally. */
                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                    return JS_FALSE;
                EMIT_CATCH_GOTO(cx, cg, jmp);
                if (jmp < 0)
                    return JS_FALSE;
                if (!iter->pn_kid2)     /* leave iter at last catch */
                    break;
                iter = iter->pn_kid2;
            }
        }

        /*
         * We use a [leavewith],[gosub],rethrow block for rethrowing
         * when there's no unguarded catch, and also for running finally
         * code while letting an uncaught exception pass through.
         */
        if (pn->pn_kid3 ||
            (catchJump != -1 && iter->pn_kid1->pn_expr)) {
            /*
             * Emit another stack fixup, because the catch could itself
             * throw an exception in an unbalanced state, and the finally
             * may need to call functions.  If there is no finally, only
             * guarded catches, the rethrow code below nevertheless needs
             * stack fixup.
             */
            finallyCatch = CG_OFFSET(cg);
            EMIT_ATOM_INDEX_OP(JSOP_SETSP, (jsatomid)depth);
            cg->stackDepth = depth;

            /* Last discriminant jumps to rethrow if none match. */
            if (catchJump != -1 && iter->pn_kid1->pn_expr) {
                CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, catchJump);

                /* Compensate for the [leavewith]. */
                cg->stackDepth++;
                JS_ASSERT((uintN) cg->stackDepth <= cg->maxStackDepth);

                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                    js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0) {
                    return JS_FALSE;
                }
            }

            if (pn->pn_kid3) {
                EMIT_FINALLY_GOSUB(cx, cg, jmp);
                if (jmp < 0)
                    return JS_FALSE;
                cg->stackDepth = depth;
            }

            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                js_Emit1(cx, cg, JSOP_EXCEPTION) < 0 ||
                js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                js_Emit1(cx, cg, JSOP_THROW) < 0) {
                return JS_FALSE;
            }
            JS_ASSERT(cg->stackDepth == depth);
        }

        /*
         * If we have a finally, it belongs here, and we have to fix up the
         * gosubs that might have been emitted before non-local jumps.
         */
        if (pn->pn_kid3) {
            if (!BackPatch(cx, cg, stmtInfo.gosub, CG_NEXT(cg), JSOP_GOSUB))
                return JS_FALSE;

            /*
             * The stack budget must be balanced at this point, and we need
             * one more slot for the JSOP_RETSUB return address pushed by a
             * JSOP_GOSUB opcode that calls this finally clause.
             */
            JS_ASSERT(cg->stackDepth == depth);
            if ((uintN)++cg->stackDepth > cg->maxStackDepth)
                cg->maxStackDepth = cg->stackDepth;

            /* Now indicate that we're emitting a subroutine body. */
            stmtInfo.type = STMT_SUBROUTINE;
            if (!UpdateLinenoNotes(cx, cg, pn->pn_kid3))
                return JS_FALSE;
            if (js_Emit1(cx, cg, JSOP_FINALLY) < 0 ||
                !js_EmitTree(cx, cg, pn->pn_kid3) ||
                js_Emit1(cx, cg, JSOP_RETSUB) < 0) {
                return JS_FALSE;
            }
        }
        js_PopStatementCG(cx, cg);

        if (js_NewSrcNote(cx, cg, SRC_ENDBRACE) < 0 ||
            js_Emit1(cx, cg, JSOP_NOP) < 0) {
            return JS_FALSE;
        }

        /* Fix up the end-of-try/catch jumps to come here. */
        if (!BackPatch(cx, cg, stmtInfo.catchJump, CG_NEXT(cg), JSOP_GOTO))
            return JS_FALSE;

        /*
         * Add the try note last, to let post-order give us the right ordering
         * (first to last for a given nesting level, inner to outer by level).
         */
        if (pn->pn_kid2) {
            JS_ASSERT(end != -1 && catchStart != -1);
            if (!js_NewTryNote(cx, cg, start, end, catchStart))
                return JS_FALSE;
        }

        /*
         * If we've got a finally, mark try+catch region with additional
         * trynote to catch exceptions (re)thrown from a catch block or
         * for the try{}finally{} case.
         */
        if (pn->pn_kid3) {
            JS_ASSERT(finallyCatch != -1);
            if (!js_NewTryNote(cx, cg, start, finallyCatch, finallyCatch))
                return JS_FALSE;
        }
        break;
      }

#endif /* JS_HAS_EXCEPTIONS */

      case TOK_VAR:
        off = noteIndex = -1;
        for (pn2 = pn->pn_head; ; pn2 = pn2->pn_next) {
            JS_ASSERT(pn2->pn_type == TOK_NAME);
            if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
                return JS_FALSE;
            op = pn2->pn_op;
            if (op == JSOP_ARGUMENTS) {
                JS_ASSERT(!pn2->pn_expr); /* JSOP_ARGUMENTS => no initializer */
#ifdef __GNUC__
                atomIndex = 0;            /* quell GCC overwarning */
#endif
            } else {
                if (pn2->pn_slot >= 0) {
                    atomIndex = (jsatomid) pn2->pn_slot;
                } else {
                    ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
                    if (!ale)
                        return JS_FALSE;
                    atomIndex = ALE_INDEX(ale);

                    if (!(cg->treeContext.flags & TCF_IN_FUNCTION) ||
                        (cg->treeContext.flags & TCF_FUN_HEAVYWEIGHT)) {
                        /* Emit a prolog bytecode to predefine the variable. */
                        CG_SWITCH_TO_PROLOG(cg);
                        EMIT_ATOM_INDEX_OP(pn->pn_op, atomIndex);
                        CG_SWITCH_TO_MAIN(cg);
                    }
                }
                if (pn2->pn_expr) {
                    if (op == JSOP_SETNAME)
                        EMIT_ATOM_INDEX_OP(JSOP_BINDNAME, atomIndex);
                    if (!js_EmitTree(cx, cg, pn2->pn_expr))
                        return JS_FALSE;
                }
            }
            if (pn2 == pn->pn_head &&
                js_NewSrcNote(cx, cg,
                              (pn->pn_op == JSOP_DEFCONST)
                              ? SRC_CONST
                              : SRC_VAR) < 0) {
                return JS_FALSE;
            }
            if (op == JSOP_ARGUMENTS) {
                if (js_Emit1(cx, cg, op) < 0)
                    return JS_FALSE;
            } else {
                EMIT_ATOM_INDEX_OP(op, atomIndex);
            }
            tmp = CG_OFFSET(cg);
            if (noteIndex >= 0) {
                if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, tmp-off))
                    return JS_FALSE;
            }
            if (!pn2->pn_next)
                break;
            off = tmp;
            noteIndex = js_NewSrcNote2(cx, cg, SRC_PCDELTA, 0);
            if (noteIndex < 0 ||
                js_Emit1(cx, cg, JSOP_POP) < 0) {
                return JS_FALSE;
            }
        }
        if (pn->pn_extra) {
            if (js_Emit1(cx, cg, JSOP_POP) < 0)
                return JS_FALSE;
        }
        break;

      case TOK_RETURN:
        /* Push a return value */
        pn2 = pn->pn_kid;
        if (pn2) {
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
        } else {
            if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
                return JS_FALSE;
        }

        /*
         * EmitNonLocalJumpFixup mutates op to JSOP_RETRVAL after emitting a
         * JSOP_SETRVAL if there are open try blocks having finally clauses.
         * We can't simply transfer control flow to our caller in that case,
         * because we must gosub to those clauses from inner to outer, with
         * the correct stack pointer (i.e., after popping any with, for/in,
         * etc., slots nested inside the finally's try).
         */
        op = JSOP_RETURN;
        if (!EmitNonLocalJumpFixup(cx, cg, NULL, &op))
            return JS_FALSE;
        if (js_Emit1(cx, cg, op) < 0)
            return JS_FALSE;
        break;

      case TOK_LC:
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_BLOCK, top);
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
        }
        ok = js_PopStatementCG(cx, cg);
        break;

      case TOK_SEMI:
        pn2 = pn->pn_kid;
        if (pn2) {
            /*
             * Top-level or called-from-a-native JS_Execute/EvaluateScript,
             * debugger, and eval frames may need the value of the ultimate
             * expression statement as the script's result, despite the fact
             * that it appears useless to the compiler.
             */
            useful = !cx->fp->fun ||
                     cx->fp->fun->native ||
                     (cx->fp->flags & JSFRAME_SPECIAL);
            if (!useful) {
                if (!CheckSideEffects(cx, &cg->treeContext, pn2, &useful))
                    return JS_FALSE;
            }
            if (!useful) {
                cg->currentLine = pn2->pn_pos.begin.lineno;
                if (!js_ReportCompileErrorNumber(cx, NULL, cg,
                                                 JSREPORT_WARNING |
                                                 JSREPORT_STRICT,
                                                 JSMSG_USELESS_EXPR)) {
                    return JS_FALSE;
                }
            } else {
                if (!js_EmitTree(cx, cg, pn2))
                    return JS_FALSE;
                if (js_Emit1(cx, cg, JSOP_POPV) < 0)
                    return JS_FALSE;
            }
        }
        break;

      case TOK_COLON:
        /* Emit an annotated nop so we know to decompile a label. */
        atom = pn->pn_atom;
        ale = js_IndexAtom(cx, atom, &cg->atomList);
        if (!ale)
            return JS_FALSE;
        pn2 = pn->pn_expr;
        noteIndex = js_NewSrcNote2(cx, cg,
                                   (pn2->pn_type == TOK_LC)
                                   ? SRC_LABELBRACE
                                   : SRC_LABEL,
                                   (ptrdiff_t) ALE_INDEX(ale));
        if (noteIndex < 0 ||
            js_Emit1(cx, cg, JSOP_NOP) < 0) {
            return JS_FALSE;
        }

        /* Emit code for the labeled statement. */
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_LABEL,
                         CG_OFFSET(cg));
        stmtInfo.label = atom;
        if (!js_EmitTree(cx, cg, pn2))
            return JS_FALSE;
        if (!js_PopStatementCG(cx, cg))
            return JS_FALSE;

        /* If the statement was compound, emit a note for the end brace. */
        if (pn2->pn_type == TOK_LC) {
            if (js_NewSrcNote(cx, cg, SRC_ENDBRACE) < 0 ||
                js_Emit1(cx, cg, JSOP_NOP) < 0) {
                return JS_FALSE;
            }
        }
        break;

      case TOK_COMMA:
        /*
         * Emit SRC_PCDELTA notes on each JSOP_POP between comma operands.
         * These notes help the decompiler bracket the bytecodes generated
         * from each sub-expression that follows a comma.
         */
        off = noteIndex = -1;
        for (pn2 = pn->pn_head; ; pn2 = pn2->pn_next) {
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
            tmp = CG_OFFSET(cg);
            if (noteIndex >= 0) {
                if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, tmp-off))
                    return JS_FALSE;
            }
            if (!pn2->pn_next)
                break;
            off = tmp;
            noteIndex = js_NewSrcNote2(cx, cg, SRC_PCDELTA, 0);
            if (noteIndex < 0 ||
                js_Emit1(cx, cg, JSOP_POP) < 0) {
                return JS_FALSE;
            }
        }
        break;

      case TOK_ASSIGN:
        /*
         * Check left operand type and generate specialized code for it.
         * Specialize to avoid ECMA "reference type" values on the operand
         * stack, which impose pervasive runtime "GetValue" costs.
         */
        pn2 = pn->pn_left;
        JS_ASSERT(pn2->pn_type != TOK_RP);
        atomIndex = (jsatomid) -1; /* Suppress warning. */
        switch (pn2->pn_type) {
          case TOK_NAME:
            if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
                return JS_FALSE;
            if (pn2->pn_slot >= 0) {
                atomIndex = (jsatomid) pn2->pn_slot;
            } else {
                ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                atomIndex = ALE_INDEX(ale);
                EMIT_ATOM_INDEX_OP(JSOP_BINDNAME, atomIndex);
            }
            break;
          case TOK_DOT:
            if (!js_EmitTree(cx, cg, pn2->pn_expr))
                return JS_FALSE;
            ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
            if (!ale)
                return JS_FALSE;
            atomIndex = ALE_INDEX(ale);
            break;
          case TOK_LB:
            if (!js_EmitTree(cx, cg, pn2->pn_left))
                return JS_FALSE;
            if (!js_EmitTree(cx, cg, pn2->pn_right))
                return JS_FALSE;
            break;
#if JS_HAS_LVALUE_RETURN
          case TOK_LP:
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
            break;
#endif
          default:
            JS_ASSERT(0);
        }

        op = pn->pn_op;
#if JS_HAS_GETTER_SETTER
        if (op == JSOP_GETTER || op == JSOP_SETTER) {
            /* We'll emit these prefix bytecodes after emitting the r.h.s. */
        } else
#endif
        /* If += or similar, dup the left operand and get its value. */
        if (op != JSOP_NOP) {
            switch (pn2->pn_type) {
              case TOK_NAME:
                if (pn2->pn_op != JSOP_SETNAME) {
                    EMIT_ATOM_INDEX_OP((pn2->pn_op == JSOP_SETARG)
                                       ? JSOP_GETARG
                                       : JSOP_GETVAR,
                                       atomIndex);
                    break;
                }
                /* FALL THROUGH */
              case TOK_DOT:
                if (js_Emit1(cx, cg, JSOP_DUP) < 0)
                    return JS_FALSE;
                EMIT_ATOM_INDEX_OP(JSOP_GETPROP, atomIndex);
                break;
              case TOK_LB:
#if JS_HAS_LVALUE_RETURN
              case TOK_LP:
#endif
                if (js_Emit1(cx, cg, JSOP_DUP2) < 0)
                    return JS_FALSE;
                if (js_Emit1(cx, cg, JSOP_GETELEM) < 0)
                    return JS_FALSE;
                break;
              default:;
            }
        }

        /* Now emit the right operand (it may affect the namespace). */
        if (!js_EmitTree(cx, cg, pn->pn_right))
            return JS_FALSE;

        /* If += etc., emit the binary operator with a decompiler note. */
        if (op != JSOP_NOP) {
            if (js_NewSrcNote(cx, cg, SRC_ASSIGNOP) < 0 ||
                js_Emit1(cx, cg, op) < 0) {
                return JS_FALSE;
            }
        }

        /* Left parts such as a.b.c and a[b].c need a decompiler note. */
        if (pn2->pn_type != TOK_NAME) {
            if (js_NewSrcNote2(cx, cg, SRC_PCBASE, CG_OFFSET(cg) - top) < 0)
                return JS_FALSE;
        }

        /* Finally, emit the specialized assignment bytecode. */
        switch (pn2->pn_type) {
          case TOK_NAME:
            if (pn2->pn_slot < 0 || !(pn2->pn_attrs & JSPROP_READONLY)) {
          case TOK_DOT:
                EMIT_ATOM_INDEX_OP(pn2->pn_op, atomIndex);
            }
            break;
          case TOK_LB:
#if JS_HAS_LVALUE_RETURN
          case TOK_LP:
#endif
            if (js_Emit1(cx, cg, JSOP_SETELEM) < 0)
                return JS_FALSE;
            break;
          default:;
        }
        break;

      case TOK_HOOK:
        /* Emit the condition, then branch if false to the else part. */
        if (!js_EmitTree(cx, cg, pn->pn_kid1))
            return JS_FALSE;
        noteIndex = js_NewSrcNote(cx, cg, SRC_COND);
        if (noteIndex < 0)
            return JS_FALSE;
        beq = EmitJump(cx, cg, JSOP_IFEQ, 0);
        if (beq < 0 || !js_EmitTree(cx, cg, pn->pn_kid2))
            return JS_FALSE;

        /* Jump around else, fixup the branch, emit else, fixup jump. */
        jmp = EmitJump(cx, cg, JSOP_GOTO, 0);
        if (jmp < 0)
            return JS_FALSE;
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
        if (!js_EmitTree(cx, cg, pn->pn_kid3))
            return JS_FALSE;
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
        if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 0, jmp - beq))
            return JS_FALSE;

        /*
         * Because each branch pushes a single value, but our stack budgeting
         * analysis ignores branches, we now have two values accounted for in
         * cg->stackDepth.  Execution will follow only one path, so we must
         * decrement cg->stackDepth here.  Failing to do this will foil code,
         * such as the try/catch/finally exception handling code generator,
         * that samples cg->stackDepth for use at runtime (JSOP_SETSP).
         */
        JS_ASSERT(cg->stackDepth > 1);
        cg->stackDepth--;
        break;

      case TOK_OR:
      case TOK_AND:
        /*
         * JSOP_OR converts the operand on the stack to boolean, and if true,
         * leaves the original operand value on the stack and jumps; otherwise
         * it pops and falls into the next bytecode, which evaluates the right
         * operand.  The jump goes around the right operand evaluation.
         *
         * JSOP_AND converts the operand on the stack to boolean, and if false,
         * leaves the original operand value on the stack and jumps; otherwise
         * it pops and falls into the right operand's bytecode.
         *
         * Avoid tail recursion for long ||...|| expressions and long &&...&&
         * expressions or long mixtures of ||'s and &&'s that can easily blow
         * the stack, by forward-linking and then backpatching all the JSOP_OR
         * and JSOP_AND bytecodes' immediate jump-offset operands.
         */
        pn3 = pn;
        if (!js_EmitTree(cx, cg, pn->pn_left))
            return JS_FALSE;
        top = EmitJump(cx, cg, JSOP_BACKPATCH_POP, 0);
        if (top < 0)
            return JS_FALSE;
        jmp = top;
        pn2 = pn->pn_right;
        while (pn2->pn_type == TOK_OR || pn2->pn_type == TOK_AND) {
            pn = pn2;
            if (!js_EmitTree(cx, cg, pn->pn_left))
                return JS_FALSE;
            off = EmitJump(cx, cg, JSOP_BACKPATCH_POP, 0);
            if (off < 0)
                return JS_FALSE;
            if (!SetBackPatchDelta(cx, cg, CG_CODE(cg, jmp), off - jmp))
                return JS_FALSE;
            jmp = off;
            pn2 = pn->pn_right;
        }
        if (!js_EmitTree(cx, cg, pn2))
            return JS_FALSE;
        off = CG_OFFSET(cg);
        do {
            jsbytecode *pc = CG_CODE(cg, top);
            tmp = GetJumpOffset(cg, pc);
            CHECK_AND_SET_JUMP_OFFSET(cx, cg, pc, off - top);
            *pc = pn3->pn_op;
            top += tmp;
        } while ((pn3 = pn3->pn_right) != pn2);
        break;

      case TOK_BITOR:
      case TOK_BITXOR:
      case TOK_BITAND:
      case TOK_EQOP:
      case TOK_RELOP:
#if JS_HAS_IN_OPERATOR
      case TOK_IN:
#endif
#if JS_HAS_INSTANCEOF
      case TOK_INSTANCEOF:
#endif
      case TOK_SHOP:
      case TOK_PLUS:
      case TOK_MINUS:
      case TOK_STAR:
      case TOK_DIVOP:
        if (pn->pn_arity == PN_LIST) {
            /* Left-associative operator chain: avoid too much recursion. */
            pn2 = pn->pn_head;
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
            op = pn->pn_op;
            while ((pn2 = pn2->pn_next) != NULL) {
                if (!js_EmitTree(cx, cg, pn2))
                    return JS_FALSE;
                if (js_Emit1(cx, cg, op) < 0)
                    return JS_FALSE;
            }
        } else {
            /* Binary operators that evaluate both operands unconditionally. */
            if (!js_EmitTree(cx, cg, pn->pn_left))
                return JS_FALSE;
            if (!js_EmitTree(cx, cg, pn->pn_right))
                return JS_FALSE;
            if (js_Emit1(cx, cg, pn->pn_op) < 0)
                return JS_FALSE;
        }
        break;

#if JS_HAS_EXCEPTIONS
      case TOK_THROW:
#endif
      case TOK_UNARYOP:
        /* Unary op, including unary +/-. */
        if (!js_EmitTree(cx, cg, pn->pn_kid))
            return JS_FALSE;
        if (js_Emit1(cx, cg, pn->pn_op) < 0)
            return JS_FALSE;
        break;

      case TOK_INC:
      case TOK_DEC:
        /* Emit lvalue-specialized code for ++/-- operators. */
        pn2 = pn->pn_kid;
        JS_ASSERT(pn2->pn_type != TOK_RP);
        op = pn->pn_op;
        switch (pn2->pn_type) {
          case TOK_NAME:
            pn2->pn_op = op;
            if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
                return JS_FALSE;
            op = pn2->pn_op;
            if (pn2->pn_slot >= 0) {
                if (pn2->pn_attrs & JSPROP_READONLY)
                    op = JSOP_GETVAR;
                atomIndex = (jsatomid) pn2->pn_slot;
                EMIT_ATOM_INDEX_OP(op, atomIndex);
            } else {
                if (!EmitAtomOp(cx, pn2, op, cg))
                    return JS_FALSE;
            }
            break;
          case TOK_DOT:
            if (!EmitPropOp(cx, pn2, op, cg))
                return JS_FALSE;
            break;
          case TOK_LB:
            if (!EmitElemOp(cx, pn2, op, cg))
                return JS_FALSE;
            break;
#if JS_HAS_LVALUE_RETURN
          case TOK_LP:
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
            if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
                               CG_OFFSET(cg) - pn2->pn_offset) < 0) {
                return JS_FALSE;
            }
            if (js_Emit1(cx, cg, op) < 0)
                return JS_FALSE;
            break;
#endif
          default:
            JS_ASSERT(0);
        }
        break;

      case TOK_DELETE:
        /* Under ECMA 3, deleting a non-reference returns true. */
        pn2 = pn->pn_kid;
        switch (pn2->pn_type) {
          case TOK_NAME:
            pn2->pn_op = JSOP_DELNAME;
            if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
                return JS_FALSE;
            op = pn2->pn_op;
            if (op == JSOP_FALSE) {
                if (js_Emit1(cx, cg, op) < 0)
                    return JS_FALSE;
            } else {
                if (!EmitAtomOp(cx, pn2, op, cg))
                    return JS_FALSE;
            }
            break;
          case TOK_DOT:
            if (!EmitPropOp(cx, pn2, JSOP_DELPROP, cg))
                return JS_FALSE;
            break;
          case TOK_LB:
            if (!EmitElemOp(cx, pn2, JSOP_DELELEM, cg))
                return JS_FALSE;
            break;
          default:
            if (js_Emit1(cx, cg, JSOP_TRUE) < 0)
                return JS_FALSE;
        }
        break;

      case TOK_DOT:
        /*
         * Pop a stack operand, convert it to object, get a property named by
         * this bytecode's immediate-indexed atom operand, and push its value
         * (not a reference to it).  This bytecode sets the virtual machine's
         * "obj" register to the left operand's ToObject conversion result,
         * for use by JSOP_PUSHOBJ.
         */
        ok = EmitPropOp(cx, pn, pn->pn_op, cg);
        break;

      case TOK_LB:
        /*
         * Pop two operands, convert the left one to object and the right one
         * to property name (atom or tagged int), get the named property, and
         * push its value.  Set the "obj" register to the result of ToObject
         * on the left operand.
         */
        ok = EmitElemOp(cx, pn, pn->pn_op, cg);
        break;

      case TOK_NEW:
      case TOK_LP:
        /*
         * Emit function call or operator new (constructor call) code.
         * First, emit code for the left operand to evaluate the callable or
         * constructable object expression.
         */
        pn2 = pn->pn_head;
        if (!js_EmitTree(cx, cg, pn2))
            return JS_FALSE;

        /* Remember start of callable-object bytecode for decompilation hint. */
        off = pn2->pn_offset;

        /*
         * Push the virtual machine's "obj" register, which was set by a name,
         * property, or element get (or set) bytecode.
         */
        if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
            return JS_FALSE;

        /*
         * Emit code for each argument in order, then emit the JSOP_*CALL or
         * JSOP_NEW bytecode with a two-byte immediate telling how many args
         * were pushed on the operand stack.
         */
        for (pn2 = pn2->pn_next; pn2; pn2 = pn2->pn_next) {
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
        }
        if (js_NewSrcNote2(cx, cg, SRC_PCBASE, CG_OFFSET(cg) - off) < 0)
            return JS_FALSE;
        argc = pn->pn_count - 1;
        if (js_Emit3(cx, cg, pn->pn_op, ARGC_HI(argc), ARGC_LO(argc)) < 0)
            return JS_FALSE;
        break;

#if JS_HAS_INITIALIZERS
      case TOK_RB:
        /*
         * Emit code for [a, b, c] of the form:
         *   t = new Array; t[0] = a; t[1] = b; t[2] = c; t;
         * but use a stack slot for t and avoid dup'ing and popping it via
         * the JSOP_NEWINIT and JSOP_INITELEM bytecodes.
         */
        ale = js_IndexAtom(cx, cx->runtime->atomState.ArrayAtom,
                           &cg->atomList);
        if (!ale)
            return JS_FALSE;
        EMIT_ATOM_INDEX_OP(JSOP_NAME, ALE_INDEX(ale));
        if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
            return JS_FALSE;
        if (js_Emit1(cx, cg, JSOP_NEWINIT) < 0)
            return JS_FALSE;

        pn2 = pn->pn_head;
#if JS_HAS_SHARP_VARS
        if (pn2 && pn2->pn_type == TOK_DEFSHARP) {
            EMIT_ATOM_INDEX_OP(JSOP_DEFSHARP, (jsatomid)pn2->pn_num);
            pn2 = pn2->pn_next;
        }
#endif

        for (atomIndex = 0; pn2; pn2 = pn2->pn_next) {
            /* PrimaryExpr enforced ATOM_INDEX_LIMIT, so in-line optimize. */
            JS_ASSERT(atomIndex < ATOM_INDEX_LIMIT);
            if (atomIndex == 0) {
                if (js_Emit1(cx, cg, JSOP_ZERO) < 0)
                    return JS_FALSE;
            } else if (atomIndex == 1) {
                if (js_Emit1(cx, cg, JSOP_ONE) < 0)
                    return JS_FALSE;
            } else {
                EMIT_ATOM_INDEX_OP(JSOP_UINT16, (jsatomid)atomIndex);
            }

            /* Sub-optimal: holes in a sparse initializer are void-filled. */
            if (pn2->pn_type == TOK_COMMA) {
                if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
                    return JS_FALSE;
            } else {
                if (!js_EmitTree(cx, cg, pn2))
                    return JS_FALSE;
            }
            if (js_Emit1(cx, cg, JSOP_INITELEM) < 0)
                return JS_FALSE;

            atomIndex++;
        }

        if (pn->pn_extra) {
            /* Emit a source note so we know to decompile an extra comma. */
            if (js_NewSrcNote(cx, cg, SRC_CONTINUE) < 0)
                return JS_FALSE;
        }

        /* Emit an op for sharp array cleanup and decompilation. */
        if (js_Emit1(cx, cg, JSOP_ENDINIT) < 0)
            return JS_FALSE;
        break;

      case TOK_RC:
        /*
         * Emit code for {p:a, '%q':b, 2:c} of the form:
         *   t = new Object; t.p = a; t['%q'] = b; t[2] = c; t;
         * but use a stack slot for t and avoid dup'ing and popping it via
         * the JSOP_NEWINIT and JSOP_INITELEM bytecodes.
         */
        ale = js_IndexAtom(cx, cx->runtime->atomState.ObjectAtom,
                           &cg->atomList);
        if (!ale)
            return JS_FALSE;
        EMIT_ATOM_INDEX_OP(JSOP_NAME, ALE_INDEX(ale));

        if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
            return JS_FALSE;
        if (js_Emit1(cx, cg, JSOP_NEWINIT) < 0)
            return JS_FALSE;

        pn2 = pn->pn_head;
#if JS_HAS_SHARP_VARS
        if (pn2 && pn2->pn_type == TOK_DEFSHARP) {
            EMIT_ATOM_INDEX_OP(JSOP_DEFSHARP, (jsatomid)pn2->pn_num);
            pn2 = pn2->pn_next;
        }
#endif

        for (; pn2; pn2 = pn2->pn_next) {
            /* Emit an index for t[2], else map an atom for t.p or t['%q']. */
            pn3 = pn2->pn_left;
            switch (pn3->pn_type) {
              case TOK_NUMBER:
                if (!EmitNumberOp(cx, pn3->pn_dval, cg))
                    return JS_FALSE;
                break;
              case TOK_NAME:
              case TOK_STRING:
                ale = js_IndexAtom(cx, pn3->pn_atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                break;
              default:
                JS_ASSERT(0);
            }

            /* Emit code for the property initializer. */
            if (!js_EmitTree(cx, cg, pn2->pn_right))
                return JS_FALSE;

#if JS_HAS_GETTER_SETTER
            op = pn2->pn_op;
            if (op == JSOP_GETTER || op == JSOP_SETTER) {
                if (js_Emit1(cx, cg, op) < 0)
                    return JS_FALSE;
            }
#endif
            /* Annotate JSOP_INITELEM so we decompile 2:c and not just c. */
            if (pn3->pn_type == TOK_NUMBER) {
                if (js_NewSrcNote(cx, cg, SRC_LABEL) < 0)
                    return JS_FALSE;
                if (js_Emit1(cx, cg, JSOP_INITELEM) < 0)
                    return JS_FALSE;
            } else {
                EMIT_ATOM_INDEX_OP(JSOP_INITPROP, ALE_INDEX(ale));
            }
        }

        /* Emit an op for sharpArray cleanup and decompilation. */
        if (js_Emit1(cx, cg, JSOP_ENDINIT) < 0)
            return JS_FALSE;
        break;

#if JS_HAS_SHARP_VARS
      case TOK_DEFSHARP:
        if (!js_EmitTree(cx, cg, pn->pn_kid))
            return JS_FALSE;
        EMIT_ATOM_INDEX_OP(JSOP_DEFSHARP, (jsatomid) pn->pn_num);
        break;

      case TOK_USESHARP:
        EMIT_ATOM_INDEX_OP(JSOP_USESHARP, (jsatomid) pn->pn_num);
        break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_INITIALIZERS */

      case TOK_RP:
        /*
         * The node for (e) has e as its kid, enabling users who want to nest
         * assignment expressions in conditions to avoid the error correction
         * done by Condition (from x = y to x == y) by double-parenthesizing.
         */
        if (!js_EmitTree(cx, cg, pn->pn_kid))
            return JS_FALSE;
        if (js_Emit1(cx, cg, JSOP_GROUP) < 0)
            return JS_FALSE;
        break;

      case TOK_NAME:
        if (!LookupArgOrVar(cx, &cg->treeContext, pn))
            return JS_FALSE;
        op = pn->pn_op;
        if (op == JSOP_ARGUMENTS) {
            if (js_Emit1(cx, cg, op) < 0)
                return JS_FALSE;
            break;
        }
        if (pn->pn_slot >= 0) {
            atomIndex = (jsatomid) pn->pn_slot;
            EMIT_ATOM_INDEX_OP(op, atomIndex);
            break;
        }
        /* FALL THROUGH */
      case TOK_STRING:
      case TOK_OBJECT:
        /*
         * The scanner and parser associate JSOP_NAME with TOK_NAME, although
         * other bytecodes may result instead (JSOP_BINDNAME/JSOP_SETNAME,
         * JSOP_FORNAME, etc.).  Among JSOP_*NAME* variants, only JSOP_NAME
         * may generate the first operand of a call or new expression, so only
         * it sets the "obj" virtual machine register to the object along the
         * scope chain in which the name was found.
         *
         * Token types for STRING and OBJECT have corresponding bytecode ops
         * in pn_op and emit the same format as NAME, so they share this code.
         */
        ok = EmitAtomOp(cx, pn, pn->pn_op, cg);
        break;

      case TOK_NUMBER:
        ok = EmitNumberOp(cx, pn->pn_dval, cg);
        break;

      case TOK_PRIMARY:
        if (js_Emit1(cx, cg, pn->pn_op) < 0)
            return JS_FALSE;
        break;

#if JS_HAS_DEBUGGER_KEYWORD
      case TOK_DEBUGGER:
        if (js_Emit1(cx, cg, JSOP_DEBUGGER) < 0)
            return JS_FALSE;
        break;
#endif /* JS_HAS_DEBUGGER_KEYWORD */

      default:
        JS_ASSERT(0);
    }

    if (ok && --cg->emitLevel == 0 && cg->spanDeps)
        ok = OptimizeSpanDeps(cx, cg);

    return ok;
}

JS_FRIEND_DATA(JSSrcNoteSpec) js_SrcNoteSpec[] = {
    {"null",            0,      0,      0},
    {"if",              0,      0,      0},
    {"if-else",         1,      0,      1},
    {"while",           1,      0,      1},
    {"for",             3,      1,      1},
    {"continue",        0,      0,      0},
    {"var",             0,      0,      0},
    {"pcdelta",         1,      0,      1},
    {"assignop",        0,      0,      0},
    {"cond",            1,      0,      1},
    {"reserved0",       0,      0,      0},
    {"hidden",          0,      0,      0},
    {"pcbase",          1,      0,     -1},
    {"label",           1,      0,      0},
    {"labelbrace",      1,      0,      0},
    {"endbrace",        0,      0,      0},
    {"break2label",     1,      0,      0},
    {"cont2label",      1,      0,      0},
    {"switch",          2,      0,      1},
    {"funcdef",         1,      0,      0},
    {"catch",           1,     11,      1},
    {"const",           0,      0,      0},
    {"newline",         0,      0,      0},
    {"setline",         1,      0,      0},
    {"xdelta",          0,      0,      0},
};

static intN
AllocSrcNote(JSContext *cx, JSCodeGenerator *cg)
{
    intN index;
    JSArenaPool *pool;
    size_t size;

    index = cg->noteCount;
    if (((uintN)index & cg->noteMask) == 0) {
        pool = &cx->notePool;
        size = SRCNOTE_SIZE(cg->noteMask + 1);
        if (!cg->notes) {
            /* Allocate the first note array lazily; leave noteMask alone. */
            JS_ARENA_ALLOCATE_CAST(cg->notes, jssrcnote *, pool, size);
        } else {
            /* Grow by doubling note array size; update noteMask on success. */
            JS_ARENA_GROW_CAST(cg->notes, jssrcnote *, pool, size, size);
            if (cg->notes)
                cg->noteMask = (cg->noteMask << 1) | 1;
        }
        if (!cg->notes) {
            JS_ReportOutOfMemory(cx);
            return -1;
        }
    }

    cg->noteCount = index + 1;
    return index;
}

intN
js_NewSrcNote(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type)
{
    intN index, n;
    jssrcnote *sn;
    ptrdiff_t offset, delta, xdelta;

    /*
     * Claim a note slot in cg->notes by growing it if necessary and then
     * incrementing cg->noteCount.
     */
    index = AllocSrcNote(cx, cg);
    if (index < 0)
        return -1;
    sn = &cg->notes[index];

    /*
     * Compute delta from the last annotated bytecode's offset.  If it's too
     * big to fit in sn, allocate one or more xdelta notes and reset sn.
     */
    offset = CG_OFFSET(cg);
    delta = offset - cg->lastNoteOffset;
    cg->lastNoteOffset = offset;
    if (delta >= SN_DELTA_LIMIT) {
        do {
            xdelta = JS_MIN(delta, SN_XDELTA_MASK);
            SN_MAKE_XDELTA(sn, xdelta);
            delta -= xdelta;
            index = AllocSrcNote(cx, cg);
            if (index < 0)
                return -1;
            sn = &cg->notes[index];
        } while (delta >= SN_DELTA_LIMIT);
    }

    /*
     * Initialize type and delta, then allocate the minimum number of notes
     * needed for type's arity.  Usually, we won't need more, but if an offset
     * does take two bytes, js_SetSrcNoteOffset will grow cg->notes.
     */
    SN_MAKE_NOTE(sn, type, delta);
    for (n = (intN)js_SrcNoteSpec[type].arity; n > 0; n--) {
        if (js_NewSrcNote(cx, cg, SRC_NULL) < 0)
            return -1;
    }
    return index;
}

intN
js_NewSrcNote2(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type,
               ptrdiff_t offset)
{
    intN index;

    index = js_NewSrcNote(cx, cg, type);
    if (index >= 0) {
        if (!js_SetSrcNoteOffset(cx, cg, index, 0, offset))
            return -1;
    }
    return index;
}

intN
js_NewSrcNote3(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type,
               ptrdiff_t offset1, ptrdiff_t offset2)
{
    intN index;

    index = js_NewSrcNote(cx, cg, type);
    if (index >= 0) {
        if (!js_SetSrcNoteOffset(cx, cg, index, 0, offset1))
            return -1;
        if (!js_SetSrcNoteOffset(cx, cg, index, 1, offset2))
            return -1;
    }
    return index;
}

static JSBool
GrowSrcNotes(JSContext *cx, JSCodeGenerator *cg)
{
    JSArenaPool *pool;
    size_t size;

    /* Grow by doubling note array size; update noteMask on success. */
    pool = &cx->notePool;
    size = SRCNOTE_SIZE(cg->noteMask + 1);
    JS_ARENA_GROW_CAST(cg->notes, jssrcnote *, pool, size, size);
    if (!cg->notes) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    cg->noteMask = (cg->noteMask << 1) | 1;
    return JS_TRUE;
}

jssrcnote *
js_AddToSrcNoteDelta(JSContext *cx, JSCodeGenerator *cg, jssrcnote *sn,
                     ptrdiff_t delta)
{
    ptrdiff_t base, limit, newdelta, diff;
    intN index;

    JS_ASSERT(delta < SN_XDELTA_LIMIT);
    base = SN_DELTA(sn);
    limit = SN_IS_XDELTA(sn) ? SN_XDELTA_LIMIT : SN_DELTA_LIMIT;
    newdelta = base + delta;
    if (newdelta < limit) {
        SN_SET_DELTA(sn, newdelta);
    } else {
        index = sn - cg->notes;
        if ((cg->noteCount & cg->noteMask) == 0) {
            if (!GrowSrcNotes(cx, cg))
                return NULL;
            sn = cg->notes + index;
        }
        diff = cg->noteCount - index;
        cg->noteCount++;
        memmove(sn + 1, sn, SRCNOTE_SIZE(diff));
        SN_MAKE_XDELTA(sn, delta);
        sn++;
    }
    return sn;
}

uintN
js_SrcNoteLength(jssrcnote *sn)
{
    uintN arity;
    jssrcnote *base;

    arity = (intN)js_SrcNoteSpec[SN_TYPE(sn)].arity;
    for (base = sn++; arity--; sn++) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }
    return sn - base;
}

JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, uintN which)
{
    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    JS_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    JS_ASSERT(which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }
    if (*sn & SN_3BYTE_OFFSET_FLAG) {
        return (ptrdiff_t)((((uint32)(sn[0] & SN_3BYTE_OFFSET_MASK)) << 16)
                           | (sn[1] << 8) | sn[2]);
    }
    return (ptrdiff_t)*sn;
}

JSBool
js_SetSrcNoteOffset(JSContext *cx, JSCodeGenerator *cg, uintN index,
                    uintN which, ptrdiff_t offset)
{
    jssrcnote *sn;
    ptrdiff_t diff;

    if ((jsuword)offset >= (jsuword)(((ptrdiff_t)SN_3BYTE_OFFSET_FLAG) << 16)) {
        ReportStatementTooLarge(cx, cg);
        return JS_FALSE;
    }

    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    sn = &cg->notes[index];
    JS_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    JS_ASSERT(which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }

    /* See if the new offset requires three bytes. */
    if (offset > (ptrdiff_t)SN_3BYTE_OFFSET_MASK) {
        /* Maybe this offset was already set to a three-byte value. */
        if (!(*sn & SN_3BYTE_OFFSET_FLAG)) {
            /* Losing, need to insert another two bytes for this offset. */
            index = PTRDIFF(sn, cg->notes, jssrcnote);

            /*
             * Simultaneously test to see if the source note array must grow to
             * accomodate either the first or second byte of additional storage
             * required by this 3-byte offset.
             */
            if (((cg->noteCount + 1) & cg->noteMask) <= 1) {
                if (!GrowSrcNotes(cx, cg))
                    return JS_FALSE;
                sn = cg->notes + index;
            }
            cg->noteCount += 2;

            diff = cg->noteCount - (index + 3);
            JS_ASSERT(diff >= 0);
            if (diff > 0)
                memmove(sn + 3, sn + 1, SRCNOTE_SIZE(diff));
        }
        *sn++ = (jssrcnote)(SN_3BYTE_OFFSET_FLAG | (offset >> 16));
        *sn++ = (jssrcnote)(offset >> 8);
    }
    *sn = (jssrcnote)offset;
    return JS_TRUE;
}

jssrcnote *
js_FinishTakingSrcNotes(JSContext *cx, JSCodeGenerator *cg)
{
    uintN count;
    jssrcnote *tmp, *final;

    count = cg->noteCount;
    tmp   = cg->notes;
    final = (jssrcnote *) JS_malloc(cx, SRCNOTE_SIZE(count + 1));
    if (!final)
        return NULL;
    memcpy(final, tmp, SRCNOTE_SIZE(count));
    SN_MAKE_TERMINATOR(&final[count]);
    return final;
}

JSBool
js_AllocTryNotes(JSContext *cx, JSCodeGenerator *cg)
{
    size_t size, incr;
    ptrdiff_t delta;

    size = TRYNOTE_SIZE(cg->treeContext.tryCount);
    if (size <= cg->tryNoteSpace)
        return JS_TRUE;

    /*
     * Allocate trynotes from cx->tempPool.
     * XXX Too much growing and we bloat, as other tempPool allocators block
     * in-place growth, and we never recycle old free space in an arena.
     * YYY But once we consume an entire arena, we'll realloc it, letting the
     * malloc heap recycle old space, while still freeing _en masse_ via the
     * arena pool.
     */
    if (!cg->tryBase) {
        size = JS_ROUNDUP(size, TRYNOTE_SIZE(TRYNOTE_GRAIN));
        JS_ARENA_ALLOCATE_CAST(cg->tryBase, JSTryNote *, &cx->tempPool, size);
        if (!cg->tryBase)
            return JS_FALSE;
        cg->tryNoteSpace = size;
        cg->tryNext = cg->tryBase;
    } else {
        delta = PTRDIFF((char *)cg->tryNext, (char *)cg->tryBase, char);
        incr = size - cg->tryNoteSpace;
        incr = JS_ROUNDUP(incr, TRYNOTE_SIZE(TRYNOTE_GRAIN));
        size = cg->tryNoteSpace;
        JS_ARENA_GROW_CAST(cg->tryBase, JSTryNote *, &cx->tempPool, size, incr);
        if (!cg->tryBase)
            return JS_FALSE;
        cg->tryNoteSpace = size + incr;
        cg->tryNext = (JSTryNote *)((char *)cg->tryBase + delta);
    }
    return JS_TRUE;
}

JSTryNote *
js_NewTryNote(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t start,
              ptrdiff_t end, ptrdiff_t catchStart)
{
    JSTryNote *tn;

    JS_ASSERT(cg->tryBase <= cg->tryNext);
    JS_ASSERT(catchStart >= 0);
    tn = cg->tryNext++;
    tn->start = start;
    tn->length = end - start;
    tn->catchStart = catchStart;
    return tn;
}

JSBool
js_FinishTakingTryNotes(JSContext *cx, JSCodeGenerator *cg, JSTryNote **tryp)
{
    uintN count;
    JSTryNote *tmp, *final;

    count = PTRDIFF(cg->tryNext, cg->tryBase, JSTryNote);
    if (!count) {
        *tryp = NULL;
        return JS_TRUE;
    }

    tmp = cg->tryBase;
    final = (JSTryNote *) JS_malloc(cx, TRYNOTE_SIZE(count + 1));
    if (!final) {
        *tryp = NULL;
        return JS_FALSE;
    }
    memcpy(final, tmp, TRYNOTE_SIZE(count));
    final[count].start = 0;
    final[count].length = CG_OFFSET(cg);
    final[count].catchStart = 0;
    *tryp = final;
    return JS_TRUE;
}
