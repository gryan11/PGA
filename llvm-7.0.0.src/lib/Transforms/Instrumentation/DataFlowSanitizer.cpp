//===- DataFlowSanitizer.cpp - dynamic data flow analysis -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This file is a part of DataFlowSanitizer, a generalised dynamic data flow
/// analysis.
///
/// Unlike other Sanitizer tools, this tool is not designed to detect a specific
/// class of bugs on its own.  Instead, it provides a generic dynamic data flow
/// analysis framework to be used by clients to help detect application-specific
/// issues within their own code.
///
/// The analysis is based on automatic propagation of data flow labels (also
/// known as taint labels) through a program as it performs computation.  Each
/// byte of application memory is backed by two bytes of shadow memory which
/// hold the label.  On Linux/x86_64, memory is laid out as follows:
///
/// +--------------------+ 0x800000000000 (top of memory)
/// | application memory |
/// +--------------------+ 0x700000008000 (kAppAddr)
/// |                    |
/// |       unused       |
/// |                    |
/// +--------------------+ 0x200200000000 (kUnusedAddr)
/// |    union table     |
/// +--------------------+ 0x200000000000 (kUnionTableAddr)
/// |   shadow memory    |
/// +--------------------+ 0x000000010000 (kShadowAddr)
/// | reserved by kernel |
/// +--------------------+ 0x000000000000
///
/// To derive a shadow memory address from an application memory address,
/// bits 44-46 are cleared to bring the address into the range
/// [0x000000008000,0x100000000000).  Then the address is shifted left by 1 to
/// account for the double byte representation of shadow labels and move the
/// address into the shadow memory range.  See the function
/// DataFlowSanitizer::getShadowAddress below.
///
/// For more information, please refer to the design document:
/// http://clang.llvm.org/docs/DataFlowSanitizerDesign.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/SpecialCaseList.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
//#include "Annotation.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <atomic>
#include <functional>

using namespace llvm;

#define DFSAN_GRAD_NAMESPACE	"GRAD"

// External symbol to be used when generating the shadow address for
// architectures with multiple VMAs. Instead of using a constant integer
// the runtime will set the external mask based on the VMA range.
static const char *const kDFSanExternShadowPtrMask = "__dfsan_shadow_ptr_mask";

// The -dfsan-preserve-alignment flag controls whether this pass assumes that
// alignment requirements provided by the input IR are correct.  For example,
// if the input IR contains a load with alignment 8, this flag will cause
// the shadow load to have alignment 16.  This flag is disabled by default as
// we have unfortunately encountered too much code (including Clang itself;
// see PR14291) which performs misaligned access.
static cl::opt<bool> ClPreserveAlignment(
    "dfsan-preserve-alignment",
    cl::desc("respect alignment requirements provided by input IR"), cl::Hidden,
    cl::init(false));

// The ABI list files control how shadow parameters are passed. The pass treats
// every function labelled "uninstrumented" in the ABI list file as conforming
// to the "native" (i.e. unsanitized) ABI.  Unless the ABI list contains
// additional annotations for those functions, a call to one of those functions
// will produce a warning message, as the labelling behaviour of the function is
// unknown.  The other supported annotations are "functional" and "discard",
// which are described below under DataFlowSanitizer::WrapperKind.
static cl::list<std::string> ClABIListFiles(
    "dfsan-abilist",
    cl::desc("File listing native ABI functions and how the pass treats them"),
    cl::Hidden);

// Controls whether the pass uses IA_Args or IA_TLS as the ABI for instrumented
// functions (see DataFlowSanitizer::InstrumentedABI below).
static cl::opt<bool> ClArgsABI(
    "dfsan-args-abi",
    cl::desc("Use the argument ABI rather than the TLS ABI"),
    cl::Hidden);

// Controls whether the pass includes or ignores the labels of pointers in load
// instructions.
static cl::opt<bool> ClCombinePointerLabelsOnLoad(
    "dfsan-combine-pointer-labels-on-load",
    cl::desc("Combine the label of the pointer with the label of the data when "
             "loading from memory."),
    cl::Hidden, cl::init(true));

// Controls whether the pass includes or ignores the labels of pointers in
// stores instructions.
static cl::opt<bool> ClCombinePointerLabelsOnStore(
    "dfsan-combine-pointer-labels-on-store",
    cl::desc("Combine the label of the pointer with the label of the data when "
             "storing in memory."),
    cl::Hidden, cl::init(false));

static cl::opt<bool> ClDebugNonzeroLabels(
    "dfsan-debug-nonzero-labels",
    cl::desc("Insert calls to __dfsan_nonzero_label on observing a parameter, "
             "load or return with a nonzero label"),
    cl::Hidden);

static StringRef GetGlobalTypeString(const GlobalValue &G) {
  // Types of GlobalVariables are always pointer types.
  Type *GType = G.getValueType();
  // For now we support blacklisting struct types only.
  if (StructType *SGType = dyn_cast<StructType>(GType)) {
    if (!SGType->isLiteral())
      return SGType->getName();
  }
  return "<unknown type>";
}

namespace {

class DFSanABIList {
  std::unique_ptr<SpecialCaseList> SCL;

 public:
  DFSanABIList() = default;

  void set(std::unique_ptr<SpecialCaseList> List) { SCL = std::move(List); }

  /// Returns whether either this function or its source file are listed in the
  /// given category.
  bool isIn(const Function &F, StringRef Category) const {
    return isIn(*F.getParent(), Category) ||
           SCL->inSection("dataflow", "fun", F.getName(), Category);
  }

  /// Returns whether this global alias is listed in the given category.
  ///
  /// If GA aliases a function, the alias's name is matched as a function name
  /// would be.  Similarly, aliases of globals are matched like globals.
  bool isIn(const GlobalAlias &GA, StringRef Category) const {
    if (isIn(*GA.getParent(), Category))
      return true;

    if (isa<FunctionType>(GA.getValueType()))
      return SCL->inSection("dataflow", "fun", GA.getName(), Category);

    return SCL->inSection("dataflow", "global", GA.getName(), Category) ||
           SCL->inSection("dataflow", "type", GetGlobalTypeString(GA),
                          Category);
  }

  /// Returns whether this module is listed in the given category.
  bool isIn(const Module &M, StringRef Category) const {
    return SCL->inSection("dataflow", "src", M.getModuleIdentifier(), Category);
  }
};

/// TransformedFunction is used to express the result of transforming one
/// function type into another.  This struct is immutable.  It holds metadata
/// useful for updating calls of the old function to the new type.
struct TransformedFunction {
  TransformedFunction(FunctionType* OriginalType,
                      FunctionType* TransformedType,
                      std::vector<unsigned> ArgumentIndexMapping)
      : OriginalType(OriginalType),
        TransformedType(TransformedType),
        ArgumentIndexMapping(ArgumentIndexMapping) {}

  // Disallow copies.
  TransformedFunction(const TransformedFunction&) = delete;
  TransformedFunction& operator=(const TransformedFunction&) = delete;

  // Allow moves.
  TransformedFunction(TransformedFunction&&) = default;
  TransformedFunction& operator=(TransformedFunction&&) = default;

  /// Type of the function before the transformation.
  FunctionType* const OriginalType;

  /// Type of the function after the transformation.
  FunctionType* const TransformedType;

  /// Transforming a function may change the position of arguments.  This
  /// member records the mapping from each argument's old position to its new
  /// position.  Argument positions are zero-indexed.  If the transformation
  /// from F to F' made the first argument of F into the third argument of F',
  /// then ArgumentIndexMapping[0] will equal 2.
  const std::vector<unsigned> ArgumentIndexMapping;
};

/// Given function attributes from a call site for the original function,
/// return function attributes appropriate for a call to the transformed
/// function.
AttributeList TransformFunctionAttributes(
    const TransformedFunction& TransformedFunction,
    LLVMContext& Ctx, AttributeList CallSiteAttrs) {

  // Construct a vector of AttributeSet for each function argument.
  std::vector<llvm::AttributeSet> ArgumentAttributes(
      TransformedFunction.TransformedType->getNumParams());

  // Copy attributes from the parameter of the original function to the
  // transformed version.  'ArgumentIndexMapping' holds the mapping from
  // old argument position to new.
  for (unsigned i=0, ie = TransformedFunction.ArgumentIndexMapping.size();
       i < ie; ++i) {
    unsigned TransformedIndex = TransformedFunction.ArgumentIndexMapping[i];
    ArgumentAttributes[TransformedIndex] = CallSiteAttrs.getParamAttributes(i);
  }

  // Copy annotations on varargs arguments.
  for (unsigned i = TransformedFunction.OriginalType->getNumParams(),
       ie = CallSiteAttrs.getNumAttrSets(); i<ie; ++i) {
    ArgumentAttributes.push_back(CallSiteAttrs.getParamAttributes(i));
  }

  return AttributeList::get(
      Ctx,
      CallSiteAttrs.getFnAttributes(),
      CallSiteAttrs.getRetAttributes(),
      llvm::makeArrayRef(ArgumentAttributes));
}

class DataFlowSanitizer : public ModulePass {
  friend struct DFSanFunction;
  friend class DFSanVisitor;

  enum {
    ShadowWidth = 16
  };

  /// Which ABI should be used for instrumented functions?
  enum InstrumentedABI {
    /// Argument and return value labels are passed through additional
    /// arguments and by modifying the return type.
    IA_Args,

    /// Argument and return value labels are passed through TLS variables
    /// __dfsan_arg_tls and __dfsan_retval_tls.
    IA_TLS
  };

  /// How should calls to uninstrumented functions be handled?
  enum WrapperKind {
    /// This function is present in an uninstrumented form but we don't know
    /// how it should be handled.  Print a warning and call the function anyway.
    /// Don't label the return value.
    WK_Warning,

    /// This function does not write to (user-accessible) memory, and its return
    /// value is unlabelled.
    WK_Discard,

    /// This function does not write to (user-accessible) memory, and the label
    /// of its return value is the union of the label of its arguments.
    WK_Functional,

    /// Instead of calling the function, a custom wrapper __dfsw_F is called,
    /// where F is the name of the function.  This function may wrap the
    /// original function or provide its own implementation.  This is similar to
    /// the IA_Args ABI, except that IA_Args uses a struct return type to
    /// pass the return value shadow in a register, while WK_Custom uses an
    /// extra pointer argument to return the shadow.  This allows the wrapped
    /// form of the function type to be expressed in C.
    WK_Custom
  };

