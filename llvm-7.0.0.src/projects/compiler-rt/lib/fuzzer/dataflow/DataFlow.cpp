/*===- DataFlow.cpp - a standalone DataFlow tracer                  -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// An experimental data-flow tracer for fuzz targets.
// It is based on DFSan and SanitizerCoverage.
// https://clang.llvm.org/docs/DataFlowSanitizer.html
// https://clang.llvm.org/docs/SanitizerCoverage.html#tracing-data-flow
//
// It executes the fuzz target on the given input while monitoring the
// data flow for every instrumented comparison instruction.
//
// The output shows which functions depend on which bytes of the input.
//
// Build:
//   1. Compile this file with -fsanitize=dataflow
//   2. Build the fuzz target with -g -fsanitize=dataflow
//       -fsanitize-coverage=trace-pc-guard,pc-table,func,trace-cmp
//   3. Link those together with -fsanitize=dataflow
//
//  -fsanitize-coverage=trace-cmp inserts callbacks around every comparison
//  instruction, DFSan modifies the calls to pass the data flow labels.
//  The callbacks update the data flow label for the current function.
//  See e.g. __dfsw___sanitizer_cov_trace_cmp1 below.
//
//  -fsanitize-coverage=trace-pc-guard,pc-table,func instruments function
//  entries so that the comparison callback knows that current function.
//
//
// Run:
//   # Collect data flow for INPUT_FILE, write to OUTPUT_FILE (default: stdout)
//   ./a.out INPUT_FILE [OUTPUT_FILE]
//
//   # Print all instrumented functions. llvm-symbolizer must be present in PATH
//   ./a.out
//
// Example output:
// ===============
//  F0 11111111111111
//  F1 10000000000000
//  ===============
// "FN xxxxxxxxxx": tells what bytes of the input does the function N depend on.
//    The byte string is LEN+1 bytes. The last byte is set if the function
//    depends on the input length.
//===----------------------------------------------------------------------===*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <execinfo.h>  // backtrace_symbols_fd

#include <sanitizer/dfsan_interface.h>

extern "C" {
extern int LLVMFuzzerTestOneInput(const unsigned char *Data, size_t Size);
__attribute__((weak)) extern int LLVMFuzzerInitialize(int *argc, char ***argv);
} // extern "C"

static const char *Input;
static size_t InputLen;
static size_t NumIterations;

static const int kMaxLabels = 1 << (sizeof(dfsan_label) * 8);
static const float init_deriv = 1.0;
static const uint32_t kMaxBugTargets = 1000;
static int num_errors = 0; // Ensure all filter statements still printed

struct Metadata {
  dfsan_label_info local_dfsan_label_info[kMaxLabels];
  int num_dfsan_labels;
};

static Metadata **MetadataPerIter;  // NumIterations x Metadata;

struct BugTarget {
  int (*loss) (int x, int f_x);
  uint64_t srcId; // byte number in general
  uint64_t opcode; 
  uint64_t sinkId; // index in the Metadata array 
};

static BugTarget **BugTargets;

int byte_overflow_loss(int x, int f_x) {
  return 257 - f_x;
}


// from dfsan.cc
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

void PopulateMetadata(Metadata * md) {
    assert(md);
    memset(md->local_dfsan_label_info, 0, sizeof(md->local_dfsan_label_info));
    size_t cnt = dfsan_get_label_count();
    md->num_dfsan_labels = cnt;
    for (dfsan_label l = 1; l <= cnt+1; ++l) {
      const struct dfsan_label_info* out_dli = dfsan_get_label_info(l);
      memcpy(&(md->local_dfsan_label_info[l]), out_dli, sizeof(dfsan_label_info));
    }
}

void print_full_info() {

  for (size_t Iter = 0; Iter < NumIterations; Iter++) {
    if (MetadataPerIter[Iter]->num_dfsan_labels <= 1) {
        continue;
    }

    for (dfsan_label l = 1; l <= MetadataPerIter[Iter]->num_dfsan_labels; ++l) {
        const struct dfsan_label_info* i_info = &MetadataPerIter[Iter]->local_dfsan_label_info[l];
        const char* opName = opcodeNames[i_info->opcode];
        fprintf(stderr, "FILTER, %zu, %d, %f, %f, %f, %f, %s, %d, %s, %s,\n", Iter, l, i_info->neg_dydx, i_info->pos_dydx, i_info->neg_bound, i_info->pos_bound, i_info->loc, i_info->f_val, opName, Input);
    }
    fprintf(stderr, "\n");
  }
}

int int_overflow_filter(BugTarget** bugtargets, int current_cnt) {

  int new_cnt = current_cnt;
  for (size_t Iter = 0; Iter < NumIterations; Iter++) {
    if (MetadataPerIter[Iter]->num_dfsan_labels <= 1) {
        continue;
    }

    for (dfsan_label l = 1; l <= MetadataPerIter[Iter]->num_dfsan_labels; ++l) {
        const struct dfsan_label_info* i_info = &MetadataPerIter[Iter]->local_dfsan_label_info[l];
        if (i_info->opcode == ADD) {
            bugtargets[new_cnt]->srcId = Iter;
            bugtargets[new_cnt]->sinkId = l;
            bugtargets[new_cnt]->opcode = i_info->opcode;
            bugtargets[new_cnt]->loss = &byte_overflow_loss;
            new_cnt += 1;
            if (new_cnt >= kMaxBugTargets) {
                num_errors += 1;
                new_cnt = 0;
            }
        }
        const char* opName = opcodeNames[i_info->opcode];
        fprintf(stderr, "FILTER, %zu, %d, %f, %f, %f, %f, %s, %d, %s, %s,\n", Iter, l, i_info->neg_dydx, i_info->pos_dydx, i_info->neg_bound, i_info->pos_bound, i_info->loc, i_info->f_val, opName, Input);
    }
    fprintf(stderr, "\n");
  }
  return new_cnt;
}

void newton_optimizer(BugTarget * bt, unsigned char *Buf) {
  uint64_t srcId = bt->srcId;
  uint64_t sinkId = bt->sinkId;
  uint64_t opcode = bt->opcode;
  size_t maxEpochs = 50;

  for (size_t epoch = 0; epoch < maxEpochs; epoch++) {
    // clear gradients
    dfsan_flush();

    // mark gradient
    dfsan_label L = dfsan_create_label("x", init_deriv);
    dfsan_set_label(L, Buf + srcId, 1);

    // execute function
    LLVMFuzzerTestOneInput(Buf, InputLen);

    int Iter = 0;
    PopulateMetadata(MetadataPerIter[Iter]);

    uint8_t x = Buf[srcId];

    int f_x = 0;
    float fderiv_x_n = 0.0;
    float fderiv_x_p = 0.0;
    const struct dfsan_label_info* i_info = &MetadataPerIter[Iter]->local_dfsan_label_info[sinkId];
    // if they differe, either non-determinsim or the trace has changed in the midst of the optimization
    //assert(i_info->opcode == opcode);
    if (i_info->opcode != opcode) {
      fprintf(stderr, "INFO, FAILED OPCODECHECK, , , , , , , , , %s,\n", Input); 
      return;
    }
    f_x = i_info->f_val;
    fderiv_x_p = i_info->pos_dydx;
    fderiv_x_n = i_info->neg_dydx;
    
    // gradient update rule based on Newton's method
    int l_x = bt->loss(x, f_x);//257 - f_x;

    // TODO: figure out alternative for ceil() function, as -0.1 gradient should go to -1 but goes to 0, consider absolute value usage
    float second_term = -1.0 * ceil(fderiv_x_p / f_x); // -1 due to g(x) loss function
    int lrate = 2;
    uint8_t new_val = x - (uint8_t) lrate * second_term;
    //fprintf(stderr, "%f second term, %d new_val", second_term, new_val);

    Buf[srcId] = new_val;

    // TODO: early stopping and matplotlib plot of the loss per epoch

    fprintf(stderr, "OPT, %lu, %lu, %d, %d, %f, %f, %d, %zd, %zd, %s,\n", srcId, sinkId, x, f_x, fderiv_x_n, fderiv_x_p, Buf[srcId], epoch, maxEpochs, Input);


  }
}

// String formatting
//OPT      , src_id    , sink_id, old_x       , f_x    , f'_x_n , f'_x_p , new_x  , epoch  , epochs , Input
//COLLECT  , num_labels, iter   , total_iter  ,        ,        ,        ,        ,        ,        , Input
//FILTER   , iter      , lbl    , n_grad      , p_grad , neg_bd , pos_bd , loc    , f_val  , op_name, Input
//

extern "C"

int main(int argc, char **argv) {
  if (LLVMFuzzerInitialize)
    LLVMFuzzerInitialize(&argc, &argv);

  assert(argc == 2);
  Input = argv[1];

  FILE *In = fopen(Input, "r"); 
  assert(In); 
  fseek(In, 0, SEEK_END);
  InputLen = ftell(In);
  fseek(In, 0, SEEK_SET);
  unsigned char *Buf = (unsigned char*)malloc(InputLen);
  size_t NumBytesRead = fread(Buf, 1, InputLen, In);
  assert(NumBytesRead == InputLen);
  fclose(In);

  dfsan_flush();
  char *val = getenv("LIBFUZZER_BYTE_IDX");
  if (val) {
    dfsan_label L = dfsan_create_label("input_byte", init_deriv);
    char *endptr;
    long mark_ind = strtol(val, &endptr, 10);
    assert(mark_ind >= 0);
    assert(mark_ind < InputLen);
    dfsan_set_label(L, Buf + mark_ind, 1);
  }

  LLVMFuzzerTestOneInput(Buf, InputLen);
}
/*
int main(int argc, char **argv) {
  if (LLVMFuzzerInitialize)
    LLVMFuzzerInitialize(&argc, &argv);

  bool grad_only = false;
  bool single_execution = false;
  if ((argc == 3) && (strncmp(argv[1], "-g", 2)==0) ){
    grad_only = true;
    Input = argv[2];
  } else if ((argc == 3) && (strncmp(argv[1], "-s", 2)==0) ) {
    single_execution = true; 
    Input = argv[2];
  } else {
    assert(argc == 2);
    Input = argv[1];
  }

  fprintf(stderr, "INFO, reading, , , , , , , , , %s,\n", Input); 
  FILE *In = fopen(Input, "r"); 
  assert(In); 
  fseek(In, 0, SEEK_END);
  InputLen = ftell(In);
  fseek(In, 0, SEEK_SET);
  unsigned char *Buf = (unsigned char*)malloc(InputLen);
  size_t NumBytesRead = fread(Buf, 1, InputLen, In);
  assert(NumBytesRead == InputLen);
  fclose(In);
  if (single_execution) {
    LLVMFuzzerTestOneInput(Buf, InputLen);
    return 0;
  }


  NumIterations = NumBytesRead;

  MetadataPerIter =
          (Metadata **)calloc(NumIterations, sizeof(Metadata *));
  for (size_t Iter = 0; Iter < NumIterations; Iter++) {
     MetadataPerIter[Iter] =
          (Metadata *)calloc(1, sizeof(Metadata));
  }

  BugTargets =
          (BugTarget **)calloc(kMaxBugTargets, sizeof(BugTarget *));
  for (size_t i = 0; i < kMaxBugTargets; i++) {
     BugTargets[i] =
          (BugTarget *)calloc(1, sizeof(BugTarget));
  }

  // Collect gradients over a file wrt to each input byte
  for (size_t Iter = 0; Iter < NumIterations; Iter++) {
    dfsan_flush();

    dfsan_label L = dfsan_create_label("input_byte", init_deriv);
    dfsan_set_label(L, Buf + Iter, 1);

    char * val = getenv("ENABLE_FREAD");
    if (val) {
        char str[12];
        sprintf(str, "%zd", Iter);
        setenv("GRSAN_BYTE_IDX", str, 1);
        fprintf(stderr, "%s\n", getenv("GRSAN_BYTE_IDX"));
    }

    LLVMFuzzerTestOneInput(Buf, InputLen);

    PopulateMetadata(MetadataPerIter[Iter]);
    fprintf(stderr, "COLLECT, %d, %zd, %zd, , , , , , , %s,\n", MetadataPerIter[Iter]->num_dfsan_labels, Iter, NumIterations, Input);
  }
  
  if (grad_only) {
    print_full_info();
  } else {
  // Select which bytes to optimize over (filtering)
  // install new filters here by adding a ftn / increasing the size of array (filters)
    const int kNumFilters = 1;
    int (*filters[kNumFilters])(BugTarget** bugtargets, int current_cnt) = {int_overflow_filter};

    int bugTargetCnt = 0;
    for (int i = 0; i < kNumFilters; i++) {
      bugTargetCnt += filters[i](BugTargets, bugTargetCnt); // populate BugTargets and return how many added
    }
    //assert(!overflow);
    if (num_errors) {
        fprintf(stderr, "INFO, ERROROverflowBugTargets %d, , , , , , , , , %s,\n", bugTargetCnt+num_errors*kMaxBugTargets, Input);
        return 0;
    }
    fprintf(stderr, "INFO, BugTargets %d, , , , , , , , , %s,\n", bugTargetCnt, Input);

    // Perform Optimization
    for (int i = 0; i < bugTargetCnt; i++) {
        newton_optimizer(BugTargets[i], Buf);
    }
    for (size_t Iter = 0; Iter < bugTargetCnt; Iter++) {
       free(BugTargets[Iter]);
    }
    free(BugTargets);
  }

  free(Buf);
  for (size_t Iter = 0; Iter < NumIterations; Iter++) {
     free(MetadataPerIter[Iter]);
  }
  free(MetadataPerIter); 
}
*/

