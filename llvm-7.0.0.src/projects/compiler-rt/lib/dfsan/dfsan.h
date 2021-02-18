//===-- dfsan.h -------------------------------------------------*- C++ -*-===//
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
// Private DFSan header.
//===----------------------------------------------------------------------===//

#ifndef DFSAN_H
#define DFSAN_H

#include "sanitizer_common/sanitizer_internal_defs.h"
#include "dfsan_platform.h"

using __sanitizer::uptr;
using __sanitizer::u16;

// Copy declarations from public sanitizer/dfsan_interface.h header here.
typedef u16 dfsan_label;

struct dfsan_label_info {
  dfsan_label l1;
  dfsan_label l2;
  const char *loc;
  float neg_dydx;
  float pos_dydx;
  dfsan_label opcode;
  int f_val;
};

struct branch_record {
  unsigned long file_id;
  unsigned long inst_id;
  dfsan_label lhs_label;
  dfsan_label rhs_label;
  float lhs_v;
  float rhs_v;
  float lhs_ndx;
  float lhs_pdx;
  float rhs_ndx;
  float rhs_pdx;
  bool cond;
  unsigned int is_ptr;
  const char* loc;
};

struct func_arg_record {
  unsigned long file_id;
  unsigned int inst_id;
  unsigned int arg_ind;
  dfsan_label label;
  float v;
  float ndx;
  float pdx;
  const char* loc;
};

extern int gr_mode_perf;

void record_arg(unsigned long file_id, unsigned int inst_id, unsigned int arg_ind, dfsan_label label, float v,
        const char* location);


extern "C" {
void *dfsan_memcpy(void *dest, const void *src, unsigned long n);
void dfsan_add_label(dfsan_label label, void *addr, uptr size);
void dfsan_set_label(dfsan_label label, void *addr, uptr size);
dfsan_label dfsan_read_label(const void *addr, uptr size);
dfsan_label dfsan_union(dfsan_label l1, dfsan_label l2);
dfsan_label dfsan_create_label(const char *desc);
}  // extern "C"
extern int gr_mode_perf;

template <typename T>
void dfsan_set_label(dfsan_label label, T &data) {  // NOLINT
  dfsan_set_label(label, (void *)&data, sizeof(T));
}

namespace __dfsan {

void InitializeInterceptors();

inline dfsan_label *shadow_for(void *ptr) {
  return (dfsan_label *) ((((uptr) ptr) & ShadowMask()) << 1);
}

inline const dfsan_label *shadow_for(const void *ptr) {
  return shadow_for(const_cast<void *>(ptr));
}

struct Flags {
#define DFSAN_FLAG(Type, Name, DefaultValue, Description) Type Name;
#include "dfsan_flags.inc"
#undef DFSAN_FLAG

  void SetDefaults();
};

extern Flags flags_data;
inline Flags &flags() {
  return flags_data;
}

}  // namespace __dfsan

#endif  // DFSAN_H