  Module *Mod;
  LLVMContext *Ctx;
  PointerType *CharPtrTy;
  IntegerType *OpCodeTy;
  IntegerType *ShadowTy;
  IntegerType *InstIdTy;
  IntegerType *Int8Ty;
  IntegerType *Int16Ty;
  IntegerType *Int32Ty;
  IntegerType *Int64Ty;
  IntegerType *SizeTy;
  PointerType *ShadowPtrTy;
  IntegerType *IntptrTy;
  ConstantInt *ZeroShadow;
  ConstantInt *ShadowPtrMask;
  ConstantInt *ShadowPtrMul;
  PointerType *VoidPtrTy;
  Constant *ArgTLS;
  Constant *RetvalTLS;
  void *(*GetArgTLSPtr)();
  void *(*GetRetvalTLSPtr)();
  Constant *GetArgTLS;
  Constant *GetRetvalTLS;
  Constant *ExternalShadowMask;
    FunctionType *MemCpyFnTy;
    FunctionType *BasicBlockFnTy;
  FunctionType *BranchVisitorCharFnTy;
  FunctionType *BranchVisitorShortFnTy;
  FunctionType *BranchVisitorIntFnTy;
  FunctionType *BranchVisitorLongFnTy;
  FunctionType *BranchVisitorLongLongFnTy;
  FunctionType *BranchVisitorFloatFnTy;
  FunctionType *BranchVisitorDoubleFnTy;
  FunctionType *DFSanUnionUnSupFnTy;
  FunctionType *DFSanUnionUnSupFnDerivTy;
  FunctionType *DFSanUnionFnDerivTy;
  FunctionType *DFSanUnionFnDerivLongTy;
  FunctionType *DFSanUnionFnDerivByteTy;
  FunctionType *DFSanUnionFnDerivShortTy;
  FunctionType *DFSanUnionFnDerivFloatTy;
  FunctionType *DFSanUnionFnDerivDoubleTy;
  FunctionType *DFSanUnionLoadFnTy;
  FunctionType *DFSanUnimplementedFnTy;
  FunctionType *DFSanSetLabelFnTy;
  FunctionType *DFSanNonzeroLabelFnTy;
  FunctionType *DFSanVarargWrapperFnTy;
    Constant *MemCpyFn;
    Constant *BasicBlockFn;
  Constant *BranchVisitorCharFn;
  Constant *BranchVisitorShortFn;
  Constant *BranchVisitorIntFn;
  Constant *BranchVisitorLongFn;
  Constant *BranchVisitorLongLongFn;
  Constant *BranchVisitorFloatFn;
  Constant *BranchVisitorDoubleFn;
  Constant *DFSanUnionUnSupFn;
  Constant *DFSanUnionFn;
  Constant *DFSanUnionLongFn;
  Constant *DFSanUnionByteFn;
  Constant *DFSanUnionShortFn;
  Constant *DFSanUnionFloatFn;
  Constant *DFSanUnionDoubleFn;
  Constant *DFSanUnionLoadFn;
  Constant *DFSanUnimplementedFn;
  Constant *DFSanSetLabelFn;
  Constant *DFSanNonzeroLabelFn;
  Constant *DFSanVarargWrapperFn;
  MDNode *ColdCallWeights;
  DFSanABIList ABIList;
  DenseMap<Value *, Function *> UnwrappedFnMap;
  AttrBuilder ReadOnlyNoneAttrs;
  bool DFSanRuntimeShadowMask = false;

  Value *getShadowAddress(Value *Addr, Instruction *Pos);
  bool isInstrumented(const Function *F);
  bool isInstrumented(const GlobalAlias *GA);
  FunctionType *getArgsFunctionType(FunctionType *T);
  FunctionType *getTrampolineFunctionType(FunctionType *T);
  TransformedFunction getCustomFunctionType(FunctionType *T);
  InstrumentedABI getInstrumentedABI();
  WrapperKind getWrapperKind(Function *F);
  void addGlobalNamePrefix(GlobalValue *GV);
  Function *buildWrapperFunction(Function *F, StringRef NewFName,
                                 GlobalValue::LinkageTypes NewFLink,
                                 FunctionType *NewFT);
  Constant *getOrBuildTrampolineFunction(FunctionType *FT, StringRef FName);

    std::atomic<uint64_t> branch_id;
    std::atomic<uint64_t> bb_id;

public:
  static char ID;

  DataFlowSanitizer(
      const std::vector<std::string> &ABIListFiles = std::vector<std::string>(),
      void *(*getArgTLS)() = nullptr, void *(*getRetValTLS)() = nullptr);

  bool doInitialization(Module &M) override;
  bool runOnModule(Module &M) override;
};

struct DFSanFunction {
  DataFlowSanitizer &DFS;
  Function *F;
  DominatorTree DT;
  DataFlowSanitizer::InstrumentedABI IA;
  bool IsNativeABI;
  Value *ArgTLSPtr = nullptr;
  Value *RetvalTLSPtr = nullptr;
  AllocaInst *LabelReturnAlloca = nullptr;
  DenseMap<Value *, Value *> ValShadowMap;
  DenseMap<AllocaInst *, AllocaInst *> AllocaShadowMap;
  std::vector<std::pair<PHINode *, PHINode *>> PHIFixups;
  DenseSet<Instruction *> SkipInsts;
  std::vector<Value *> NonZeroChecks;
  bool AvoidNewBlocks;

  struct CachedCombinedShadow {
    BasicBlock *Block;
    Value *Shadow;
  };
  DenseMap<std::pair<Value *, Value *>, CachedCombinedShadow>
      CachedCombinedShadows;
  DenseMap<Value *, std::set<Value *>> ShadowElements;

  DFSanFunction(DataFlowSanitizer &DFS, Function *F, bool IsNativeABI)
      : DFS(DFS), F(F), IA(DFS.getInstrumentedABI()), IsNativeABI(IsNativeABI) {
    DT.recalculate(*F);
    AvoidNewBlocks = true;
  }

  void memCpy(MemTransferInst &I, Value* src, Value* dst, Value* n,
                             Value* srcShadow, Value* dstShadow, Value* nShadow);
  void recordBasicBlock(BasicBlock* BB);
  void recordBranchInst(BranchInst &I, Value* lhs_shadow,
          Value* rhs_shadow, Value* lhs, Value* rhs, unsigned int pred,
          std::string location);
  Value *getArgTLSPtr();
  Value *getArgTLS(unsigned Index, Instruction *Pos);
  Value *getRetvalTLS();
  Value *getShadow(Value *V);
  void setShadow(Instruction *I, Value *Shadow);
  Value *combineDerivShadows(Value *V1, Value *V2, Instruction *Pos, Value *UV1, Value *UV2);
  Value *combineShadows(Value *V1, Value *V2, Instruction *Pos);
  Value *combineOperandShadows(Instruction *Inst);
  Value *loadShadow(Value *ShadowAddr, uint64_t Size, uint64_t Align,
                    Instruction *Pos);
  void storeShadow(Value *Addr, uint64_t Size, uint64_t Align, Value *Shadow,
                   Instruction *Pos);
};

class DFSanVisitor : public InstVisitor<DFSanVisitor> {
public:
  DFSanFunction &DFSF;

  DFSanVisitor(DFSanFunction &DFSF) : DFSF(DFSF) {}

  const DataLayout &getDataLayout() const {
    return DFSF.F->getParent()->getDataLayout();
  }

  void visitOperandShadowInst(Instruction &I);
  void visitBinaryOperator(BinaryOperator &BO);
  void visitCastInst(CastInst &CI);
  void visitCmpInst(CmpInst &CI);
  void visitGetElementPtrInst(GetElementPtrInst &GEPI);
  void visitLoadInst(LoadInst &LI);
  void visitStoreInst(StoreInst &SI);
  void visitReturnInst(ReturnInst &RI);
  void visitCallSite(CallSite CS);
  void visitPHINode(PHINode &PN);
  void visitExtractElementInst(ExtractElementInst &I);
  void visitInsertElementInst(InsertElementInst &I);
  void visitShuffleVectorInst(ShuffleVectorInst &I);
  void visitExtractValueInst(ExtractValueInst &I);
  void visitInsertValueInst(InsertValueInst &I);
  void visitAllocaInst(AllocaInst &I);
  void visitSelectInst(SelectInst &I);
  void visitMemSetInst(MemSetInst &I);
  void visitMemTransferInst(MemTransferInst &I);

  void visitBranchInst(BranchInst &I);
  void visitSwitchInst(SwitchInst &I);
};

} // end anonymous namespace

char DataFlowSanitizer::ID;

INITIALIZE_PASS(DataFlowSanitizer, "dfsan",
                "DataFlowSanitizer: dynamic data flow analysis.", false, false)

ModulePass *
llvm::createDataFlowSanitizerPass(const std::vector<std::string> &ABIListFiles,
                                  void *(*getArgTLS)(),
                                  void *(*getRetValTLS)()) {
  return new DataFlowSanitizer(ABIListFiles, getArgTLS, getRetValTLS);
}

DataFlowSanitizer::DataFlowSanitizer(
    const std::vector<std::string> &ABIListFiles, void *(*getArgTLS)(),
    void *(*getRetValTLS)())
    : ModulePass(ID), GetArgTLSPtr(getArgTLS), GetRetvalTLSPtr(getRetValTLS) {
  std::vector<std::string> AllABIListFiles(std::move(ABIListFiles));
  AllABIListFiles.insert(AllABIListFiles.end(), ClABIListFiles.begin(),
                         ClABIListFiles.end());
  ABIList.set(SpecialCaseList::createOrDie(AllABIListFiles));
}

FunctionType *DataFlowSanitizer::getArgsFunctionType(FunctionType *T) {
  SmallVector<Type *, 4> ArgTypes(T->param_begin(), T->param_end());
  ArgTypes.append(T->getNumParams(), ShadowTy);
  if (T->isVarArg())
    ArgTypes.push_back(ShadowPtrTy);
  Type *RetType = T->getReturnType();
  if (!RetType->isVoidTy())
    RetType = StructType::get(RetType, ShadowTy);
  return FunctionType::get(RetType, ArgTypes, T->isVarArg());
}

FunctionType *DataFlowSanitizer::getTrampolineFunctionType(FunctionType *T) {
  assert(!T->isVarArg());
  SmallVector<Type *, 4> ArgTypes;
  ArgTypes.push_back(T->getPointerTo());
  ArgTypes.append(T->param_begin(), T->param_end());
  ArgTypes.append(T->getNumParams(), ShadowTy);
  Type *RetType = T->getReturnType();
  if (!RetType->isVoidTy())
    ArgTypes.push_back(ShadowPtrTy);
  return FunctionType::get(T->getReturnType(), ArgTypes, false);
}

TransformedFunction DataFlowSanitizer::getCustomFunctionType(FunctionType *T) {
  SmallVector<Type *, 4> ArgTypes;

  // Some parameters of the custom function being constructed are
  // parameters of T.  Record the mapping from parameters of T to
  // parameters of the custom function, so that parameter attributes
  // at call sites can be updated.
  std::vector<unsigned> ArgumentIndexMapping;
  for (unsigned i = 0, ie = T->getNumParams(); i != ie; ++i) {
    Type* param_type = T->getParamType(i);
    FunctionType *FT;
    if (isa<PointerType>(param_type) && (FT = dyn_cast<FunctionType>(
            cast<PointerType>(param_type)->getElementType()))) {
      ArgumentIndexMapping.push_back(ArgTypes.size());
      ArgTypes.push_back(getTrampolineFunctionType(FT)->getPointerTo());
      ArgTypes.push_back(Type::getInt8PtrTy(*Ctx));
    } else {
      ArgumentIndexMapping.push_back(ArgTypes.size());
      ArgTypes.push_back(param_type);
    }
  }
  for (unsigned i = 0, e = T->getNumParams(); i != e; ++i)
    ArgTypes.push_back(ShadowTy);
  if (T->isVarArg())
    ArgTypes.push_back(ShadowPtrTy);
  Type *RetType = T->getReturnType();
  if (!RetType->isVoidTy())
    ArgTypes.push_back(ShadowPtrTy);
  return TransformedFunction(
      T, FunctionType::get(T->getReturnType(), ArgTypes, T->isVarArg()),
      ArgumentIndexMapping);
}