extern "C" {

void __sanitizer_cov_trace_pc_guard_init(uint32_t *start,
                                         uint32_t *stop) {
}

void __sanitizer_cov_pcs_init(const uintptr_t *pcs_beg,
                              const uintptr_t *pcs_end) {
}

void __sanitizer_cov_trace_pc_indir(uint64_t x){}  // unused.

void __sanitizer_cov_trace_pc_guard(uint32_t *guard){
}

void __dfsw___sanitizer_cov_trace_switch(uint64_t Val, uint64_t *Cases,
                                         dfsan_label L1, dfsan_label UnusedL) {
}

#define HOOK(Name, Type)                                                       \
  void Name(Type Arg1, Type Arg2, dfsan_label L1, dfsan_label L2) {            \
  }

//HOOK(__dfsw___sanitizer_cov_trace_const_cmp1, uint8_t)
//HOOK(__dfsw___sanitizer_cov_trace_const_cmp2, uint16_t)
//HOOK(__dfsw___sanitizer_cov_trace_const_cmp4, uint32_t)
//HOOK(__dfsw___sanitizer_cov_trace_const_cmp8, uint64_t)
//HOOK(__dfsw___sanitizer_cov_trace_cmp1, uint8_t)
//HOOK(__dfsw___sanitizer_cov_trace_cmp2, uint16_t)
//HOOK(__dfsw___sanitizer_cov_trace_cmp4, uint32_t)
//HOOK(__dfsw___sanitizer_cov_trace_cmp8, uint64_t)

} // extern "C"
