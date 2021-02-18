//===-- dfsan.cc ----------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of DataFlowSanitizer.
//
// DataFlowSanitizer runtime.  This file defines the public interface to
// DataFlowSanitizer as well as the definition of certain runtime functions
// called automatically by the compiler (specifically the instrumentation pass
// in llvm/lib/Transforms/Instrumentation/DataFlowSanitizer.cpp).
//
// The public interface is defined in include/sanitizer/dfsan_interface.h whose
// functions are prefixed dfsan_ while the compiler interface functions are
// prefixed __dfsan_.
//===----------------------------------------------------------------------===//
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#include "sanitizer_common/sanitizer_atomic.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_file.h"
#include "sanitizer_common/sanitizer_flags.h"
#include "sanitizer_common/sanitizer_flag_parser.h"
#include "sanitizer_common/sanitizer_libc.h"

#include "dfsan/dfsan.h"
#include "stdint.h"


#include "gradtest_macros.h"

#define DEBUG false

#define BRANCH_RECORDS_SIZE 1048576
#define FUNC_ARGS_SIZE 65535

using namespace __dfsan;

typedef atomic_uint16_t atomic_dfsan_label;
static const dfsan_label kInitializingLabel = -1;

static const uptr kNumLabels = 1 << (sizeof(dfsan_label) * 8);

static const float LOG2 = 0.69314718056;

//
// Note: If you add more structures, please change dfsan_flush()
//
static atomic_dfsan_label __dfsan_last_label;
static dfsan_label_info __dfsan_label_info[kNumLabels];

// record:
static atomic_uint64_t __dfsan_record_index;
static atomic_uint16_t __dfsan_arg_index;
static branch_record __branch_records[BRANCH_RECORDS_SIZE];
static func_arg_record __func_arg_records[FUNC_ARGS_SIZE];

int gr_mode_perf = 0;

Flags __dfsan::flags_data;

SANITIZER_INTERFACE_ATTRIBUTE THREADLOCAL dfsan_label __dfsan_retval_tls;
SANITIZER_INTERFACE_ATTRIBUTE THREADLOCAL dfsan_label __dfsan_arg_tls[64];

SANITIZER_INTERFACE_ATTRIBUTE uptr __dfsan_shadow_ptr_mask;

// On Linux/x86_64, memory is laid out as follows:
//
// +--------------------+ 0x800000000000 (top of memory)
// | application memory |
// +--------------------+ 0x700000008000 (kAppAddr)
// |                    |
// |       unused       |
// |                    |
// +--------------------+ 0x200200000000 (kUnusedAddr)
// |    union table     |
// +--------------------+ 0x200000000000 (kUnionTableAddr)
// |   shadow memory    |
// +--------------------+ 0x000000010000 (kShadowAddr)
// | reserved by kernel |
// +--------------------+ 0x000000000000
//
// To derive a shadow memory address from an application memory address,
// bits 44-46 are cleared to bring the address into the range
// [0x000000008000,0x100000000000).  Then the address is shifted left by 1 to
// account for the double byte representation of shadow labels and move the
// address into the shadow memory range.  See the function shadow_for below.

// On Linux/MIPS64, memory is laid out as follows:
//
// +--------------------+ 0x10000000000 (top of memory)
// | application memory |
// +--------------------+ 0xF000008000 (kAppAddr)
// |                    |
// |       unused       |
// |                    |
// +--------------------+ 0x2200000000 (kUnusedAddr)
// |    union table     |
// +--------------------+ 0x2000000000 (kUnionTableAddr)
// |   shadow memory    |
// +--------------------+ 0x0000010000 (kShadowAddr)
// | reserved by kernel |
// +--------------------+ 0x0000000000

// On Linux/AArch64 (39-bit VMA), memory is laid out as follow:
//
// +--------------------+ 0x8000000000 (top of memory)
// | application memory |
// +--------------------+ 0x7000008000 (kAppAddr)
// |                    |
// |       unused       |
// |                    |
// +--------------------+ 0x1200000000 (kUnusedAddr)
// |    union table     |
// +--------------------+ 0x1000000000 (kUnionTableAddr)
// |   shadow memory    |
// +--------------------+ 0x0000010000 (kShadowAddr)
// | reserved by kernel |
// +--------------------+ 0x0000000000