bool DataFlowSanitizer::doInitialization(Module &M) {

  Triple TargetTriple(M.getTargetTriple());
  bool IsX86_64 = TargetTriple.getArch() == Triple::x86_64;
  bool IsMIPS64 = TargetTriple.isMIPS64();
  bool IsAArch64 = TargetTriple.getArch() == Triple::aarch64 ||
                   TargetTriple.getArch() == Triple::aarch64_be;

  const DataLayout &DL = M.getDataLayout();

  Mod = &M;
  Ctx = &M.getContext();
  CharPtrTy = Type::getInt8PtrTy(*Ctx);
  OpCodeTy = IntegerType::get(*Ctx, 16);
  InstIdTy = IntegerType::get(*Ctx, 16);
  Int8Ty = IntegerType::get(*Ctx, 8);
  Int16Ty = IntegerType::get(*Ctx, 16);
  Int32Ty = IntegerType::get(*Ctx, 32);
  Int64Ty = IntegerType::get(*Ctx, 64);
  SizeTy = IntegerType::get(*Ctx, 64);
  ShadowTy = IntegerType::get(*Ctx, ShadowWidth);
  ShadowPtrTy = PointerType::getUnqual(ShadowTy);
  IntptrTy = DL.getIntPtrType(*Ctx);
  ZeroShadow = ConstantInt::getSigned(ShadowTy, 0);
  ShadowPtrMul = ConstantInt::getSigned(IntptrTy, ShadowWidth / 8);
  VoidPtrTy = PointerType::getUnqual(IntegerType::get(*Ctx, 8));
  if (IsX86_64)
    ShadowPtrMask = ConstantInt::getSigned(IntptrTy, ~0x700000000000LL);
  else if (IsMIPS64)
    ShadowPtrMask = ConstantInt::getSigned(IntptrTy, ~0xF000000000LL);
  // AArch64 supports multiple VMAs and the shadow mask is set at runtime.
  else if (IsAArch64)
    DFSanRuntimeShadowMask = true;
  else
    report_fatal_error("unsupported triple");

  MemCpyFnTy =
          FunctionType::get(Type::getVoidTy(*Ctx),
                            { PointerType::getUnqual(IntegerType::get(*Ctx, 8)),
                              PointerType::getUnqual(IntegerType::get(*Ctx, 8)),
                              IntegerType::get(*Ctx, 64),
                              ShadowTy, ShadowTy, ShadowTy, CharPtrTy}, /*isVarArg=*/ false);

    BasicBlockFnTy =
            FunctionType::get(Type::getVoidTy(*Ctx),
                              { SizeTy, SizeTy }, /*isVarArg=*/ false);

  BranchVisitorCharFnTy =
          FunctionType::get(Type::getVoidTy(*Ctx),
                  { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 8), IntegerType::get(*Ctx, 8),
                                                     IntegerType::get(*Ctx, 1),
                                                     Int32Ty, SizeTy, SizeTy, InstIdTy, CharPtrTy},
                                                     /*isVarArg=*/ false);

  BranchVisitorShortFnTy =
          FunctionType::get(Type::getVoidTy(*Ctx),
                  { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 16), IntegerType::get(*Ctx, 16),
                                                     IntegerType::get(*Ctx, 1), Int32Ty, SizeTy, SizeTy, InstIdTy, CharPtrTy},
                                                     /*isVarArg=*/ false);

  BranchVisitorIntFnTy =
    FunctionType::get(Type::getVoidTy(*Ctx), { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 32), IntegerType::get(*Ctx, 32),
                                               IntegerType::get(*Ctx, 1), Int32Ty, SizeTy, SizeTy, InstIdTy, CharPtrTy}, /*isVarArg=*/ false);

  BranchVisitorLongFnTy =
          FunctionType::get(Type::getVoidTy(*Ctx), { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 64), IntegerType::get(*Ctx, 64),
                                                     IntegerType::get(*Ctx, 1), Int32Ty, SizeTy, SizeTy, InstIdTy, CharPtrTy}, /*isVarArg=*/ false);

  BranchVisitorLongLongFnTy =
          FunctionType::get(Type::getVoidTy(*Ctx), { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 128), IntegerType::get(*Ctx, 128),
                                                     IntegerType::get(*Ctx, 1), Int32Ty, SizeTy, SizeTy, InstIdTy, CharPtrTy}, /*isVarArg=*/ false);



  BranchVisitorFloatFnTy =
          FunctionType::get(Type::getVoidTy(*Ctx), { ShadowTy, ShadowTy, Type::getFloatTy(*Ctx), Type::getFloatTy(*Ctx),
                                                     IntegerType::get(*Ctx, 1), Int32Ty, SizeTy, SizeTy, InstIdTy, CharPtrTy}, /*isVarArg=*/ false);

  BranchVisitorDoubleFnTy =
          FunctionType::get(Type::getVoidTy(*Ctx), { ShadowTy, ShadowTy, Type::getDoubleTy(*Ctx), Type::getDoubleTy(*Ctx),
                                                     IntegerType::get(*Ctx, 1), Int32Ty, SizeTy, SizeTy, InstIdTy, CharPtrTy}, /*isVarArg=*/ false);

  Type *DFSanUnionUnSupDerivArgs[5] = { ShadowTy, ShadowTy, IntptrTy, OpCodeTy, CharPtrTy};
  DFSanUnionUnSupFnDerivTy =
          FunctionType::get(ShadowTy, DFSanUnionUnSupDerivArgs, /*isVarArg=*/ false);

  Type *DFSanUnionDerivArgs[7] = { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 32), IntegerType::get(*Ctx, 32), IntptrTy, OpCodeTy, CharPtrTy};
  DFSanUnionFnDerivTy =
      FunctionType::get(ShadowTy, DFSanUnionDerivArgs, /*isVarArg=*/ false);

  Type *DFSanUnionDerivLongArgs[7] = { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 64), IntegerType::get(*Ctx, 64), IntptrTy, OpCodeTy, CharPtrTy};
  DFSanUnionFnDerivLongTy =
      FunctionType::get(ShadowTy, DFSanUnionDerivLongArgs, /*isVarArg=*/ false);

  Type *DFSanUnionDerivByteArgs[7] = { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 8), IntegerType::get(*Ctx, 8), IntptrTy, OpCodeTy, CharPtrTy};
  DFSanUnionFnDerivByteTy =
      FunctionType::get(ShadowTy, DFSanUnionDerivByteArgs, /*isVarArg=*/ false);

  Type *DFSanUnionDerivShortArgs[7] = { ShadowTy, ShadowTy, IntegerType::get(*Ctx, 16), IntegerType::get(*Ctx, 16), IntptrTy, OpCodeTy, CharPtrTy};
  DFSanUnionFnDerivShortTy =
          FunctionType::get(ShadowTy, DFSanUnionDerivShortArgs, /*isVarArg=*/ false);

  Type *DFSanUnionDerivFloatArgs[7] = { ShadowTy, ShadowTy, Type::getFloatTy(*Ctx), Type::getFloatTy(*Ctx), IntptrTy, OpCodeTy, CharPtrTy};
  DFSanUnionFnDerivFloatTy =
          FunctionType::get(ShadowTy, DFSanUnionDerivFloatArgs, /*isVarArg=*/ false);

  Type *DFSanUnionDerivDoubleArgs[7] = { ShadowTy, ShadowTy, Type::getDoubleTy(*Ctx), Type::getDoubleTy(*Ctx), IntptrTy, OpCodeTy, CharPtrTy};
  DFSanUnionFnDerivDoubleTy =
          FunctionType::get(ShadowTy, DFSanUnionDerivDoubleArgs, /*isVarArg=*/ false);


  Type *DFSanUnionLoadArgs[2] = { ShadowPtrTy, IntptrTy };
  DFSanUnionLoadFnTy =
      FunctionType::get(ShadowTy, DFSanUnionLoadArgs, /*isVarArg=*/ false);
  DFSanUnimplementedFnTy = FunctionType::get(
      Type::getVoidTy(*Ctx), Type::getInt8PtrTy(*Ctx), /*isVarArg=*/false);
  Type *DFSanSetLabelArgs[3] = { ShadowTy, Type::getInt8PtrTy(*Ctx), IntptrTy };
  DFSanSetLabelFnTy = FunctionType::get(Type::getVoidTy(*Ctx),
                                        DFSanSetLabelArgs, /*isVarArg=*/false);
  DFSanNonzeroLabelFnTy = FunctionType::get(
      Type::getVoidTy(*Ctx), None, /*isVarArg=*/false);
  DFSanVarargWrapperFnTy = FunctionType::get(
      Type::getVoidTy(*Ctx), Type::getInt8PtrTy(*Ctx), /*isVarArg=*/false);

  if (GetArgTLSPtr) {
    Type *ArgTLSTy = ArrayType::get(ShadowTy, 64);
    ArgTLS = nullptr;
    GetArgTLS = ConstantExpr::getIntToPtr(
        ConstantInt::get(IntptrTy, uintptr_t(GetArgTLSPtr)),
        PointerType::getUnqual(
            FunctionType::get(PointerType::getUnqual(ArgTLSTy), false)));
  }
  if (GetRetvalTLSPtr) {
    RetvalTLS = nullptr;
    GetRetvalTLS = ConstantExpr::getIntToPtr(
        ConstantInt::get(IntptrTy, uintptr_t(GetRetvalTLSPtr)),
        PointerType::getUnqual(
            FunctionType::get(PointerType::getUnqual(ShadowTy), false)));
  }

  ColdCallWeights = MDBuilder(*Ctx).createBranchWeights(1, 1000);

  branch_id = 0;
  return true;
}

bool DataFlowSanitizer::isInstrumented(const Function *F) {
  return !ABIList.isIn(*F, "uninstrumented");
}

bool DataFlowSanitizer::isInstrumented(const GlobalAlias *GA) {
  return !ABIList.isIn(*GA, "uninstrumented");
}

DataFlowSanitizer::InstrumentedABI DataFlowSanitizer::getInstrumentedABI() {
  return ClArgsABI ? IA_Args : IA_TLS;
}

DataFlowSanitizer::WrapperKind DataFlowSanitizer::getWrapperKind(Function *F) {
  if (ABIList.isIn(*F, "functional"))
    return WK_Functional;
  if (ABIList.isIn(*F, "discard"))
    return WK_Discard;
  if (ABIList.isIn(*F, "custom"))
    return WK_Custom;

  return WK_Warning;
}

void DataFlowSanitizer::addGlobalNamePrefix(GlobalValue *GV) {
  std::string GVName = GV->getName(), Prefix = "dfs$";
  GV->setName(Prefix + GVName);

  // Try to change the name of the function in module inline asm.  We only do
  // this for specific asm directives, currently only ".symver", to try to avoid
  // corrupting asm which happens to contain the symbol name as a substring.
  // Note that the substitution for .symver assumes that the versioned symbol
  // also has an instrumented name.
  std::string Asm = GV->getParent()->getModuleInlineAsm();
  std::string SearchStr = ".symver " + GVName + ",";
  size_t Pos = Asm.find(SearchStr);
  if (Pos != std::string::npos) {
    Asm.replace(Pos, SearchStr.size(),
                ".symver " + Prefix + GVName + "," + Prefix);
    GV->getParent()->setModuleInlineAsm(Asm);
  }
}

