//===-- dfsan_flags.inc -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// DFSan runtime flags.
//
//===----------------------------------------------------------------------===//
#ifndef DFSAN_FLAG
# error "Define DFSAN_FLAG prior to including this file!"
#endif

// DFSAN_FLAG(Type, Name, DefaultValue, Description)
// See COMMON_FLAG in sanitizer_flags.inc for more details.

DFSAN_FLAG(bool, warn_unimplemented, true,
           "Whether to warn on unimplemented functions.")
DFSAN_FLAG(bool, warn_nonzero_labels, false,
           "Whether to warn on unimplemented functions.")
DFSAN_FLAG(
    bool, strict_data_dependencies, true,
    "Whether to propagate labels only when there is an obvious data dependency"
    "(e.g., when comparing strings, ignore the fact that the output of the"
    "comparison might be data-dependent on the content of the strings). This"
    "applies only to the custom functions defined in 'custom.c'.")

DFSAN_FLAG(const char *, gradient_logfile, "gradient.csv", 
                "Log file for gradient (recorded as csv).")

DFSAN_FLAG(const char *, branch_logfile, "", 
                "Log file for branch gradients (recorded as csv).")

DFSAN_FLAG(const char *, func_logfile, "",
                "Log file for function gradients (recorded as csv).")

DFSAN_FLAG(bool, reuse_labels, true, 
             "Optimization to reuse labels when gradient does not change")

DFSAN_FLAG(int, samples, 5,
            "Samples to use with most ops.")

DFSAN_FLAG(bool, branch_barriers, 1, 
        "Use barrier functions with branches.")

DFSAN_FLAG(int, gep_default, 1,
        "Default value for GEPs with nonzero grad inputs")

DFSAN_FLAG(int, select_default, 1,
        "Default value for Selects with nonzero grad inputs")

DFSAN_FLAG(bool, default_nan, false, "Default to nan for unsupported ops.")

/* ENV VAR:
 *    GRSAN_DISABLE_LOGGING 
 *    ENV VAR that disables branch/function logging
 *    defaults to false
 *    set to 1 to reduce overhead
 */