// On Linux/AArch64 (42-bit VMA), memory is laid out as follow:
//
// +--------------------+ 0x40000000000 (top of memory)
// | application memory |
// +--------------------+ 0x3ff00008000 (kAppAddr)
// |                    |
// |       unused       |
// |                    |
// +--------------------+ 0x1200000000 (kUnusedAddr)
// |    union table     |
// +--------------------+ 0x8000000000 (kUnionTableAddr)
// |   shadow memory    |
// +--------------------+ 0x0000010000 (kShadowAddr)
// | reserved by kernel |
// +--------------------+ 0x0000000000

// On Linux/AArch64 (48-bit VMA), memory is laid out as follow:
//
// +--------------------+ 0x1000000000000 (top of memory)
// | application memory |
// +--------------------+ 0xffff00008000 (kAppAddr)
// |       unused       |
// +--------------------+ 0xaaaab0000000 (top of PIE address)
// | application PIE    |
// +--------------------+ 0xaaaaa0000000 (top of PIE address)
// |                    |
// |       unused       |
// |                    |
// +--------------------+ 0x1200000000 (kUnusedAddr)
// |    union table     |
// +--------------------+ 0x8000000000 (kUnionTableAddr)
// |   shadow memory    |
// +--------------------+ 0x0000010000 (kShadowAddr)
// | reserved by kernel |
// +--------------------+ 0x0000000000

//typedef atomic_dfsan_label dfsan_union_table_t[kNumLabels][kNumLabels];

#ifdef DFSAN_RUNTIME_VMA
// Runtime detected VMA size.
int __dfsan::vmaSize;
#endif

static uptr UnusedAddr() {
  return MappingArchImpl<MAPPING_UNION_TABLE_ADDR>();
//         + sizeof(dfsan_union_table_t);
}

// Checks we do not run out of labels.
static void dfsan_check_label(dfsan_label label) {
  if (label == kInitializingLabel) {
    Report("FATAL: DataFlowSanitizer: out of labels\n");
    Die();
  }
}


static const char* supportedLabel(bool supported) {
  return supported ? "supported" : "UNSUPPORTED";
}

/* Cmp Instruction Predicates (copied from InstrTypes.h:885) */
enum Predicate {
  // Opcode              U L G E    Intuitive operation
  FCMP_FALSE =  0,  ///< 0 0 0 0    Always false (always folded)
  FCMP_OEQ   =  1,  ///< 0 0 0 1    True if ordered and equal
  FCMP_OGT   =  2,  ///< 0 0 1 0    True if ordered and greater than
  FCMP_OGE   =  3,  ///< 0 0 1 1    True if ordered and greater than or equal
  FCMP_OLT   =  4,  ///< 0 1 0 0    True if ordered and less than
  FCMP_OLE   =  5,  ///< 0 1 0 1    True if ordered and less than or equal
  FCMP_ONE   =  6,  ///< 0 1 1 0    True if ordered and operands are unequal
  FCMP_ORD   =  7,  ///< 0 1 1 1    True if ordered (no nans)
  FCMP_UNO   =  8,  ///< 1 0 0 0    True if unordered: isnan(X) | isnan(Y)
  FCMP_UEQ   =  9,  ///< 1 0 0 1    True if unordered or equal
  FCMP_UGT   = 10,  ///< 1 0 1 0    True if unordered or greater than
  FCMP_UGE   = 11,  ///< 1 0 1 1    True if unordered, greater than, or equal
  FCMP_ULT   = 12,  ///< 1 1 0 0    True if unordered or less than
  FCMP_ULE   = 13,  ///< 1 1 0 1    True if unordered, less than, or equal
  FCMP_UNE   = 14,  ///< 1 1 1 0    True if unordered or not equal
  FCMP_TRUE  = 15,  ///< 1 1 1 1    Always true (always folded)
  FIRST_FCMP_PREDICATE = FCMP_FALSE,
  LAST_FCMP_PREDICATE = FCMP_TRUE,
  BAD_FCMP_PREDICATE = FCMP_TRUE + 1,
  ICMP_EQ    = 32,  ///< equal
  ICMP_NE    = 33,  ///< not equal
  ICMP_UGT   = 34,  ///< unsigned greater than
  ICMP_UGE   = 35,  ///< unsigned greater or equal
  ICMP_ULT   = 36,  ///< unsigned less than
  ICMP_ULE   = 37,  ///< unsigned less or equal
  ICMP_SGT   = 38,  ///< signed greater than
  ICMP_SGE   = 39,  ///< signed greater or equal
  ICMP_SLT   = 40,  ///< signed less than
  ICMP_SLE   = 41,  ///< signed less or equal
  FIRST_ICMP_PREDICATE = ICMP_EQ,
  LAST_ICMP_PREDICATE = ICMP_SLE,
  BAD_ICMP_PREDICATE = ICMP_SLE + 1
};