Function *
DataFlowSanitizer::buildWrapperFunction(Function *F, StringRef NewFName,
                                        GlobalValue::LinkageTypes NewFLink,
                                        FunctionType *NewFT) {
  // gabe: shouldn't be called with vararg
  if (F->isVarArg()) {
    report_fatal_error(std::string("Unable to instrument vararg function ") + std::string(F->getName()));
  }

  FunctionType *FT = F->getFunctionType();
  Function *NewF = Function::Create(NewFT, NewFLink, NewFName,
                                    F->getParent());
  NewF->copyAttributesFrom(F);
  NewF->removeAttributes(
      AttributeList::ReturnIndex,
      AttributeFuncs::typeIncompatible(NewFT->getReturnType()));

  BasicBlock *BB = BasicBlock::Create(*Ctx, "entry", NewF);

  if (F->isVarArg()) {
    NewF->removeAttributes(AttributeList::FunctionIndex,
                           AttrBuilder().addAttribute("split-stack"));
    CallInst::Create(DFSanVarargWrapperFn,
                     IRBuilder<>(BB).CreateGlobalStringPtr(F->getName()), "",
                     BB);
    new UnreachableInst(*Ctx, BB);
  } else {
    std::vector<Value *> Args;
    unsigned n = FT->getNumParams();
    for (Function::arg_iterator ai = NewF->arg_begin(); n != 0; ++ai, --n)
      Args.push_back(&*ai);
    CallInst *CI = CallInst::Create(F, Args, "", BB);
    if (FT->getReturnType()->isVoidTy())
      ReturnInst::Create(*Ctx, BB);
    else
      ReturnInst::Create(*Ctx, CI, BB);
  }

  return NewF;
}

Constant *DataFlowSanitizer::getOrBuildTrampolineFunction(FunctionType *FT,
                                                          StringRef FName) {
  FunctionType *FTT = getTrampolineFunctionType(FT);
  Constant *C = Mod->getOrInsertFunction(FName, FTT);
  Function *F = dyn_cast<Function>(C);
  if (F && F->isDeclaration()) {
    F->setLinkage(GlobalValue::LinkOnceODRLinkage);
    BasicBlock *BB = BasicBlock::Create(*Ctx, "entry", F);
    std::vector<Value *> Args;
    Function::arg_iterator AI = F->arg_begin(); ++AI;
    for (unsigned N = FT->getNumParams(); N != 0; ++AI, --N)
      Args.push_back(&*AI);
    CallInst *CI = CallInst::Create(&*F->arg_begin(), Args, "", BB);
    ReturnInst *RI;
    if (FT->getReturnType()->isVoidTy())
      RI = ReturnInst::Create(*Ctx, BB);
    else
      RI = ReturnInst::Create(*Ctx, CI, BB);

    DFSanFunction DFSF(*this, F, /*IsNativeABI=*/true);
    Function::arg_iterator ValAI = F->arg_begin(), ShadowAI = AI; ++ValAI;
    for (unsigned N = FT->getNumParams(); N != 0; ++ValAI, ++ShadowAI, --N)
      DFSF.ValShadowMap[&*ValAI] = &*ShadowAI;
    DFSanVisitor(DFSF).visitCallInst(*CI);
    if (!FT->getReturnType()->isVoidTy())
      new StoreInst(DFSF.getShadow(RI->getReturnValue()),
                    &*std::prev(F->arg_end()), RI);
  }

  return C;
}

