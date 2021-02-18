
#ifdef GR_MODE_PERF
#define record_branch(file_id, br_id, lhs, rhs, lhs_v, rhs_v, cond, location)	{;}
#define record_arg(file_id, inst_id, arg_ind, label, v, location)       	{;}
#endif

/* OPCODES defined in include/llvm/IR/Instruction.def */

#define DFSAN_INT_UNION(FunctionName, Type, UnsignedDivType, SignedDivType, BitwiseType)      \
extern "C" SANITIZER_INTERFACE_ATTRIBUTE \
dfsan_label FunctionName(dfsan_label l1, dfsan_label l2 , Type x1, Type x2, uptr insnID, u16 opcode, char* location) { \
  extern int gr_mode_perf; \
  bool reuse_labels = flags().reuse_labels;\
  bool supported = true; \
  int nsamples = 1; \
  int f_val = -1; \
  const char* opName = opcodeNames[opcode]; \
  float neg_dx1 = 0, neg_dx2 = 0, pos_dx1 = 0, pos_dx2 = 0, \
          neg_dydx = 0, pos_dydx = 0; \
  if (l1 == 0 && l2 == 0) { \
    return 0; \
  } \
  if (l1 != 0) { \
    neg_dx1 = __dfsan_label_info[l1].neg_dydx;\
    pos_dx1 = __dfsan_label_info[l1].pos_dydx;\
  }\
  if (l2 != 0) {\
    neg_dx2 = __dfsan_label_info[l2].neg_dydx;\
    pos_dx2 = __dfsan_label_info[l2].pos_dydx;\
  }\
  if (reuse_labels) {\
    if (neg_dx1 == 0 && pos_dx1 == 0 && neg_dx2 == 0 && pos_dx2 == 0) {\
      return l1 ? l1 : l2;\
    }\
  }\
  switch (opcode) { \
    case ADD:  \
      neg_dydx = neg_dx1 + neg_dx2; \
      pos_dydx = pos_dx1 + pos_dx2; \
      f_val = x1 + x2; \
      break; \
    case SUB:  \
      neg_dydx = neg_dx1 - neg_dx2;\
      pos_dydx = pos_dx1 - pos_dx2;\
      f_val = x1 - x2; \
      break;\
    case MUL:\
      neg_dydx = x1 * neg_dx2 + x2 * neg_dx1;\
      pos_dydx = x1 * pos_dx2 + x2 * pos_dx1;\
      f_val = x1 * x2; \
      break;\
    case SDIV: \
      if (l2) {\
        unsigned long ret_addr = (unsigned long)__builtin_return_address(0);\
        if (!gr_mode_perf) record_arg(ret_addr, 18, 0, l2, (float)x2, location);\
      }\
      if (x2 != 0) {\
        neg_dydx = (x2 * neg_dx1 - x1 * neg_dx2) / x2;\
        pos_dydx = (x2 * pos_dx1 - x1 * pos_dx2) / x2;\
        f_val = x1 / x2; \
      } else {\
        neg_dydx = nanf("div 0");\
        pos_dydx = nanf("div 0");\
        assert(false); \
      }\
      break;\
    case UREM: { /*Urem*/\
      if (l2) {\
        unsigned long ret_addr = (unsigned long)__builtin_return_address(0);\
        if (!gr_mode_perf) record_arg(ret_addr, 20, 0, l2, (float)x2, location);\
      }\
      nsamples = flags().samples;\
      UnsignedDivType ux1 = UnsignedDivType(x1), ux2 = UnsignedDivType(x2);\
      UnsignedDivType y = ux1 % ux2;\
      f_val = y; \
      if (DEBUG) {\
        printf("  URem neg_dx1 %f %u,  neg_dx2 %f %u, pos_dx1 %f %u, pos_dx2 %f %u\n",\
              neg_dx1, UnsignedDivType(neg_dx1), neg_dx2, UnsignedDivType(neg_dx2),\
              pos_dx1, UnsignedDivType(pos_dx1), pos_dx2, UnsignedDivType(pos_dx2));\
        printf("    x1 %d ux1 %u,  x2 %d ux2 %u\n", x1, ux1, x2, ux2);\
      }\
      for (int smp=1; smp<=nsamples; smp++) {\
        UnsignedDivType neg_y = (ux1 - UnsignedDivType(smp*neg_dx1)) % (ux2 - UnsignedDivType(smp*neg_dx2));\
        neg_dydx = (neg_dydx > -0.00001 && neg_dydx < 0.00001) ? float(y - neg_y)/smp : neg_dydx;\
        UnsignedDivType pos_y = (ux1 + UnsignedDivType(smp*pos_dx1)) % (ux2 + UnsignedDivType(smp*pos_dx2));\
        pos_dydx = (pos_dydx > -0.00001 && pos_dydx < 0.00001) ? float(pos_y - y)/smp : pos_dydx;\
        if (DEBUG) {\
          printf("       sample %u: neg y %u x1 %u x2 %u, pos y %u x1 %u x2 %u\n", smp, neg_y,\
                UnsignedDivType(smp*neg_dx1/2.0), UnsignedDivType(smp*neg_dx2/2.0),\
                pos_y, UnsignedDivType(smp*pos_dx1/2.0), UnsignedDivType(smp*pos_dx2/2.0));\
        }\
      }\
      if (DEBUG) {\
        printf("    neg_dydx %f,  pos_dydx %f\n", neg_dydx,  neg_dydx);\
      }\
      break;\
    }\
    case SREM: { \
      if (l2) {\
        unsigned long ret_addr = (unsigned long)__builtin_return_address(0);\
        if (!gr_mode_perf) record_arg(ret_addr, 21, 0, l2, (float)x2, location);\
      }\
      nsamples = flags().samples;\
      SignedDivType y = x1 % x2;\
      f_val = y; \
      for (int smp=1; smp<=nsamples; smp++) {\
        SignedDivType neg_y = (x1 - SignedDivType(smp*neg_dx1)) % (x2 - SignedDivType(smp*neg_dx2));\
        neg_dydx = (neg_dydx > -0.00001 && neg_dydx < 0.00001) ? float(y - neg_y)/smp : neg_dydx;\
        SignedDivType pos_y = (x1 + SignedDivType(smp*pos_dx1)) % (x2 + SignedDivType(smp*pos_dx2));\
        pos_dydx = (pos_dydx > -0.00001 && pos_dydx < 0.00001) ? float(pos_y - y)/smp : pos_dydx;\
      }\
      break;\
    }\
    case SHL: { \
      nsamples = flags().samples;\
      BitwiseType y = x1 << x2;\
      f_val = y; \
      BitwiseType neg_y, pos_y;\
      for (int smp=1; smp<=nsamples; smp++) {\
        neg_y = (x1 - BitwiseType(smp*neg_dx1)) << (x2 - BitwiseType(smp*neg_dx2));\
        neg_dydx = (neg_dydx > -0.00001 && neg_dydx < 0.00001) ? float(y - neg_y)/smp : neg_dydx;\
        pos_y = (x1 + BitwiseType(smp*pos_dx1)) << (x2 + BitwiseType(smp*pos_dx2));\
        pos_dydx = (pos_dydx > -0.00001 && pos_dydx < 0.00001) ? float(pos_y - y)/smp : pos_dydx;\
      }\
      break;\
    }\
    case LSHR: { \
      nsamples = flags().samples;\
      unsigned int offset = 1;\
      UnsignedDivType y = x1 >> x2;\
      f_val = y; \
      UnsignedDivType neg_y, pos_y;\
      if (DEBUG) {\
        printf("  LShr neg_dx1 %f %u,  neg_dx2 %f %u, pos_dx1 %f %u, pos_dx2 %f %u\n",\
            neg_dx1, UnsignedDivType(neg_dx1), neg_dx2, UnsignedDivType(neg_dx2),\
            pos_dx1, UnsignedDivType(pos_dx1), pos_dx2, UnsignedDivType(pos_dx2));\
      }\
      for (int smp=1; smp<=nsamples; smp++) {\
        neg_y = (x1 - UnsignedDivType(offset*neg_dx1)) >> (x2 - UnsignedDivType(offset*neg_dx2));\
        neg_dydx = (neg_dydx > -0.00001 && neg_dydx < 0.00001) ? float(y - neg_y)/offset : neg_dydx;\
        pos_y = (x1 + UnsignedDivType(offset*pos_dx1)) >> (x2 + UnsignedDivType(offset*pos_dx2));\
        pos_dydx = (pos_dydx > -0.00001 && pos_dydx < 0.00001) ? float(pos_y - y)/offset : pos_dydx;\
        if (DEBUG) {\
          printf("    sample %u, offset %u: neg y %u x1 %u x2 %u, pos y %u x1 %u x2 %u\n", smp, offset, neg_y,\
              BitwiseType(offset*neg_dx1), BitwiseType(offset*neg_dx2),\
              pos_y, BitwiseType(offset*pos_dx1), BitwiseType(offset*pos_dx2));\
        }\
        offset = offset<<1;\
      }\
      break;\
    }\
    case ASHR: { \
      nsamples = flags().samples;\
      unsigned int offset = 1;\
      BitwiseType y = x1 >> x2;\
      f_val = y; \
      BitwiseType neg_y, pos_y;\
      for (int smp=1; smp<=nsamples; smp++) {\
        neg_y = (x1 - BitwiseType(offset*neg_dx1)) >> (x2 - BitwiseType(offset*neg_dx2));\
        neg_dydx = (neg_dydx > -0.00001 && neg_dydx < 0.00001) ? float(y - neg_y)/offset : neg_dydx;\
        pos_y = (x1 + BitwiseType(offset*pos_dx1)) >> (x2 + BitwiseType(offset*pos_dx2));\
        pos_dydx = (pos_dydx > -0.00001 && pos_dydx < 0.00001) ? float(pos_y - y)/offset : pos_dydx;\
        offset = offset<<1;\
      }\
      break;\
    }\
    case AND: { \
      nsamples = flags().samples;\
      /*unsigned int bwidth = sizeof(BitwiseType);*/\
      unsigned int offset = 1;\
      BitwiseType y = x1 & x2;\
      f_val = y; \
      BitwiseType neg_y, pos_y;\
      if (DEBUG) {\
        printf("  AND neg_dx1 %f %u,  neg_dx2 %f %u, pos_dx1 %f %u, pos_dx2 %f %u\n",\
            neg_dx1, BitwiseType(neg_dx1), neg_dx2, BitwiseType(neg_dx2),\
            pos_dx1, BitwiseType(pos_dx1), pos_dx2, BitwiseType(pos_dx2));\
      }\
      for (int smp=1; smp<=nsamples; smp++) {\
        neg_y = (x1 - BitwiseType(offset*neg_dx1)) & (x2 -BitwiseType(offset*neg_dx2));\
        neg_dydx = (neg_dydx > -0.00001 && neg_dydx < 0.00001) ? float(y - neg_y)/offset : neg_dydx;\
        pos_y = (x1 + BitwiseType(offset*pos_dx1)) & (x2 + BitwiseType(offset*pos_dx2));\
        pos_dydx = (pos_dydx > -0.00001 && pos_dydx < 0.00001) ? float(pos_y - y)/offset : pos_dydx;\
        if (DEBUG) {\
          printf("    sample %u, offset %u: neg y %u x1 %u x2 %u, pos y %u x1 %u x2 %u\n", smp, offset, neg_y,\
              BitwiseType(offset*neg_dx1), BitwiseType(offset*neg_dx2),\
              pos_y, BitwiseType(offset*pos_dx1), BitwiseType(offset*pos_dx2));\
        }\
        offset = offset<<1;\
      }\
      break;\
    }\
    case OR: { \
      nsamples = flags().samples;\
      BitwiseType y = x1 | x2;\
      f_val = y; \
      BitwiseType neg_y, pos_y;\
      for (int smp=1; smp<=nsamples; smp++) {\
        neg_y = (x1 - BitwiseType(smp*neg_dx1)) | (x2 - BitwiseType(smp*neg_dx2));\
        neg_dydx = (neg_dydx > -0.00001 && neg_dydx < 0.00001) ? float(y - neg_y)/smp : neg_dydx;\
        pos_y = (x1 + BitwiseType(smp*pos_dx1)) | (x2 + BitwiseType(smp*pos_dx2));\
        pos_dydx = (pos_dydx > -0.00001 && pos_dydx < 0.00001) ? float(pos_y - y)/smp : pos_dydx;\
      }\
      break;\
    }\
    case XOR: { \
      nsamples = flags().samples;\
      BitwiseType y = x1 ^ x2;\
      f_val = y; \
      BitwiseType neg_y, pos_y;\
      for (int smp=1; smp<=nsamples; smp++) {\
        neg_y = (x1 - BitwiseType(smp*neg_dx1)) ^ (x2 - BitwiseType(smp*neg_dx2));\
        neg_dydx = (neg_dydx > -0.00001 && neg_dydx < 0.00001) ? float(y - neg_y)/smp : neg_dydx;\
        pos_y = (x1 + BitwiseType(smp*pos_dx1)) ^ (x2 + BitwiseType(smp*pos_dx2));\
        pos_dydx = (pos_dydx > -0.00001 && pos_dydx < 0.00001) ? float(pos_y - y)/smp : pos_dydx;\
      }\
      break;\
    }\
    case GETELEMENTPTRT: /* GetElmentPtr */{ \
      if (flags().gep_default) {\
        neg_dydx = 1.0; /*nanf("unsupported");*/\
        pos_dydx = 1.0; /*nanf("unsupported");*/\
      } else {\
        neg_dydx = 0.0; /*nanf("unsupported");*/\
        pos_dydx = 0.0; /*nanf("unsupported");*/\
      }\
      break;\
    }\
    case SELECT: /* Select */{ \
      if (flags().select_default) {\
        neg_dydx = 1.0; /*nanf("unsupported");*/\
        pos_dydx = 1.0; /*nanf("unsupported");*/\
      } else {\
        neg_dydx = 0.0; /*nanf("unsupported");*/\
        pos_dydx = 0.0; /*nanf("unsupported");*/\
      }\
      break;\
    }\
    default:\
      if (flags().default_nan) {\
        neg_dydx = nanf("unsupported");\
        pos_dydx = nanf("unsupported");\
      } else {\
        neg_dydx = 0.0; /*nanf("unsupported");*/\
        pos_dydx = 0.0; /*nanf("unsupported");*/\
      }\
      supported = false;\
  }\
  if (reuse_labels) {\
    if (l1 && pos_dydx == pos_dx1 && neg_dydx == neg_dx1) {\
      return l1;\
    } else if (l2 && pos_dydx == pos_dx2 && neg_dydx == neg_dx2) {\
      return l2;\
    }\
  }\
  dfsan_label label = atomic_fetch_add(&__dfsan_last_label, 1, memory_order_relaxed) + 1; \
  dfsan_check_label(label); \
  __dfsan_label_info[label].l1 = l1;\
  __dfsan_label_info[label].l2 = l2;\
  __dfsan_label_info[label].opcode = opcode;\
  __dfsan_label_info[label].neg_dydx = neg_dydx;\
  __dfsan_label_info[label].pos_dydx = pos_dydx;\
  __dfsan_label_info[label].loc = location;\
  __dfsan_label_info[label].f_val = f_val;\
  if (DEBUG) {\
    char neg_dx1_str[32], neg_dx2_str[32];\
    char pos_dx1_str[32], pos_dx2_str[32];\
    char neg_dydx_str[32];\
    char pos_dydx_str[32];\
    float2str(neg_dydx_str, __dfsan_label_info[label].neg_dydx, 32);\
    float2str(pos_dydx_str, __dfsan_label_info[label].pos_dydx, 32);\
    float2str(neg_dx1_str, neg_dx1, 32);\
    float2str(pos_dx1_str, pos_dx1, 32);\
    float2str(neg_dx2_str, neg_dx2, 32);\
    float2str(pos_dx2_str, pos_dx2, 32);\
    printf("dfsan_int_union %d: %d %d dx %s %s x1 %d x2 %d dx1 %s %s dx2 %s %s -- %s %s insnID: %u\n", label, l1, l2,\
           neg_dydx_str, pos_dydx_str, x1, x2, neg_dx1_str, pos_dx1_str, neg_dx2_str, pos_dx2_str,\
           opName, supportedLabel(supported), insnID);\
  }\
  return label;\
}\