static const char* opcodeNames[] = {"",
  "Ret",
  "Br",
  "Switch",
  "IndirectBr",
  "Invoke",
  "Resume",
  "Unreachable",
  "CleanupRet",
  "CatchRet",
  "CatchSwitch",
  "Add",
  "FAdd",
  "Sub",
  "FSub",
  "Mul",
  "FMul",
  "UDiv",
  "SDiv",
  "FDiv",
  "URem",
  "SRem",
  "FRem",
  "Shl",
  "LShr",
  "AShr",
  "And",
  "Or",
  "Xor",
  "Alloca",
  "Load",
  "Store",
  "GetElementPtrt",
  "Fence",
  "AtomicCmpXchgst",
  "AtomicRMW",
  "Trunc",
  "ZExt",
  "SExt",
  "FPToUI",
  "FPToSI",
  "UIToFP",
  "SIToFP",
  "FPTrunc",
  "FPExt",
  "PtrToInt",
  "IntToPtr",
  "BitCast",
  "AddrSpaceCast",
  "CleanupPad",
  "CatchPad",
  "ICmp",
  "FCmp",
  "PHI",
  "Call",
  "Select",
  "UserOp1",
  "UserOp2",
  "VAArg",
  "ExtractElement",
  "InsertElement",
  "ShuffleVector",
  "ExtractValue",
  "InsertValue",
  "LandingPad"
};

/* from include/llvm/IR/Instruction.def */
enum OpCode {
  RET = 1,
  BR = 2,
  SWITCH = 3,
  INDIRECTBR = 4,
  INVOKE = 5,
  RESUME = 6,
  UNREACHABLE = 7,
  CLEANUPRET = 8,
  CATCHRET = 9,
  CATCHSWITCH = 10,

  ADD = 11,
  FADD = 12,
  SUB = 13,
  FSUB = 14,
  MUL = 15,
  FMUL = 16,
  UDIV = 17,
  SDIV = 18,
  FDIV = 19,
  UREM = 20,
  SREM = 21,
  FREM = 22,

  SHL = 23,
  LSHR = 24,
  ASHR = 25,
  AND = 26,
  OR = 27,
  XOR = 28,

  ALLOCA = 29,
  LOAD = 30,
  STORE = 31,
  GETELEMENTPTRT = 32,
  FENCE = 33,
  ATOMICCMPXCHGST = 34,
  ATOMICRMW = 35,

  TRUNC = 36,
  ZEXT = 37,
  SEXT = 38,
  FPTOUI = 39,
  FPTOSI = 40,
  UITOFP = 41,
  SITOFP = 42,
  FPTRUNC = 43,
  FPEXT = 44,
  PTRTOINT = 45,
  INTTOPTR = 46,
  BITCAST = 47,
  ADDRSPACECAST = 48,

  CLEANUPPAD = 49,
  CATCHPAD = 50,
  