bool DataFlowSanitizer::runOnModule(Module &M) {
  if (ABIList.isIn(M, "skip"))
    return false;

  if (!GetArgTLSPtr) {
    Type *ArgTLSTy = ArrayType::get(ShadowTy, 64);
    ArgTLS = Mod->getOrInsertGlobal("__dfsan_arg_tls", ArgTLSTy);
    if (GlobalVariable *G = dyn_cast<GlobalVariable>(ArgTLS))
      G->setThreadLocalMode(GlobalVariable::InitialExecTLSModel);
  }
  if (!GetRetvalTLSPtr) {
    RetvalTLS = Mod->getOrInsertGlobal("__dfsan_retval_tls", ShadowTy);
    if (GlobalVariable *G = dyn_cast<GlobalVariable>(RetvalTLS))
      G->setThreadLocalMode(GlobalVariable::InitialExecTLSModel);
  }

  ExternalShadowMask =
      Mod->getOrInsertGlobal(kDFSanExternShadowPtrMask, IntptrTy);

  MemCpyFn = Mod->getOrInsertFunction("__memcpy", MemCpyFnTy);
  if (Function *F = dyn_cast<Function>(MemCpyFn)) {
    F->addParamAttr(2, Attribute::ZExt);
    F->addParamAttr(3, Attribute::ZExt);
    F->addParamAttr(4, Attribute::ZExt);
    F->addParamAttr(5, Attribute::ZExt);
  }

    BasicBlockFn = Mod->getOrInsertFunction("__basicblock", BasicBlockFnTy);

  BranchVisitorCharFn = Mod->getOrInsertFunction("__branch_visitor_char", BranchVisitorCharFnTy);
  if (Function *F = dyn_cast<Function>(BranchVisitorCharFn)) {
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }

  BranchVisitorShortFn = Mod->getOrInsertFunction("__branch_visitor_short", BranchVisitorShortFnTy);
  if (Function *F = dyn_cast<Function>(BranchVisitorShortFn)) {
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }

  BranchVisitorIntFn = Mod->getOrInsertFunction("__branch_visitor_int", BranchVisitorIntFnTy);
  if (Function *F = dyn_cast<Function>(BranchVisitorIntFn)) {
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }

  BranchVisitorLongFn = Mod->getOrInsertFunction("__branch_visitor_long", BranchVisitorLongFnTy);
  if (Function *F = dyn_cast<Function>(BranchVisitorLongFn)) {
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }

  BranchVisitorLongLongFn = Mod->getOrInsertFunction("__branch_visitor_longlong", BranchVisitorLongLongFnTy);
  if (Function *F = dyn_cast<Function>(BranchVisitorLongLongFn)) {
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }

  BranchVisitorFloatFn = Mod->getOrInsertFunction("__branch_visitor_float", BranchVisitorFloatFnTy);
  if (Function *F = dyn_cast<Function>(BranchVisitorFloatFn)) {
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }

  BranchVisitorDoubleFn = Mod->getOrInsertFunction("__branch_visitor_double", BranchVisitorDoubleFnTy);
  if (Function *F = dyn_cast<Function>(BranchVisitorDoubleFn)) {
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }


  DFSanUnionUnSupFn = Mod->getOrInsertFunction("__dfsan_union_unsupported_type", DFSanUnionUnSupFnDerivTy);
  if (Function *F = dyn_cast<Function>(DFSanUnionUnSupFn)) {
    F->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeList::FunctionIndex, Attribute::ReadNone);
    F->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }
   

  DFSanUnionFn = Mod->getOrInsertFunction("__dfsan_union", DFSanUnionFnDerivTy);
  if (Function *F = dyn_cast<Function>(DFSanUnionFn)) {
    F->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeList::FunctionIndex, Attribute::ReadNone);
    F->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }
  DFSanUnionLongFn = Mod->getOrInsertFunction("__dfsan_union_long", DFSanUnionFnDerivLongTy);
  if (Function *F = dyn_cast<Function>(DFSanUnionLongFn)) {
    F->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeList::FunctionIndex, Attribute::ReadNone);
    F->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }
  DFSanUnionByteFn = Mod->getOrInsertFunction("__dfsan_union_byte", DFSanUnionFnDerivByteTy);
  if (Function *F = dyn_cast<Function>(DFSanUnionByteFn)) {
    F->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeList::FunctionIndex, Attribute::ReadNone);
    F->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }
  DFSanUnionShortFn = Mod->getOrInsertFunction("__dfsan_union_short", DFSanUnionFnDerivShortTy);
  if (Function *F = dyn_cast<Function>(DFSanUnionShortFn)) {
    F->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeList::FunctionIndex, Attribute::ReadNone);
    F->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }

  DFSanUnionFloatFn = Mod->getOrInsertFunction("__dfsan_union_float", DFSanUnionFnDerivFloatTy);
  if (Function *F = dyn_cast<Function>(DFSanUnionFloatFn)) {
    F->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeList::FunctionIndex, Attribute::ReadNone);
    F->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }
  DFSanUnionDoubleFn = Mod->getOrInsertFunction("__dfsan_union_double", DFSanUnionFnDerivDoubleTy);
  if (Function *F = dyn_cast<Function>(DFSanUnionDoubleFn)) {
    F->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeList::FunctionIndex, Attribute::ReadNone);
    F->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
    F->addParamAttr(0, Attribute::ZExt);
    F->addParamAttr(1, Attribute::ZExt);
  }
  DFSanUnionLoadFn =
      Mod->getOrInsertFunction("__dfsan_union_load", DFSanUnionLoadFnTy);
  if (Function *F = dyn_cast<Function>(DFSanUnionLoadFn)) {
    F->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeList::FunctionIndex, Attribute::ReadOnly);
    F->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
  }
  DFSanUnimplementedFn =
      Mod->getOrInsertFunction("__dfsan_unimplemented", DFSanUnimplementedFnTy);
  DFSanSetLabelFn =
      Mod->getOrInsertFunction("__dfsan_set_label", DFSanSetLabelFnTy);
  if (Function *F = dyn_cast<Function>(DFSanSetLabelFn)) {
    F->addParamAttr(0, Attribute::ZExt);
  }
  DFSanNonzeroLabelFn =
      Mod->getOrInsertFunction("__dfsan_nonzero_label", DFSanNonzeroLabelFnTy);
  DFSanVarargWrapperFn = Mod->getOrInsertFunction("__dfsan_vararg_wrapper",
                                                  DFSanVarargWrapperFnTy);

  std::vector<Function *> FnsToInstrument;
  SmallPtrSet<Function *, 2> FnsWithNativeABI;
  for (Function &i : M) {
    if (!i.isIntrinsic() &&
            &i != MemCpyFn &&
            &i != BasicBlockFn &&
            &i != BranchVisitorCharFn &&
            &i != BranchVisitorShortFn &&
            &i != BranchVisitorIntFn &&
            &i != BranchVisitorLongFn &&
            &i != BranchVisitorLongLongFn &&
            &i != BranchVisitorFloatFn &&
            &i != BranchVisitorDoubleFn &&
            &i != DFSanUnionFn &&
            &i != DFSanUnionUnSupFn &&
        &i != DFSanUnionLongFn &&
            &i != DFSanUnionByteFn &&
            &i != DFSanUnionShortFn &&
            &i != DFSanUnionFloatFn &&
            &i != DFSanUnionDoubleFn &&
        &i != DFSanUnionLoadFn &&
        &i != DFSanUnimplementedFn &&
        &i != DFSanSetLabelFn &&
        &i != DFSanNonzeroLabelFn &&
        &i != DFSanVarargWrapperFn)
      FnsToInstrument.push_back(&i);
  }

  // Give function aliases prefixes when necessary, and build wrappers where the
  // instrumentedness is inconsistent.
  for (Module::alias_iterator i = M.alias_begin(), e = M.alias_end(); i != e;) {
    GlobalAlias *GA = &*i;
    ++i;
    // Don't stop on weak.  We assume people aren't playing games with the
    // instrumentedness of overridden weak aliases.
    if (auto F = dyn_cast<Function>(GA->getBaseObject())) {
      bool GAInst = isInstrumented(GA), FInst = isInstrumented(F);
      if (GAInst && FInst) {
        addGlobalNamePrefix(GA);
      } else if (GAInst != FInst) {
        // gabe: skip varargs
        errs() << "skipping varargs " << F->getName() << "\n";
        if (F->isVarArg()) continue;

        // Non-instrumented alias of an instrumented function, or vice versa.
        // Replace the alias with a native-ABI wrapper of the aliasee.  The pass
        // below will take care of instrumenting it.
        Function *NewF =
            buildWrapperFunction(F, "", GA->getLinkage(), F->getFunctionType());
        GA->replaceAllUsesWith(ConstantExpr::getBitCast(NewF, GA->getType()));
        NewF->takeName(GA);
        GA->eraseFromParent();
        FnsToInstrument.push_back(NewF);
      }
    }
  }

  ReadOnlyNoneAttrs.addAttribute(Attribute::ReadOnly)
      .addAttribute(Attribute::ReadNone);


  // First, change the ABI of every function in the module.  ABI-listed
  // functions keep their original ABI and get a wrapper function.
  for (std::vector<Function *>::iterator i = FnsToInstrument.begin(),
                                         e = FnsToInstrument.end();
       i != e; ++i) {
    Function &F = **i;
    FunctionType *FT = F.getFunctionType();

    bool IsZeroArgsVoidRet = (FT->getNumParams() == 0 && !FT->isVarArg() &&
                              FT->getReturnType()->isVoidTy());

    if (isInstrumented(&F)) {
      // Instrumented functions get a 'dfs$' prefix.  This allows us to more
      // easily identify cases of mismatching ABIs.
      if (getInstrumentedABI() == IA_Args && !IsZeroArgsVoidRet) {
        FunctionType *NewFT = getArgsFunctionType(FT);
        Function *NewF = Function::Create(NewFT, F.getLinkage(), "", &M);
        NewF->copyAttributesFrom(&F);
        NewF->removeAttributes(
            AttributeList::ReturnIndex,
            AttributeFuncs::typeIncompatible(NewFT->getReturnType()));
        for (Function::arg_iterator FArg = F.arg_begin(),
                                    NewFArg = NewF->arg_begin(),
                                    FArgEnd = F.arg_end();
             FArg != FArgEnd; ++FArg, ++NewFArg) {
          FArg->replaceAllUsesWith(&*NewFArg);
        }
        NewF->getBasicBlockList().splice(NewF->begin(), F.getBasicBlockList());

        for (Function::user_iterator UI = F.user_begin(), UE = F.user_end();
             UI != UE;) {
          BlockAddress *BA = dyn_cast<BlockAddress>(*UI);
          ++UI;
          if (BA) {
            BA->replaceAllUsesWith(
                BlockAddress::get(NewF, BA->getBasicBlock()));
            delete BA;
          }
        }
        F.replaceAllUsesWith(
            ConstantExpr::getBitCast(NewF, PointerType::getUnqual(FT)));
        NewF->takeName(&F);
        F.eraseFromParent();
        *i = NewF;
        addGlobalNamePrefix(NewF);
      } else {
        addGlobalNamePrefix(&F);
      }
    } else if ( (!IsZeroArgsVoidRet || getWrapperKind(&F) == WK_Custom)
        && !FT->isVarArg()) {
      // gabe: skip vararg

      // Build a wrapper function for F.  The wrapper simply calls F, and is
      // added to FnsToInstrument so that any instrumentation according to its
      // WrapperKind is done in the second pass below.
      FunctionType *NewFT = getInstrumentedABI() == IA_Args
                                ? getArgsFunctionType(FT)
                                : FT;

      // If the function being wrapped has local linkage, then preserve the
      // function's linkage in the wrapper function.
      GlobalValue::LinkageTypes wrapperLinkage =
          F.hasLocalLinkage()
              ? F.getLinkage()
              : GlobalValue::LinkOnceODRLinkage;

      Function *NewF = buildWrapperFunction(
          &F, std::string("dfsw$") + std::string(F.getName()),
          wrapperLinkage, NewFT);
      if (getInstrumentedABI() == IA_TLS)
        NewF->removeAttributes(AttributeList::FunctionIndex, ReadOnlyNoneAttrs);

      Value *WrappedFnCst =
          ConstantExpr::getBitCast(NewF, PointerType::getUnqual(FT));
      F.replaceAllUsesWith(WrappedFnCst);

      UnwrappedFnMap[WrappedFnCst] = &F;
      *i = NewF;

      if (!F.isDeclaration()) {
        // This function is probably defining an interposition of an
        // uninstrumented function and hence needs to keep the original ABI.
        // But any functions it may call need to use the instrumented ABI, so
        // we instrument it in a mode which preserves the original ABI.
        FnsWithNativeABI.insert(&F);

        // This code needs to rebuild the iterators, as they may be invalidated
        // by the push_back, taking care that the new range does not include
        // any functions added by this code.
        size_t N = i - FnsToInstrument.begin(),
               Count = e - FnsToInstrument.begin();
        FnsToInstrument.push_back(&F);
        i = FnsToInstrument.begin() + N;
        e = FnsToInstrument.begin() + Count;
      }
               // Hopefully, nobody will try to indirectly call a vararg
               // function... yet.
    } else if (FT->isVarArg()) {

      errs() << "ignoring vararg function " << std::string(F.getName())  << "\n";

      UnwrappedFnMap[&F] = &F;
      *i = nullptr;
    }
  }

  for (Function *i : FnsToInstrument) {
    if (!i || i->isDeclaration())
      continue;

    removeUnreachableBlocks(*i);

    DFSanFunction DFSF(*this, i, FnsWithNativeABI.count(i));

    // DFSanVisitor may create new basic blocks, which confuses df_iterator.
    // Build a copy of the list before iterating over it.
    SmallVector<BasicBlock *, 4> BBList(depth_first(&i->getEntryBlock()));

    for (BasicBlock *i : BBList) {

        DFSF.recordBasicBlock(i);
      Instruction *Inst = &i->front();
      while (true) {
        // DFSanVisitor may split the current basic block, changing the current
        // instruction's next pointer and moving the next instruction to the
        // tail block from which we should continue.
        Instruction *Next = Inst->getNextNode();
        // DFSanVisitor may delete Inst, so keep track of whether it was a
        // terminator.
        bool IsTerminator = isa<TerminatorInst>(Inst);
        if (!DFSF.SkipInsts.count(Inst))
          DFSanVisitor(DFSF).visit(Inst);
        if (IsTerminator)
          break;
        Inst = Next;
      }
    }

    // We will not necessarily be able to compute the shadow for every phi node
    // until we have visited every block.  Therefore, the code that handles phi
    // nodes adds them to the PHIFixups list so that they can be properly
    // handled here.
    for (std::vector<std::pair<PHINode *, PHINode *>>::iterator
             i = DFSF.PHIFixups.begin(),
             e = DFSF.PHIFixups.end();
         i != e; ++i) {
      for (unsigned val = 0, n = i->first->getNumIncomingValues(); val != n;
           ++val) {
        i->second->setIncomingValue(
            val, DFSF.getShadow(i->first->getIncomingValue(val)));
      }
    }

    // -dfsan-debug-nonzero-labels will split the CFG in all kinds of crazy
    // places (i.e. instructions in basic blocks we haven't even begun visiting
    // yet).  To make our life easier, do this work in a pass after the main
    // instrumentation.
    if (ClDebugNonzeroLabels) {
      for (Value *V : DFSF.NonZeroChecks) {
        Instruction *Pos;
        if (Instruction *I = dyn_cast<Instruction>(V))
          Pos = I->getNextNode();
        else
          Pos = &DFSF.F->getEntryBlock().front();
        while (isa<PHINode>(Pos) || isa<AllocaInst>(Pos))
          Pos = Pos->getNextNode();
        IRBuilder<> IRB(Pos);
        Value *Ne = IRB.CreateICmpNE(V, DFSF.DFS.ZeroShadow);
        BranchInst *BI = cast<BranchInst>(SplitBlockAndInsertIfThen(
            Ne, Pos, /*Unreachable=*/false, ColdCallWeights));
        IRBuilder<> ThenIRB(BI);
        ThenIRB.CreateCall(DFSF.DFS.DFSanNonzeroLabelFn, {});
      }
    }
  }

  return false;
}

Value *DFSanFunction::getArgTLSPtr() {
  if (ArgTLSPtr)
    return ArgTLSPtr;
  if (DFS.ArgTLS)
    return ArgTLSPtr = DFS.ArgTLS;

  IRBuilder<> IRB(&F->getEntryBlock().front());
  return ArgTLSPtr = IRB.CreateCall(DFS.GetArgTLS, {});
}

Value *DFSanFunction::getRetvalTLS() {
  if (RetvalTLSPtr)
    return RetvalTLSPtr;
  if (DFS.RetvalTLS)
    return RetvalTLSPtr = DFS.RetvalTLS;

  IRBuilder<> IRB(&F->getEntryBlock().front());
  return RetvalTLSPtr = IRB.CreateCall(DFS.GetRetvalTLS, {});
}

Value *DFSanFunction::getArgTLS(unsigned Idx, Instruction *Pos) {
  IRBuilder<> IRB(Pos);
  return IRB.CreateConstGEP2_64(getArgTLSPtr(), 0, Idx);
}

Value *DFSanFunction::getShadow(Value *V) {
  if (!isa<Argument>(V) && !isa<Instruction>(V))
    return DFS.ZeroShadow;
  Value *&Shadow = ValShadowMap[V];
  if (!Shadow) {
    if (Argument *A = dyn_cast<Argument>(V)) {
      if (IsNativeABI)
        return DFS.ZeroShadow;
      switch (IA) {
      case DataFlowSanitizer::IA_TLS: {
        Value *ArgTLSPtr = getArgTLSPtr();
        Instruction *ArgTLSPos =
            DFS.ArgTLS ? &*F->getEntryBlock().begin()
                       : cast<Instruction>(ArgTLSPtr)->getNextNode();
        IRBuilder<> IRB(ArgTLSPos);
        Shadow = IRB.CreateLoad(getArgTLS(A->getArgNo(), ArgTLSPos));
        break;
      }
      case DataFlowSanitizer::IA_Args: {
        unsigned ArgIdx = A->getArgNo() + F->arg_size() / 2;
        Function::arg_iterator i = F->arg_begin();
        while (ArgIdx--)
          ++i;
        Shadow = &*i;
        assert(Shadow->getType() == DFS.ShadowTy);
        break;
      }
      }
      NonZeroChecks.push_back(Shadow);
    } else {
      Shadow = DFS.ZeroShadow;
    }
  }
  return Shadow;
}

void DFSanFunction::setShadow(Instruction *I, Value *Shadow) {
  assert(!ValShadowMap.count(I));
  assert(Shadow->getType() == DFS.ShadowTy);
  ValShadowMap[I] = Shadow;
}

Value *DataFlowSanitizer::getShadowAddress(Value *Addr, Instruction *Pos) {
   
  assert(Addr != RetvalTLS && "Reinstrumenting?");
  IRBuilder<> IRB(Pos);
  Value *ShadowPtrMaskValue;
  if (DFSanRuntimeShadowMask)
    ShadowPtrMaskValue = IRB.CreateLoad(IntptrTy, ExternalShadowMask);
  else
    ShadowPtrMaskValue = ShadowPtrMask;
  return IRB.CreateIntToPtr(
      IRB.CreateMul(
          IRB.CreateAnd(IRB.CreatePtrToInt(Addr, IntptrTy),
                        IRB.CreatePtrToInt(ShadowPtrMaskValue, IntptrTy)),
          ShadowPtrMul),
      ShadowPtrTy);
}