#define DFSAN_FLOAT_UNION(FunctionName, Type) \
extern "C" SANITIZER_INTERFACE_ATTRIBUTE \
dfsan_label FunctionName(dfsan_label l1, dfsan_label l2 , Type x1, Type x2, uptr insnID, uptr opcode, char* location) { \
  extern int gr_mode_perf; \
  bool reuse_labels = flags().reuse_labels;\
  const char* opName = opcodeNames[opcode]; \
  bool supported = true; \
  float neg_dx1 = 0, neg_dx2 = 0, pos_dx1 = 0, pos_dx2 = 0;\
  float neg_dydx = 0, pos_dydx = 0; \
  if (l1 == 0 && l2 == 0) { \
    return 0; \
  } \
  if (l1 != 0) { \
    neg_dx1 = __dfsan_label_info[l1].neg_dydx; \
    pos_dx1 = __dfsan_label_info[l1].pos_dydx; \
  } \
  if (l2 != 0) { \
    neg_dx2 = __dfsan_label_info[l2].neg_dydx; \
    pos_dx2 = __dfsan_label_info[l2].pos_dydx; \
  } \
  if (reuse_labels) {\
    if (neg_dx1 == 0 && pos_dx1 == 0 && neg_dx2 == 0 && pos_dx2 == 0) {\
      return l1 ? l1 : l2;\
    }\
  }\
  switch (opcode) { \
    case FADD: { \
      neg_dydx = neg_dx1 + neg_dx2; \
      pos_dydx = pos_dx1 + pos_dx2; \
      break; \
    }\
    case FSUB: { \
      neg_dydx = neg_dx1 - neg_dx2;\
      pos_dydx = pos_dx1 - pos_dx2;\
      break;\
    }\
    case FMUL: { \
      neg_dydx = x1 * neg_dx2 + x2 * neg_dx1;\
      pos_dydx = x1 * pos_dx2 + x2 * pos_dx1;\
      break;\
    }\
    case FDIV: { \
      if (l2) {\
        unsigned long ret_addr = (unsigned long)__builtin_return_address(0);\
        if (!gr_mode_perf) record_arg(ret_addr, 19, 0, l2, (float)x2, location);\
      }\
      if (x2 != 0.0) { \
        neg_dydx = (x2 * neg_dx1 - x1 * neg_dx2) / x2;\
        pos_dydx = (x2 * pos_dx1 - x1 * pos_dx2) / x2;\
      } else { \
        neg_dydx = nanf("div 0"); \
        pos_dydx = nanf("div 0"); \
      } \
      break; \
    }\
    case FREM: { \
      if (l2) {\
        unsigned long ret_addr = (unsigned long)__builtin_return_address(0);\
        if (!gr_mode_perf) record_arg(ret_addr, 22, 0, l2, (float)x2, location);\
      }\
      Type y = fmod(x1, x2);\
      Type neg_y = fmod((x1 - Type(neg_dx1)), (x2 - Type(neg_dx2)));\
      neg_dydx = y - neg_y;\
      Type pos_y = fmod((x1 + Type(pos_dx1)), (x2 + Type(pos_dx2)));\
      pos_dydx = pos_y - y;\
      break;\
    }\
    default: \
      if (flags().default_nan) {\
        neg_dydx = nanf("unsupported");\
        pos_dydx = nanf("unsupported");\
      } else {\
        neg_dydx = 0.0; /*nanf("unsupported");*/\
        pos_dydx = 0.0; /*nanf("unsupported");*/\
      }\
      supported = false;\
  }\
  if (reuse_labels) {\
    if (l1 && pos_dydx == pos_dx1 && neg_dydx == neg_dx1) {\
      return l1;\
    } else if (l2 && pos_dydx == pos_dx2 && neg_dydx == neg_dx2) {\
      return l2;\
    }\
  }\
  dfsan_label label = atomic_fetch_add(&__dfsan_last_label, 1, memory_order_relaxed) + 1; \
  dfsan_check_label(label); \
  __dfsan_label_info[label].l1 = l1; \
  __dfsan_label_info[label].l2 = l2; \
  __dfsan_label_info[label].opcode = opcode;\
  __dfsan_label_info[label].neg_dydx = neg_dydx;\
  __dfsan_label_info[label].pos_dydx = pos_dydx;\
  __dfsan_label_info[label].loc = location;\
  if (DEBUG) {\
    char neg_dx1_str[32], neg_dx2_str[32];\
    char pos_dx1_str[32], pos_dx2_str[32];\
    char x1_str[32], x2_str[32];\
    char neg_dydx_str[32];\
    char pos_dydx_str[32];\
    float2str(neg_dydx_str, __dfsan_label_info[label].neg_dydx, 32);\
    float2str(pos_dydx_str, __dfsan_label_info[label].pos_dydx, 32);\
    float2str(neg_dx1_str, neg_dx1, 32);\
    float2str(pos_dx1_str, pos_dx1, 32);\
    float2str(neg_dx2_str, neg_dx2, 32);\
    float2str(pos_dx2_str, pos_dx2, 32);\
    float2str(x1_str, x1, 32);\
    float2str(x2_str, x2, 32);\
    printf("dfsan_float_union %d: %d %d dx %s %s x1 %s x2 %s dx1 %s %s dx2 %s %s -- %s %s insnID: %u\n", label, l1, l2,\
           neg_dydx_str, pos_dydx_str, x1_str, x2_str, neg_dx1_str, pos_dx1_str, neg_dx2_str, pos_dx2_str,\
           opName, supportedLabel(supported), insnID);\
  }\
  return label;\
}\