  ICMP = 51,
  FCMP = 52,
  PHI = 53,
  CALL = 54,
  SELECT = 55,
  USEROP1 = 56,
  USEROP2 = 57,
  VAARG = 58,
  EXTRACTELEMENT = 59,
  INSERTELEMENT = 60,
  SHUFFLEVECTOR = 61,
  EXTRACTVALUE = 62,
  INSERTVALUE = 63,
  LANDINGPAD = 64,
};


void float2str(char * buf, float f, size_t len) {
  snprintf(buf, len, "%f", f);
}


void double2str(char * buf, double f, size_t len) {
  snprintf(buf, len, "%f", f);
}

__attribute__((constructor))
void gr_check_perf_mode() {
  const char *s = getenv("GRSAN_DISABLE_LOGGING");
  if ((NULL != s) && (0 != strncmp(s, "0", 1)) && 
                     (0 != strncmp(s, "false", 5))) {
    gr_mode_perf = 1;
  } else {
    gr_mode_perf = 0;
  }
  return;
}

void record_branch(unsigned long file_id, unsigned long inst_id, dfsan_label lhs_label, dfsan_label rhs_label,
        float lhs_v, float rhs_v, bool cond, uint32_t is_ptr, const char* location) {
  /* should have a nonzero label */

  uint64_t index = atomic_fetch_add(&__dfsan_record_index, 1, memory_order_relaxed);

  if (index >= BRANCH_RECORDS_SIZE) {
    Printf("ERROR: Out of branch record space!\n");
    Die();
  }

  __branch_records[index] = {file_id, inst_id, lhs_label, rhs_label, lhs_v, rhs_v,
                             __dfsan_label_info[lhs_label].neg_dydx,
                             __dfsan_label_info[lhs_label].pos_dydx,
                             __dfsan_label_info[rhs_label].neg_dydx,
                             __dfsan_label_info[rhs_label].pos_dydx,
                             cond, is_ptr, location};
}

void record_arg(unsigned long file_id, unsigned int inst_id, unsigned int arg_ind, dfsan_label label,
        float v, const char* location) {
  /* if this gets called label should be nonzero */

  if (!gr_mode_perf) {
    uint16_t index = atomic_fetch_add(&__dfsan_arg_index, 1, memory_order_relaxed);

    if (index >= FUNC_ARGS_SIZE) {
      Printf("ERROR: Out of func record space!\n");
      Die();
    }

    __func_arg_records[index] = {file_id, inst_id, arg_ind, label, v,
                                 __dfsan_label_info[label].neg_dydx,
                                 __dfsan_label_info[label].pos_dydx,
                                 location};
  }
}


extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void __memcpy(void *dest, const void *src, unsigned long n,
                    dfsan_label dest_label, dfsan_label src_label,
                    dfsan_label n_label,
                    const char* location) {
  unsigned long ret_addr = (unsigned long)__builtin_return_address(0);
  if (dest_label) record_arg(ret_addr, 6, 0, dest_label, 0, location);
  if (src_label) record_arg(ret_addr, 6, 1, src_label, 0, location);
  if (n_label) record_arg(ret_addr, 6, 2, n_label, (float)n, location);
  dfsan_memcpy(dest, src, n);
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
dfsan_label __dfsan_union_unsupported_type(dfsan_label l1, dfsan_label l2, uptr insnID, u16 opcode,
        const char* location) {

  // if inputs unlabeled can return early
  if (l1 == 0 && l2 == 0) {
    return 0;
  }

  dfsan_label label = atomic_fetch_add(&__dfsan_last_label, 1, memory_order_relaxed) + 1;
  dfsan_check_label(label);

  const char* opName = opcodeNames[opcode];

  float neg_dydx = nanf("unsupported type");
  float pos_dydx = nanf("unsupported type");


  // update derivative
  __dfsan_label_info[label].l1 = l1;
  __dfsan_label_info[label].l2 = l2;
  __dfsan_label_info[label].opcode = opcode;
  __dfsan_label_info[label].neg_dydx = neg_dydx;
  __dfsan_label_info[label].pos_dydx = pos_dydx;
  __dfsan_label_info[label].loc = location;

  // print result
  if (DEBUG) {
    Printf("%d: %d %d dx %s %s -- %s %s (TYPE) insnID: %u\n",
           label, l1, l2, "nan", "nan", opName,
           supportedLabel(opcode), insnID);
  }


  return label;
}

//#include "gradtest_macros.h"
// TAG1
// INSERT indented.txt here for easier use with debugger
// TAG2
DFSAN_FLOAT_UNION(__dfsan_union_float, float)
DFSAN_FLOAT_UNION(__dfsan_union_double, double)
DFSAN_FLOAT_BRANCH(__branch_visitor_float, float, "float", float2str)
DFSAN_FLOAT_BRANCH(__branch_visitor_double, double, "double", double2str)

DFSAN_INT_UNION(__dfsan_union_byte, u8, uint8_t, int8_t, char)
DFSAN_INT_UNION(__dfsan_union_short, u16, uint16_t, int16_t, int16_t)
DFSAN_INT_UNION(__dfsan_union, int, uint32_t, int, long)
DFSAN_INT_UNION(__dfsan_union_long, long, uint64_t, long, long)

DFSAN_INT_BRANCH(__branch_visitor_char, uint8_t, int8_t, "char")
DFSAN_INT_BRANCH(__branch_visitor_short, uint16_t, int16_t, "short")
DFSAN_INT_BRANCH(__branch_visitor_int, uint32_t, int32_t, "int")
DFSAN_INT_BRANCH(__branch_visitor_long, uint64_t, int64_t, "long")
DFSAN_INT_BRANCH(__branch_visitor_longlong, __uint128_t, __int128_t, "longlong")

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
dfsan_label __dfsan_union_load(const dfsan_label *ls, uptr n) {
  dfsan_label label = ls[0];
  for (uptr i = 1; i != n; ++i) {
    dfsan_label next_label = ls[i];
    if (label != next_label) {
      Printf("ERROR Non-instrumented call to dfsan_union via dfsan_union_load\n");
      Printf("label %d != next_label %d\n", label, next_label);
      label = __dfsan_union(label, next_label, 0, 0, 0, 0, 0);
      if (true) {
          Die();
      }
    }
  }
  return label;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void __dfsan_unimplemented(char *fname) {
  if (flags().warn_unimplemented)
    if (DEBUG) {
      Report("WARNING: DataFlowSanitizer: call to uninstrumented function %s\n",
             fname);
    }

}

// Use '-mllvm -dfsan-debug-nonzero-labels' and break on this function
// to try to figure out where labels are being introduced in a nominally
// label-free program.
extern "C" SANITIZER_INTERFACE_ATTRIBUTE void __dfsan_nonzero_label() {
  if (flags().warn_nonzero_labels)
    Report("WARNING: DataFlowSanitizer: saw nonzero label\n");
}

// Indirect call to an uninstrumented vararg function. We don't have a way of
// handling these at the moment.
extern "C" SANITIZER_INTERFACE_ATTRIBUTE void
__dfsan_vararg_wrapper(const char *fname) {
  if (DEBUG) {
    Report("FATAL: DataFlowSanitizer: unsupported indirect call to vararg "
           "function %s\n", fname);

  }
//  Die();
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
dfsan_label dfsan_create_label(const char *desc) {
  dfsan_label label =    
          atomic_fetch_add(&__dfsan_last_label, 1, memory_order_relaxed) + 1;
  dfsan_check_label(label);
  __dfsan_label_info[label].l1 = __dfsan_label_info[label].l2 = 0;
  __dfsan_label_info[label].loc = desc;
  __dfsan_label_info[label].neg_dydx = 1.0;
  __dfsan_label_info[label].pos_dydx = 1.0;

  return label;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void __dfsan_set_label(dfsan_label label, void *addr, uptr size) {
  for (dfsan_label *labelp = shadow_for(addr); size != 0; --size, ++labelp) {
    // Don't write the label if it is already the value we need it to be.
    // In a program where most addresses are not labeled, it is common that
    // a page of shadow memory is entirely zeroed.  The Linux copy-on-write
    // implementation will share all of the zeroed pages, making a copy of a
    // page when any value is written.  The un-sharing will happen even if
    // the value written does not change the value in memory.  Avoiding the
    // write when both |label| and |*labelp| are zero dramatically reduces
    // the amount of real memory used by large programs.
    if (label == *labelp)
      continue;

    *labelp = label;
  }
}

SANITIZER_INTERFACE_ATTRIBUTE
void dfsan_set_label(dfsan_label label, void *addr, uptr size) {
  __dfsan_set_label(label, addr, size);
}

SANITIZER_INTERFACE_ATTRIBUTE
void dfsan_add_label(dfsan_label label, void *addr, uptr size) {
  for (dfsan_label *labelp = shadow_for(addr); size != 0; --size, ++labelp)
    if (*labelp != label) {
      Printf("ERROR already labeled");
      Die();
    }
}

// Unlike the other dfsan interface functions the behavior of this function
// depends on the label of one of its arguments.  Hence it is implemented as a
// custom function.
extern "C" SANITIZER_INTERFACE_ATTRIBUTE dfsan_label
__dfsw_dfsan_get_label(long data, dfsan_label data_label,
                       dfsan_label *ret_label) {
  *ret_label = 0;
  return data_label;
}

SANITIZER_INTERFACE_ATTRIBUTE dfsan_label
dfsan_read_label(const void *addr, uptr size) {
  if (size == 0)
    return 0;
  return __dfsan_union_load(shadow_for(addr), size);
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
const struct  dfsan_label_info *dfsan_get_label_info(dfsan_label label) {
  return &__dfsan_label_info[label];
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE int
dfsan_has_label(dfsan_label label, dfsan_label elem) {
  if (label == elem)
    return true;
  const dfsan_label_info *info = dfsan_get_label_info(label);
  if (info->l1 != 0) {
    return dfsan_has_label(info->l1, elem) || dfsan_has_label(info->l2, elem);
  } else {
    return false;
  }
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE dfsan_label
dfsan_has_label_with_desc(dfsan_label label, const char *desc) {
  const dfsan_label_info *info = dfsan_get_label_info(label);
  if (info->l1 != 0) {
    return dfsan_has_label_with_desc(info->l1, desc) ||
           dfsan_has_label_with_desc(info->l2, desc);
  } else {
    return internal_strcmp(desc, info->loc) == 0;
  }
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE uptr
dfsan_get_label_count(void) {
  dfsan_label max_label_allocated =
          atomic_load(&__dfsan_last_label, memory_order_relaxed);

  return static_cast<uptr>(max_label_allocated);
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE void
dfsan_dump_labels(int fd) {
  dfsan_label last_label =
      atomic_load(&__dfsan_last_label, memory_order_relaxed);

  char buf[512] = "label,ndx,pdx,location,f_val,opcode\n";

  WriteToFile(fd, buf, internal_strlen(buf));
  // NOTE: Label 0 is unused
  for (uptr l = 1; l <= last_label; ++l) {

    struct dfsan_label_info* i_info = &__dfsan_label_info[l];

    const char* opName = opcodeNames[i_info->opcode];
    snprintf(buf, 512, "%lu,%f,%f,%s,%d,%s", l, i_info->neg_dydx, 
      i_info->pos_dydx, i_info->loc, i_info->f_val, opName);

    WriteToFile(fd, buf, internal_strlen(buf));
    WriteToFile(fd, "\n", 1);
  }
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE void
dfsan_dump_branches(int fd) {
  unsigned long last_index =
          atomic_load(&__dfsan_record_index, memory_order_relaxed);

  char buf[512] = "file_id,inst_id,lhs_label,rhs_label,lhs_val,rhs_val,lhs_ndx,lhs_pdx,rhs_ndx,rhs_pdx,cond_val,zero,is_ptr,location\n";

  WriteToFile(fd, buf, internal_strlen(buf));
  for (uptr l = 0; l < last_index; ++l) {
    struct branch_record br = __branch_records[l];

    char lhs_v_s[32], rhs_v_s[32];
    char lhs_ndx_s[32], lhs_pdx_s[32], rhs_ndx_s[32], rhs_pdx_s[32];
    float2str(lhs_ndx_s, br.lhs_ndx, 32);
    float2str(lhs_pdx_s, br.lhs_pdx, 32);
    float2str(rhs_ndx_s, br.rhs_ndx, 32);
    float2str(rhs_pdx_s, br.rhs_pdx, 32);

    float2str(lhs_v_s, br.lhs_v, 32);
    float2str(rhs_v_s, br.rhs_v, 32);


    bool zero = (br.lhs_ndx == 0) && (br.lhs_pdx == 0) &&
                (br.rhs_ndx == 0) && (br.rhs_pdx == 0);

    internal_snprintf(buf, sizeof(buf), "%zu,%u,%u,%u,%s,%s,%s,%s,%s,%s,%u,%u,%u,%s",
                      br.file_id, br.inst_id, br.lhs_label, br.rhs_label, lhs_v_s, rhs_v_s,
                      lhs_ndx_s, lhs_pdx_s, rhs_ndx_s, rhs_pdx_s,
                      br.cond, zero, br.is_ptr, br.loc);


    WriteToFile(fd, buf, internal_strlen(buf));

    WriteToFile(fd, "\n", 1);
  }
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE void
dfsan_dump_func_args(int fd) {
  unsigned short last_index =
          atomic_load(&__dfsan_arg_index, memory_order_relaxed);

  char buf[512] = "file_id,inst_id,arg_ind,label,val,ndx,pdx,location\n";

  WriteToFile(fd, buf, internal_strlen(buf));

  for (uptr l = 0; l < last_index; ++l) {
    struct func_arg_record br = __func_arg_records[l];

    char lhs_v_s[32];
    char lhs_ndx_s[32], lhs_pdx_s[32];
    float2str(lhs_ndx_s, br.ndx, 32);
    float2str(lhs_pdx_s, br.pdx, 32);

    float2str(lhs_v_s, br.v, 32);

    internal_snprintf(buf, sizeof(buf), "%zu,%u,%u,%u,%s,%s,%s,%s",
                      br.file_id, br.inst_id, br.arg_ind, br.label, lhs_v_s, lhs_ndx_s, lhs_pdx_s, br.loc);


    WriteToFile(fd, buf, internal_strlen(buf));

    WriteToFile(fd, "\n", 1);
  }
}

void Flags::SetDefaults() {
#define DFSAN_FLAG(Type, Name, DefaultValue, Description) Name = DefaultValue;
#include "dfsan_flags.inc"
#undef DFSAN_FLAG
}

static void RegisterDfsanFlags(FlagParser *parser, Flags *f) {
#define DFSAN_FLAG(Type, Name, DefaultValue, Description) \
  RegisterFlag(parser, #Name, Description, &f->Name);
#include "dfsan_flags.inc"
#undef DFSAN_FLAG
}

static void InitializeFlags() {
  SetCommonFlagsDefaults();
  flags().SetDefaults();

  FlagParser parser;
  RegisterCommonFlags(&parser);
  RegisterDfsanFlags(&parser, &flags());
  parser.ParseString(GetEnv("DFSAN_OPTIONS"));
  InitializeCommonFlags();
  if (Verbosity()) ReportUnrecognizedFlags();
  if (common_flags()->help) parser.PrintFlagDescriptions();
}

static void InitializePlatformEarly() {
  AvoidCVE_2016_2143();
#ifdef DFSAN_RUNTIME_VMA
  __dfsan::vmaSize =
    (MostSignificantSetBitIndex(GET_CURRENT_FRAME()) + 1);
  if (__dfsan::vmaSize == 39 || __dfsan::vmaSize == 42 ||
      __dfsan::vmaSize == 48) {
    __dfsan_shadow_ptr_mask = ShadowMask();
  } else {
    Printf("FATAL: DataFlowSanitizer: unsupported VMA range\n");
    Printf("FATAL: Found %d - Supported 39, 42, and 48\n", __dfsan::vmaSize);
    Die();
  }
#endif
}

static void dfsan_fini() {
  if (gr_mode_perf) {
    return;
  }

  if (internal_strcmp(flags().gradient_logfile, "") != 0) {
    fd_t fd = OpenFile(flags().gradient_logfile, WrOnly);
    if (fd == kInvalidFd) {
      Report("WARNING: DataFlowSanitizer: unable to open output file %s\n",
             flags().gradient_logfile);
      return;
    }

    if (DEBUG) {
      Report("INFO: DataFlowSanitizer: dumping derivatives to %s\n",
             flags().gradient_logfile);
    }

    dfsan_dump_labels(fd);
    CloseFile(fd);
  }

  if (internal_strcmp(flags().branch_logfile, "") != 0) {
    fd_t fd = OpenFile(flags().branch_logfile, WrOnly);
    if (fd == kInvalidFd) {
      Report("WARNING: DataFlowSanitizer: unable to open output file %s\n",
             flags().branch_logfile);
      return;
    }

    if (DEBUG) {
      Report("INFO: DataFlowSanitizer: dumping branches to %s\n",
             flags().branch_logfile);
    }

    dfsan_dump_branches(fd);
    CloseFile(fd);
  }

  if (internal_strcmp(flags().func_logfile, "") != 0) {
    fd_t fd = OpenFile(flags().func_logfile, WrOnly);
    if (fd == kInvalidFd) {
      Report("WARNING: DataFlowSanitizer: unable to open output file %s\n",
             flags().func_logfile);
      return;
    }

    if (DEBUG) {
      Report("INFO: DataFlowSanitizer: dumping branches to %s\n",
             flags().func_logfile);
    }

    dfsan_dump_func_args(fd);
    CloseFile(fd);
  }
}

// Used if you want to reset the shadow memory for in-process fuzzing
extern "C" void dfsan_flush() {
  UnmapOrDie((void*)ShadowAddr(), UnusedAddr() - ShadowAddr());
  if (!MmapFixedNoReserve(ShadowAddr(), UnusedAddr() - ShadowAddr()))
    Die();

  memset(__dfsan_label_info, 0, sizeof(dfsan_label_info)*kNumLabels);
  memset(__branch_records, 0, sizeof(branch_record)*BRANCH_RECORDS_SIZE);
  memset(__func_arg_records, 0, sizeof(func_arg_record)*FUNC_ARGS_SIZE);

  atomic_store(&__dfsan_last_label, 0, memory_order_relaxed);
  atomic_store(&__dfsan_arg_index, 0, memory_order_relaxed);
  atomic_store(&__dfsan_record_index, 0, memory_order_relaxed);
}

static void dfsan_init(int argc, char **argv, char **envp) {
  InitializeFlags();

  InitializePlatformEarly();

  if (!MmapFixedNoReserve(ShadowAddr(), UnusedAddr() - ShadowAddr()))
    Die();

  // Protect the region of memory we don't use, to preserve the one-to-one
  // mapping from application to shadow memory. But if ASLR is disabled, Linux
  // will load our executable in the middle of our unused region. This mostly
  // works so long as the program doesn't use too much memory. We support this
  // case by disabling memory protection when ASLR is disabled.
  uptr init_addr = (uptr)&dfsan_init;
  if (!(init_addr >= UnusedAddr() && init_addr < AppAddr()))
    MmapFixedNoAccess(UnusedAddr(), AppAddr() - UnusedAddr());

  InitializeInterceptors();

  // Register the fini callback to run when the program terminates successfully
  // or it is killed by the runtime.
  Atexit(dfsan_fini);
  AddDieCallback(dfsan_fini);

  __dfsan_label_info[kInitializingLabel].loc = "<init label>";
}

#if SANITIZER_CAN_USE_PREINIT_ARRAY
__attribute__((section(".preinit_array"), used))
static void (*dfsan_init_ptr)(int, char **, char **) = dfsan_init;
#endif