void DFSanFunction::recordBranchInst(BranchInst &I, Value* lhs_shadow,
                                     Value* rhs_shadow, Value* lhs, Value* rhs,
                                     unsigned int pred, std::string location) {
  IRBuilder<> IRB(&I);
  CallInst *Call;

  Constant* visitorFunction;

  Value* cond = I.getCondition();
  Value* isPointer = ConstantInt::get(DFS.InstIdTy, 0);
  IntegerType* ctype = dyn_cast<IntegerType>(cond->getType());
  assert(ctype);
  assert(ctype->getBitWidth() == 1);

  if (auto *type = dyn_cast<IntegerType>(lhs->getType())) {
    switch (type->getBitWidth()) {
      case 8:
        visitorFunction = DFS.BranchVisitorCharFn;
        break;
      case 16:
        visitorFunction = DFS.BranchVisitorShortFn;
        break;
      case 32:
        visitorFunction = DFS.BranchVisitorIntFn;
        break;
      case 64:
        visitorFunction = DFS.BranchVisitorLongFn;
        break;
      case 128:
        visitorFunction = DFS.BranchVisitorLongLongFn;
        break;
      default:
        /* just cast to 64 in this case */
        errs() << "branch casting: " << I << " " << *I.getCondition() << " " << location << "\n";
        errs() << "lhs: "<<*lhs<<' '<<*lhs->getType()<<" rhs: "<<*rhs<<' '<<*rhs->getType()<<"\n";
        lhs = IRB.CreateZExt(lhs, DFS.Int64Ty);
        rhs = IRB.CreateZExt(rhs, DFS.Int64Ty);
        visitorFunction = DFS.BranchVisitorLongFn;
        break;
    }
  } else if (lhs->getType()->isFloatTy()) {
    visitorFunction = DFS.BranchVisitorFloatFn;
  } else if (lhs->getType()->isDoubleTy()) {
    visitorFunction = DFS.BranchVisitorDoubleFn;
  } else if (dyn_cast<PointerType>(lhs->getType())) {
    return;

  } else {
    errs() << "branch error: " << I << " " << *I.getCondition() << "\n";
    return;
  }

  // now know branch is valid: get branch id and instrument branch
  uint64_t br_id = DFS.branch_id.fetch_add(1, std::memory_order_relaxed);

  // get file id:
  std::hash<std::string> str_hash;
  size_t file_id = str_hash(I.getModule()->getSourceFileName());

  Value *args[10] = {lhs_shadow, rhs_shadow, lhs, rhs, cond,
                     ConstantInt::get(DFS.Int32Ty, pred),
                     ConstantInt::get(DFS.SizeTy, file_id),
                     ConstantInt::get(DFS.SizeTy, br_id),
                     isPointer,
                     IRB.CreateGlobalStringPtr(StringRef(location))};

  Call = IRB.CreateCall(visitorFunction, args);
  Call->addParamAttr(0, Attribute::ZExt);
  Call->addParamAttr(1, Attribute::ZExt);
}


// Generates IR to compute the union of the two given shadows, inserting it
// before Pos.  Returns the computed union Value.
Value *DFSanFunction::combineDerivShadows(Value *V1, Value *V2, Instruction *Pos, Value *UV1, Value *UV2) {

  bool x1_is_byte  = UV1->getType() == IntegerType::get(*DFS.Ctx, 8);
  bool x2_is_byte  = UV2->getType() == IntegerType::get(*DFS.Ctx, 8);
  bool x1_is_short = UV1->getType() == IntegerType::get(*DFS.Ctx, 16);
  bool x2_is_short = UV2->getType() == IntegerType::get(*DFS.Ctx, 16);
  bool x1_is_int   = UV1->getType() == IntegerType::get(*DFS.Ctx, 32);
  bool x2_is_int   = UV2->getType() == IntegerType::get(*DFS.Ctx, 32);
  bool x1_is_long  = UV1->getType() == IntegerType::get(*DFS.Ctx, 64);
  bool x2_is_long  = UV2->getType() == IntegerType::get(*DFS.Ctx, 64);
  bool x1_is_float  = UV1->getType() == Type::getFloatTy(*DFS.Ctx);
  bool x2_is_float  = UV2->getType() == Type::getFloatTy(*DFS.Ctx);
  bool x1_is_double  = UV1->getType() == Type::getDoubleTy(*DFS.Ctx);
  bool x2_is_double  = UV2->getType() == Type::getDoubleTy(*DFS.Ctx);

  IRBuilder<> IRB(Pos);
  CallInst *Call;

  Constant* instructionID = ConstantInt::get(IntegerType::get(*DFS.Ctx, 64), 0);
  Constant* opcode = ConstantInt::get(DFS.OpCodeTy, Pos->getOpcode());


  std::string location = std::string("UNKNOWN");
  if (DILocation *Loc = Pos->getDebugLoc().get()) {
    location = std::string(Loc->getFilename().data()) + std::string(":") + std::to_string(Loc->getLine());
  }


  if (x1_is_byte && x2_is_byte) {
    Call = IRB.CreateCall(DFS.DFSanUnionByteFn, {V1, V2, UV1, UV2, instructionID, opcode, IRB.CreateGlobalStringPtr(StringRef(location))});
  }
  else if (x1_is_short && x2_is_short) {
    Call = IRB.CreateCall(DFS.DFSanUnionShortFn, {V1, V2, UV1, UV2, instructionID, opcode, IRB.CreateGlobalStringPtr(StringRef(location))});
  }
  else if (x1_is_int && x2_is_int) {
    Call = IRB.CreateCall(DFS.DFSanUnionFn, {V1, V2, UV1, UV2, instructionID, opcode, IRB.CreateGlobalStringPtr(StringRef(location))});
  }
  else if (x1_is_long && x2_is_long) {
    Call = IRB.CreateCall(DFS.DFSanUnionLongFn, {V1, V2, UV1, UV2, instructionID, opcode, IRB.CreateGlobalStringPtr(StringRef(location))});
  }
  else if (x1_is_float && x2_is_float) {
    Call = IRB.CreateCall(DFS.DFSanUnionFloatFn, {V1, V2, UV1, UV2, instructionID, opcode, IRB.CreateGlobalStringPtr(StringRef(location))});
  }
  else if (x1_is_double && x2_is_double) {
    Call = IRB.CreateCall(DFS.DFSanUnionDoubleFn, {V1, V2, UV1, UV2, instructionID, opcode, IRB.CreateGlobalStringPtr(StringRef(location))});
  }
  else {
    // set derivOp = 0 for unsupported type combination
    Call = IRB.CreateCall(DFS.DFSanUnionUnSupFn, {V1, V2, instructionID, opcode, IRB.CreateGlobalStringPtr(StringRef(location))});
    errs() << "Unsupported Type for " << *Pos << " -- " << *UV1->getType() << ' ' << *UV2->getType() << " "
      << location << "\n";
  }
  Call->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
  Call->addParamAttr(0, Attribute::ZExt);
  Call->addParamAttr(1, Attribute::ZExt);

  return Call;
}


// Generates IR to compute the union of the two given shadows, inserting it
// before Pos.  Returns the computed union Value.
Value *DFSanFunction::combineShadows(Value *V1, Value *V2, Instruction *Pos) {

  IRBuilder<> IRB(Pos);
  CallInst *Call;
  Value * zero = ConstantInt::get(IntegerType::get(*DFS.Ctx, 32), 0);

  // debug info:
  std::string location = "UNKNOWN";
  if (DILocation *Loc = Pos->getDebugLoc().get()) {
    std::string message = std::string(Loc->getFilename().data()) + std::string(":") + std::to_string(Loc->getLine());
  }

  Constant* instructionID = ConstantInt::get(IntegerType::get(*DFS.Ctx, 64), 0);
  Constant* opcode = ConstantInt::get(DFS.OpCodeTy, Pos->getOpcode());

  Call = IRB.CreateCall(DFS.DFSanUnionFn, {V1, V2, zero, zero, instructionID, opcode, IRB.CreateGlobalStringPtr(StringRef(location))});

  Call->addAttribute(AttributeList::ReturnIndex, Attribute::ZExt);
  Call->addParamAttr(0, Attribute::ZExt);
  Call->addParamAttr(1, Attribute::ZExt);

  return Call;
}

// A convenience function which folds the shadows of each of the operands
// of the provided instruction Inst, inserting the IR before Inst.  Returns
// the computed union Value.
Value *DFSanFunction::combineOperandShadows(Instruction *Inst) {
  if (Inst->getNumOperands() == 0)
    return DFS.ZeroShadow;

  Value *Shadow = getShadow(Inst->getOperand(0));
  for (unsigned i = 1, n = Inst->getNumOperands(); i != n; ++i) {
    Shadow = combineShadows(Shadow, getShadow(Inst->getOperand(i)), Inst);
  }

  return Shadow;
}

void DFSanFunction::recordBasicBlock(BasicBlock* BB) {
   return;
}


void DFSanVisitor::visitOperandShadowInst(Instruction &I) {
  Value *CombinedShadow = DFSF.combineOperandShadows(&I);
  DFSF.setShadow(&I, CombinedShadow);
}

// Generates IR to load shadow corresponding to bytes [Addr, Addr+Size), where
// Addr has alignment Align, and take the union of each of those shadows.
Value *DFSanFunction::loadShadow(Value *Addr, uint64_t Size, uint64_t Align,
                                 Instruction *Pos) {
  if (AllocaInst *AI = dyn_cast<AllocaInst>(Addr)) {
    const auto i = AllocaShadowMap.find(AI);
    if (i != AllocaShadowMap.end()) {
      IRBuilder<> IRB(Pos);
      return IRB.CreateLoad(i->second);
    }
  }

  uint64_t ShadowAlign = Align * DFS.ShadowWidth / 8;
  SmallVector<Value *, 2> Objs;
  GetUnderlyingObjects(Addr, Objs, Pos->getModule()->getDataLayout());
  bool AllConstants = true;
  for (Value *Obj : Objs) {
    if (isa<Function>(Obj) || isa<BlockAddress>(Obj))
      continue;
    if (isa<GlobalVariable>(Obj) && cast<GlobalVariable>(Obj)->isConstant())
      continue;

    AllConstants = false;
    break;
  }
  if (AllConstants)
    return DFS.ZeroShadow;

  Value *ShadowAddr = DFS.getShadowAddress(Addr, Pos);

  // for load Inst gradient take gradient of shadow at base
  // eventually may need to combine gradients of multiple shadows
  if (Size == 0) {
    return DFS.ZeroShadow;
  } else {
    LoadInst *LI = new LoadInst(ShadowAddr, "", Pos);
    LI->setAlignment(ShadowAlign);
    return LI;
  }

}

void DFSanVisitor::visitLoadInst(LoadInst &LI) {
  auto &DL = LI.getModule()->getDataLayout();
  uint64_t Size = DL.getTypeStoreSize(LI.getType());
  if (Size == 0) {
    DFSF.setShadow(&LI, DFSF.DFS.ZeroShadow);
    return;
  }

  uint64_t Align;
  if (ClPreserveAlignment) {
    Align = LI.getAlignment();
    if (Align == 0)
      Align = DL.getABITypeAlignment(LI.getType());
  } else {
    Align = 1;
  }
  IRBuilder<> IRB(&LI);
  Value *Shadow = DFSF.loadShadow(LI.getPointerOperand(), Size, Align, &LI);
  if (Shadow != DFSF.DFS.ZeroShadow)
    DFSF.NonZeroChecks.push_back(Shadow);

  DFSF.setShadow(&LI, Shadow);
}