#define DFSAN_INT_BRANCH(FunctionName, UType, SType, TypeName)      \
extern "C" SANITIZER_INTERFACE_ATTRIBUTE \
void FunctionName(dfsan_label lhs, dfsan_label rhs, \
                          UType lhs_v, UType rhs_v, bool cond, uint32_t pred, uint64_t file_id, uint64_t br_id, \
                          uint16_t is_ptr, const char* location) { \
  extern int gr_mode_perf; \
  if (lhs == 0 && rhs == 0) {\
    return; /* exit early if no gradient */\
  }\
  char lhs_neg_dydx[32], lhs_pos_dydx[32], rhs_neg_dydx[32], rhs_pos_dydx[32]; \
  if (!gr_mode_perf) {\
    if (lhs != 0 || rhs != 0) {\
      if (DEBUG) {\
        float2str(lhs_pos_dydx, __dfsan_label_info[lhs].pos_dydx, 32);\
        float2str(lhs_neg_dydx, __dfsan_label_info[lhs].neg_dydx, 32);\
        float2str(rhs_pos_dydx, __dfsan_label_info[rhs].pos_dydx, 32);\
        float2str(rhs_neg_dydx, __dfsan_label_info[rhs].neg_dydx, 32);\
        printf("dfsan int branch: " TypeName " %u, %u -- %u %s, %s : %u %s, %s -- %u pred: %u\n",\
               lhs, rhs, lhs_v, lhs_pos_dydx, lhs_neg_dydx, rhs_v, rhs_pos_dydx, rhs_neg_dydx, cond, pred);\
      }\
      record_branch(file_id, br_id, lhs, rhs, (float)lhs_v, (float)rhs_v, cond, is_ptr, location);\
    }\
  }\
  /* BRANCH BARRIER FUNCTIONS */\
  if (flags().branch_barriers) {\
    float lhs_neg_dx = 0, lhs_pos_dx = 0, rhs_neg_dx = 0, rhs_pos_dx = 0;\
    if (lhs) {\
      lhs_neg_dx = __dfsan_label_info[lhs].neg_dydx;\
      lhs_pos_dx = __dfsan_label_info[lhs].pos_dydx;\
    }\
    if (rhs) {\
      rhs_neg_dx = __dfsan_label_info[rhs].neg_dydx;\
      rhs_pos_dx = __dfsan_label_info[rhs].pos_dydx;\
    }\
    switch (pred) {\
      case ICMP_EQ: {  /* equal */\
        /* break; SKIP EQUALS */\
        UType lhs_sample = lhs_v - UType(lhs_neg_dx);\
        UType rhs_sample = rhs_v - UType(rhs_neg_dx);\
        if (cond != (lhs_sample == rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = lhs_v + UType(lhs_pos_dx);\
        rhs_sample = rhs_v + UType(rhs_pos_dx);\
        if (cond != (lhs_sample == rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_NE: {  /* not equal */\
        /* break; SKIP NEQ */\
        UType lhs_sample = lhs_v - UType(lhs_neg_dx);\
        UType rhs_sample = rhs_v - UType(rhs_neg_dx);\
        if (cond != (lhs_sample != rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = lhs_v + UType(lhs_pos_dx);\
        rhs_sample = rhs_v + UType(rhs_pos_dx);\
        if (cond != (lhs_sample != rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_UGT: { /* unsigned greater than */\
        UType lhs_sample = lhs_v - UType(lhs_neg_dx);\
        UType rhs_sample = rhs_v - UType(rhs_neg_dx);\
        if (cond != (lhs_sample > rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = lhs_v + UType(lhs_pos_dx);\
        rhs_sample = rhs_v + UType(rhs_pos_dx);\
        if (cond != (lhs_sample > rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_UGE: { /* unsigned greater or equal */\
        UType lhs_sample = lhs_v - UType(lhs_neg_dx);\
        UType rhs_sample = rhs_v - UType(rhs_neg_dx);\
        if (cond != (lhs_sample >= rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = lhs_v + UType(lhs_pos_dx);\
        rhs_sample = rhs_v + UType(rhs_pos_dx);\
        if (cond != (lhs_sample >= rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_ULT: { /* unsigned less than */\
        UType lhs_sample = lhs_v - UType(lhs_neg_dx);\
        UType rhs_sample = rhs_v - UType(rhs_neg_dx);\
        if (cond != (lhs_sample < rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = lhs_v + UType(lhs_pos_dx);\
        rhs_sample = rhs_v + UType(rhs_pos_dx);\
        if (cond != (lhs_sample < rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_ULE: { /* unsigned less or equal */\
        UType lhs_sample = lhs_v - UType(lhs_neg_dx);\
        UType rhs_sample = rhs_v - UType(rhs_neg_dx);\
        if (cond != (lhs_sample <= rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = lhs_v + UType(lhs_pos_dx);\
        rhs_sample = rhs_v + UType(rhs_pos_dx);\
        if (cond != (lhs_sample <= rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_SGT: { /* signed greater than */\
        SType lhs_sample = SType(lhs_v) - SType(lhs_neg_dx);\
        SType rhs_sample = SType(rhs_v) - SType(rhs_neg_dx);\
        if (cond != (lhs_sample > rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = SType(lhs_v) + SType(lhs_pos_dx);\
        rhs_sample = SType(rhs_v) + SType(rhs_pos_dx);\
        if (cond != (lhs_sample > rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_SGE: { /* signed greater or equal */\
        SType lhs_sample = SType(lhs_v) - SType(lhs_neg_dx);\
        SType rhs_sample = SType(rhs_v) - SType(rhs_neg_dx);\
        if (cond != (lhs_sample >= rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = SType(lhs_v) + SType(lhs_pos_dx);\
        rhs_sample = SType(rhs_v) + SType(rhs_pos_dx);\
        if (cond != (lhs_sample >= rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_SLT: { /* signed less than */\
        SType lhs_sample = SType(lhs_v) - SType(lhs_neg_dx);\
        SType rhs_sample = SType(rhs_v) - SType(rhs_neg_dx);\
        if (cond != (lhs_sample < rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = SType(lhs_v) + SType(lhs_pos_dx);\
        rhs_sample = SType(rhs_v) + SType(rhs_pos_dx);\
        if (cond != (lhs_sample < rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      case ICMP_SLE: { /* signed less or equal */\
        SType lhs_sample = SType(lhs_v) - SType(lhs_neg_dx);\
        SType rhs_sample = SType(rhs_v) - SType(rhs_neg_dx);\
        if (cond != (lhs_sample <= rhs_sample)) {\
          lhs_neg_dx = 0;\
          rhs_neg_dx = 0;\
        }\
        lhs_sample = SType(lhs_v) + SType(lhs_pos_dx);\
        rhs_sample = SType(rhs_v) + SType(rhs_pos_dx);\
        if (cond != (lhs_sample <= rhs_sample)) {\
          lhs_pos_dx = 0;\
          rhs_pos_dx = 0;\
        }\
        break;\
      }\
      default: {\
        printf("ERROR: invalid branch cmp predicate %u\n", pred);\
        exit(1);\
        break;\
      }\
    }\
    __dfsan_label_info[lhs].neg_dydx = lhs_neg_dx;\
    __dfsan_label_info[lhs].pos_dydx = lhs_pos_dx;\
    __dfsan_label_info[lhs].loc = location;\
    __dfsan_label_info[rhs].neg_dydx = rhs_neg_dx;\
    __dfsan_label_info[rhs].pos_dydx = rhs_pos_dx;\
    __dfsan_label_info[rhs].loc = location;\
  }\
}\

#define DFSAN_FLOAT_BRANCH(FunctionName, Type, TypeName, HelperFuncName)      \
extern "C" SANITIZER_INTERFACE_ATTRIBUTE \
void FunctionName(dfsan_label lhs, dfsan_label rhs, \
                          Type lhs_v, Type rhs_v, bool cond, uint32_t pred,\
                          uint64_t file_id, uint16_t br_id, \
                          uint16_t is_ptr, const char* location) { \
  extern int gr_mode_perf; \
  char lhs_neg_dydx[32], lhs_pos_dydx[32], rhs_neg_dydx[32], rhs_pos_dydx[32], lhs_str[32], rhs_str[32];\
  if (!gr_mode_perf) {\
    if (lhs != 0 || rhs != 0) {\
      if (true) {\
        float2str(lhs_pos_dydx, __dfsan_label_info[lhs].pos_dydx, 32);\
        float2str(lhs_neg_dydx, __dfsan_label_info[lhs].neg_dydx, 32);\
        float2str(rhs_pos_dydx, __dfsan_label_info[rhs].pos_dydx, 32);\
        float2str(rhs_neg_dydx, __dfsan_label_info[rhs].neg_dydx, 32);\
        HelperFuncName(lhs_str, lhs_v, 32);\
        HelperFuncName(rhs_str, rhs_v, 32);\
        printf("dfsan float branch: " TypeName " %u, %u -- %s %s, %s : %s %s, %s -- %u pred: %u %u\n",\
                lhs, rhs, lhs_str, lhs_pos_dydx, lhs_neg_dydx, rhs_str, rhs_pos_dydx, rhs_neg_dydx, cond, pred, is_ptr);\
      }\
      record_branch(file_id, br_id, lhs, rhs, (float)lhs_v, (float)rhs_v, cond, is_ptr, location);\
    }\
  }\
}\