void DFSanFunction::storeShadow(Value *Addr, uint64_t Size, uint64_t Align,
                                Value *Shadow, Instruction *Pos) {
  if (AllocaInst *AI = dyn_cast<AllocaInst>(Addr)) {
    const auto i = AllocaShadowMap.find(AI);
    if (i != AllocaShadowMap.end()) {
      IRBuilder<> IRB(Pos);
      IRB.CreateStore(Shadow, i->second);
      return;
    }
  }

  uint64_t ShadowAlign = Align * DFS.ShadowWidth / 8;
  IRBuilder<> IRB(Pos);
  Value *ShadowAddr = DFS.getShadowAddress(Addr, Pos);
  if (Shadow == DFS.ZeroShadow) {
    IntegerType *ShadowTy = IntegerType::get(*DFS.Ctx, Size * DFS.ShadowWidth);
    Value *ExtZeroShadow = ConstantInt::get(ShadowTy, 0);
    Value *ExtShadowAddr =
        IRB.CreateBitCast(ShadowAddr, PointerType::getUnqual(ShadowTy));
    IRB.CreateAlignedStore(ExtZeroShadow, ExtShadowAddr, ShadowAlign);
    return;
  }

  const unsigned ShadowVecSize = 128 / DFS.ShadowWidth;
  uint64_t Offset = 0;
  if (Size >= ShadowVecSize) {
    VectorType *ShadowVecTy = VectorType::get(DFS.ShadowTy, ShadowVecSize);
    Value *ShadowVec = UndefValue::get(ShadowVecTy);
    for (unsigned i = 0; i != ShadowVecSize; ++i) {
      ShadowVec = IRB.CreateInsertElement(
          ShadowVec, Shadow, ConstantInt::get(Type::getInt32Ty(*DFS.Ctx), i));
    }
    Value *ShadowVecAddr =
        IRB.CreateBitCast(ShadowAddr, PointerType::getUnqual(ShadowVecTy));
    do {
      Value *CurShadowVecAddr =
          IRB.CreateConstGEP1_32(ShadowVecTy, ShadowVecAddr, Offset);
      IRB.CreateAlignedStore(ShadowVec, CurShadowVecAddr, ShadowAlign);
      Size -= ShadowVecSize;
      ++Offset;
    } while (Size >= ShadowVecSize);
    Offset *= ShadowVecSize;
  }
  while (Size > 0) {
    Value *CurShadowAddr =
        IRB.CreateConstGEP1_32(DFS.ShadowTy, ShadowAddr, Offset);
    IRB.CreateAlignedStore(Shadow, CurShadowAddr, ShadowAlign);
    --Size;
    ++Offset;
  }
}

void DFSanVisitor::visitStoreInst(StoreInst &SI) {
  auto &DL = SI.getModule()->getDataLayout();
  uint64_t Size = DL.getTypeStoreSize(SI.getValueOperand()->getType());
  if (Size == 0)
    return;

  uint64_t Align;
  if (ClPreserveAlignment) {
    Align = SI.getAlignment();
    if (Align == 0)
      Align = DL.getABITypeAlignment(SI.getValueOperand()->getType());
  } else {
    Align = 1;
  }

  Value* Shadow = DFSF.getShadow(SI.getValueOperand());
  if (ClCombinePointerLabelsOnStore) {
    Value *PtrShadow = DFSF.getShadow(SI.getPointerOperand());
    Shadow = DFSF.combineShadows(Shadow, PtrShadow, &SI);
  }
  DFSF.storeShadow(SI.getPointerOperand(), Size, Align, Shadow, &SI);
}

void DFSanVisitor::visitBinaryOperator(BinaryOperator &BO) {

  Value *Shadow0 = DFSF.getShadow(BO.getOperand(0));
  Value *Shadow1 = DFSF.getShadow(BO.getOperand(1));
  Value *FinalShadow;   
  Value *x1, *x2;
  x1 = BO.getOperand(0);
  x2 = BO.getOperand(1);


  FinalShadow = DFSF.combineDerivShadows(Shadow0, Shadow1, &BO, x1, x2);

  DFSF.setShadow(&BO, FinalShadow);
}

void DFSanVisitor::visitBranchInst(BranchInst &I) {
  std::string location = std::string("UNKNOWN");
  if (DILocation *Loc = I.getDebugLoc().get()) {
    location = std::string(Loc->getFilename().data()) + std::string(":") + std::to_string(Loc->getLine());
  }

  if (I.isConditional()) {
    if (auto *CI = dyn_cast<CmpInst>(I.getCondition())) {
      Value* lhs = CI->getOperand(0), *rhs = 0;
      Value* lhs_shadow = DFSF.getShadow(lhs);
      Value* rhs_shadow = DFSF.DFS.ZeroShadow;
      if (CI->getNumOperands() == 2) {
        rhs = CI->getOperand(1);
        rhs_shadow = DFSF.getShadow(rhs);
      } else {
        errs() << "branch error: " << I << " " << *CI << " " << location << "\n";
        errs() << "branch unsupported " << CI->getNumOperands() << " operands\n";
        report_fatal_error("Invalid number branch operands (not 2)");
      }

      CmpInst::Predicate pred = CI->getPredicate();

      DFSF.recordBranchInst(I, lhs_shadow, rhs_shadow, lhs, rhs, pred, location);

    } else {
      errs() << "branch Unsupported no cmp instruction in branch, condition is " << *I.getCondition() <<
             " " <<
             *I.getType()
             << " "<< location <<"\n";
    }

  } else {
  }
}

void DFSanVisitor::visitSwitchInst(SwitchInst &I) {

}

void DFSanVisitor::visitCastInst(CastInst &CI) { visitOperandShadowInst(CI); }

void DFSanVisitor::visitCmpInst(CmpInst &CI) { visitOperandShadowInst(CI); }

void DFSanVisitor::visitGetElementPtrInst(GetElementPtrInst &GEPI) {
  visitOperandShadowInst(GEPI);
}

void DFSanVisitor::visitExtractElementInst(ExtractElementInst &I) {
  visitOperandShadowInst(I);
}

void DFSanVisitor::visitInsertElementInst(InsertElementInst &I) {
  visitOperandShadowInst(I);
}

void DFSanVisitor::visitShuffleVectorInst(ShuffleVectorInst &I) {
  visitOperandShadowInst(I);
}

void DFSanVisitor::visitExtractValueInst(ExtractValueInst &I) {
  visitOperandShadowInst(I);
}

void DFSanVisitor::visitInsertValueInst(InsertValueInst &I) {
  visitOperandShadowInst(I);
}

void DFSanVisitor::visitAllocaInst(AllocaInst &I) {
  bool AllLoadsStores = true;
  for (User *U : I.users()) {
    if (isa<LoadInst>(U))
      continue;

    if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
      if (SI->getPointerOperand() == &I)
        continue;
    }

    AllLoadsStores = false;
    break;
  }
  if (AllLoadsStores) {
    IRBuilder<> IRB(&I);
    DFSF.AllocaShadowMap[&I] = IRB.CreateAlloca(DFSF.DFS.ShadowTy);
  }
  DFSF.setShadow(&I, DFSF.DFS.ZeroShadow);
}

void DFSanVisitor::visitSelectInst(SelectInst &I) {
  Value *CondShadow = DFSF.getShadow(I.getCondition());
  Value *TrueShadow = DFSF.getShadow(I.getTrueValue());
  Value *FalseShadow = DFSF.getShadow(I.getFalseValue());

  if (isa<VectorType>(I.getCondition()->getType())) {
    DFSF.setShadow(
        &I,
        DFSF.combineShadows(
            CondShadow, DFSF.combineShadows(TrueShadow, FalseShadow, &I), &I));
  } else {
    Value *ShadowSel;
    if (TrueShadow == FalseShadow) {
      ShadowSel = TrueShadow;
    } else {
      ShadowSel =
          SelectInst::Create(I.getCondition(), TrueShadow, FalseShadow, "", &I);
    }
    DFSF.setShadow(&I, DFSF.combineShadows(CondShadow, ShadowSel, &I));
  }
}

void DFSanVisitor::visitMemSetInst(MemSetInst &I) {
  IRBuilder<> IRB(&I);
  Value *ValShadow = DFSF.getShadow(I.getValue());
  IRB.CreateCall(DFSF.DFS.DFSanSetLabelFn,
                 {ValShadow, IRB.CreateBitCast(I.getDest(), Type::getInt8PtrTy(
                                                                *DFSF.DFS.Ctx)),
                  IRB.CreateZExtOrTrunc(I.getLength(), DFSF.DFS.IntptrTy)});
}

void DFSanFunction::memCpy(MemTransferInst &I, Value* src, Value* dst, Value* n,
        Value* srcShadow, Value* dstShadow, Value* nShadow) {
  IRBuilder<> IRB(&I);

  std::string location = std::string("UNKNOWN");
  if (DILocation *Loc = I.getDebugLoc().get()) {
    location = std::string(Loc->getFilename().data()) + std::string(":") + std::to_string(Loc->getLine());
  }

  Value *srcCast =
          IRB.CreateBitCast(src, DFS.VoidPtrTy);
  Value *dstCast =
          IRB.CreateBitCast(dst, DFS.VoidPtrTy);

  if (auto *type = dyn_cast<IntegerType>(n->getType())) {
    if (type->getBitWidth() < 64) {
      n = IRB.CreateZExt(n, DFS.Int64Ty);
    }
  }


  CallInst *CustomCI = IRB.CreateCall(DFS.MemCpyFn, {dstCast, srcCast, n, srcShadow, dstShadow, nShadow,
                                                     IRB.CreateGlobalStringPtr(StringRef(location))});

  I.replaceAllUsesWith(CustomCI);
  I.eraseFromParent();
}

void DFSanVisitor::visitMemTransferInst(MemTransferInst &I) {
  Value *src, *dst, *n, *srcShadow, *dstShadow, *nShadow;

  src = I.getSource();
  dst = I.getDest();
  n = I.getLength();
  srcShadow = DFSF.getShadow(src);
  dstShadow = DFSF.getShadow(dst);
  nShadow = DFSF.getShadow(n);

  DFSF.memCpy(I, src, dst, n, srcShadow, dstShadow, nShadow);

}


void DFSanVisitor::visitReturnInst(ReturnInst &RI) {
  if (!DFSF.IsNativeABI && RI.getReturnValue()) {
    switch (DFSF.IA) {
    case DataFlowSanitizer::IA_TLS: {
      Value *S = DFSF.getShadow(RI.getReturnValue());
      IRBuilder<> IRB(&RI);
      IRB.CreateStore(S, DFSF.getRetvalTLS());
      break;
    }
    case DataFlowSanitizer::IA_Args: {
      IRBuilder<> IRB(&RI);
      Type *RT = DFSF.F->getFunctionType()->getReturnType();
      Value *InsVal =
          IRB.CreateInsertValue(UndefValue::get(RT), RI.getReturnValue(), 0);
      Value *InsShadow =
          IRB.CreateInsertValue(InsVal, DFSF.getShadow(RI.getReturnValue()), 1);
      RI.setOperand(0, InsShadow);
      break;
    }
    }
  }
}

void DFSanVisitor::visitCallSite(CallSite CS) {
  Function *F = CS.getCalledFunction();

  if ((F && F->isIntrinsic()) || isa<InlineAsm>(CS.getCalledValue())) {
    visitOperandShadowInst(*CS.getInstruction());
    return;
  }

  // Calls to this function are synthesized in wrappers, and we shouldn't
  // instrument them.
  if (F == DFSF.DFS.DFSanVarargWrapperFn) {
    return;
  }

  IRBuilder<> IRB(CS.getInstruction());

  DenseMap<Value *, Function *>::iterator i =
      DFSF.DFS.UnwrappedFnMap.find(CS.getCalledValue());
  if (i != DFSF.DFS.UnwrappedFnMap.end()) {
    Function *F = i->second;
    switch (DFSF.DFS.getWrapperKind(F)) {
    case DataFlowSanitizer::WK_Warning:
      CS.setCalledFunction(F);
      IRB.CreateCall(DFSF.DFS.DFSanUnimplementedFn,
                     IRB.CreateGlobalStringPtr(F->getName()));
      DFSF.setShadow(CS.getInstruction(), DFSF.DFS.ZeroShadow);
      return;
    case DataFlowSanitizer::WK_Discard:
        CS.setCalledFunction(F);
      DFSF.setShadow(CS.getInstruction(), DFSF.DFS.ZeroShadow);
      return;
    case DataFlowSanitizer::WK_Functional:
        CS.setCalledFunction(F);
      visitOperandShadowInst(*CS.getInstruction());
      return;
    case DataFlowSanitizer::WK_Custom:
        // Don't try to handle invokes of custom functions, it's too complicated.
      // Instead, invoke the dfsw$ wrapper, which will in turn call the __dfsw_
      // wrapper.
      if (CallInst *CI = dyn_cast<CallInst>(CS.getInstruction())) {
        FunctionType *FT = F->getFunctionType();
        TransformedFunction CustomFn = DFSF.DFS.getCustomFunctionType(FT);
        std::string CustomFName = "__dfsw_";
        CustomFName += F->getName();
        Constant *CustomF = DFSF.DFS.Mod->getOrInsertFunction(
            CustomFName, CustomFn.TransformedType);
        if (Function *CustomFn = dyn_cast<Function>(CustomF)) {
          CustomFn->copyAttributesFrom(F);

          // Custom functions returning non-void will write to the return label.
          if (!FT->getReturnType()->isVoidTy()) {
            CustomFn->removeAttributes(AttributeList::FunctionIndex,
                                       DFSF.DFS.ReadOnlyNoneAttrs);
          }
        }

        std::vector<Value *> Args;

        CallSite::arg_iterator i = CS.arg_begin();
        for (unsigned n = FT->getNumParams(); n != 0; ++i, --n) {
          Type *T = (*i)->getType();
          FunctionType *ParamFT;
          if (isa<PointerType>(T) &&
              (ParamFT = dyn_cast<FunctionType>(
                   cast<PointerType>(T)->getElementType()))) {
            std::string TName = "dfst";
            TName += utostr(FT->getNumParams() - n);
            TName += "$";
            TName += F->getName();
            Constant *T = DFSF.DFS.getOrBuildTrampolineFunction(ParamFT, TName);
            Args.push_back(T);
            Args.push_back(
                IRB.CreateBitCast(*i, Type::getInt8PtrTy(*DFSF.DFS.Ctx)));
          } else {
            Args.push_back(*i);
          }
        }

        i = CS.arg_begin();
        const unsigned ShadowArgStart = Args.size();
        for (unsigned n = FT->getNumParams(); n != 0; ++i, --n)
          Args.push_back(DFSF.getShadow(*i));

        if (FT->isVarArg()) {
          errs() << "WARNING: custom vararg may break instrumentation: " << F->getName() << "\n";

          auto *LabelVATy = ArrayType::get(DFSF.DFS.ShadowTy,
                                           CS.arg_size() - FT->getNumParams());
          auto *LabelVAAlloca = new AllocaInst(
              LabelVATy, getDataLayout().getAllocaAddrSpace(),
              "labelva", &DFSF.F->getEntryBlock().front());

          for (unsigned n = 0; i != CS.arg_end(); ++i, ++n) {
            auto LabelVAPtr = IRB.CreateStructGEP(LabelVATy, LabelVAAlloca, n);
            IRB.CreateStore(DFSF.getShadow(*i), LabelVAPtr);
          }

          Args.push_back(IRB.CreateStructGEP(LabelVATy, LabelVAAlloca, 0));
        }

        if (!FT->getReturnType()->isVoidTy()) {
          if (!DFSF.LabelReturnAlloca) {
            DFSF.LabelReturnAlloca =
              new AllocaInst(DFSF.DFS.ShadowTy,
                             getDataLayout().getAllocaAddrSpace(),
                             "labelreturn", &DFSF.F->getEntryBlock().front());
          }
          Args.push_back(DFSF.LabelReturnAlloca);
        }

        for (i = CS.arg_begin() + FT->getNumParams(); i != CS.arg_end(); ++i)
          Args.push_back(*i);

        CallInst *CustomCI = IRB.CreateCall(CustomF, Args);
        CustomCI->setCallingConv(CI->getCallingConv());
        CustomCI->setAttributes(TransformFunctionAttributes(CustomFn,
            CI->getContext(), CI->getAttributes()));

        // Update the parameter attributes of the custom call instruction to
        // zero extend the shadow parameters. This is required for targets
        // which consider ShadowTy an illegal type.
        for (unsigned n = 0; n < FT->getNumParams(); n++) {
          const unsigned ArgNo = ShadowArgStart + n;
          if (CustomCI->getArgOperand(ArgNo)->getType() == DFSF.DFS.ShadowTy)
            CustomCI->addParamAttr(ArgNo, Attribute::ZExt);
        }

        if (!FT->getReturnType()->isVoidTy()) {
          LoadInst *LabelLoad = IRB.CreateLoad(DFSF.LabelReturnAlloca);
          DFSF.setShadow(CustomCI, LabelLoad);
        }

        CI->replaceAllUsesWith(CustomCI);
        CI->eraseFromParent();
        return;
      }
      break;
    }
  }

  FunctionType *FT = cast<FunctionType>(
      CS.getCalledValue()->getType()->getPointerElementType());
  if (DFSF.DFS.getInstrumentedABI() == DataFlowSanitizer::IA_TLS) {
    for (unsigned i = 0, n = FT->getNumParams(); i != n; ++i) {
      IRB.CreateStore(DFSF.getShadow(CS.getArgument(i)),
                      DFSF.getArgTLS(i, CS.getInstruction()));
    }
  }

  Instruction *Next = nullptr;
  if (!CS.getType()->isVoidTy()) {
    if (InvokeInst *II = dyn_cast<InvokeInst>(CS.getInstruction())) {
      if (II->getNormalDest()->getSinglePredecessor()) {
        Next = &II->getNormalDest()->front();
      } else {
        BasicBlock *NewBB =
            SplitEdge(II->getParent(), II->getNormalDest(), &DFSF.DT);
        Next = &NewBB->front();
      }
    } else {
      assert(CS->getIterator() != CS->getParent()->end());
      Next = CS->getNextNode();
    }

    if (DFSF.DFS.getInstrumentedABI() == DataFlowSanitizer::IA_TLS) {
      IRBuilder<> NextIRB(Next);
      LoadInst *LI = NextIRB.CreateLoad(DFSF.getRetvalTLS());
      DFSF.SkipInsts.insert(LI);
      DFSF.setShadow(CS.getInstruction(), LI);
      DFSF.NonZeroChecks.push_back(LI);
    }
  }

  // Do all instrumentation for IA_Args down here to defer tampering with the
  // CFG in a way that SplitEdge may be able to detect.
  if (DFSF.DFS.getInstrumentedABI() == DataFlowSanitizer::IA_Args) {
    FunctionType *NewFT = DFSF.DFS.getArgsFunctionType(FT);
    Value *Func =
        IRB.CreateBitCast(CS.getCalledValue(), PointerType::getUnqual(NewFT));
    std::vector<Value *> Args;

    CallSite::arg_iterator i = CS.arg_begin(), e = CS.arg_end();
    for (unsigned n = FT->getNumParams(); n != 0; ++i, --n)
      Args.push_back(*i);

    i = CS.arg_begin();
    for (unsigned n = FT->getNumParams(); n != 0; ++i, --n)
      Args.push_back(DFSF.getShadow(*i));

    if (FT->isVarArg()) {
      unsigned VarArgSize = CS.arg_size() - FT->getNumParams();
      ArrayType *VarArgArrayTy = ArrayType::get(DFSF.DFS.ShadowTy, VarArgSize);
      AllocaInst *VarArgShadow =
        new AllocaInst(VarArgArrayTy, getDataLayout().getAllocaAddrSpace(),
                       "", &DFSF.F->getEntryBlock().front());
      Args.push_back(IRB.CreateConstGEP2_32(VarArgArrayTy, VarArgShadow, 0, 0));
      for (unsigned n = 0; i != e; ++i, ++n) {
        IRB.CreateStore(
            DFSF.getShadow(*i),
            IRB.CreateConstGEP2_32(VarArgArrayTy, VarArgShadow, 0, n));
        Args.push_back(*i);
      }
    }

    CallSite NewCS;
    if (InvokeInst *II = dyn_cast<InvokeInst>(CS.getInstruction())) {
      NewCS = IRB.CreateInvoke(Func, II->getNormalDest(), II->getUnwindDest(),
                               Args);
    } else {
      NewCS = IRB.CreateCall(Func, Args);
    }
    NewCS.setCallingConv(CS.getCallingConv());
    NewCS.setAttributes(CS.getAttributes().removeAttributes(
        *DFSF.DFS.Ctx, AttributeList::ReturnIndex,
        AttributeFuncs::typeIncompatible(NewCS.getInstruction()->getType())));

    if (Next) {
      ExtractValueInst *ExVal =
          ExtractValueInst::Create(NewCS.getInstruction(), 0, "", Next);
      DFSF.SkipInsts.insert(ExVal);
      ExtractValueInst *ExShadow =
          ExtractValueInst::Create(NewCS.getInstruction(), 1, "", Next);
      DFSF.SkipInsts.insert(ExShadow);
      DFSF.setShadow(ExVal, ExShadow);
      DFSF.NonZeroChecks.push_back(ExShadow);

      CS.getInstruction()->replaceAllUsesWith(ExVal);
    }

    CS.getInstruction()->eraseFromParent();
  }
}

void DFSanVisitor::visitPHINode(PHINode &PN) {
  PHINode *ShadowPN =
      PHINode::Create(DFSF.DFS.ShadowTy, PN.getNumIncomingValues(), "", &PN);

  // Give the shadow phi node valid predecessors to fool SplitEdge into working.
  Value *UndefShadow = UndefValue::get(DFSF.DFS.ShadowTy);
  for (PHINode::block_iterator i = PN.block_begin(), e = PN.block_end(); i != e;
       ++i) {
    ShadowPN->addIncoming(UndefShadow, *i);
  }

  DFSF.PHIFixups.push_back(std::make_pair(&PN, ShadowPN));
  DFSF.setShadow(&PN, ShadowPN);
}
