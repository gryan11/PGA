#ifndef ANNOTATION_H
#define ANNOTATION_H

#ifndef ANNOTATION_NAMESPACE
#define ANNOTATION_NAMESPACE    "ANN"
#endif

#include "llvm/Pass.h"
#include "llvm/IR/InstIterator.h"

using namespace llvm;

class Annotation {
public:
  static uint64_t assignIDs(Module &M, std::vector<Instruction*> *elementsList,
                          std::string metadataNamespace,
                          std::string lastIDHolderSuffix="_LAST_ID_HOLDER",
                          uint64_t idStart=0, bool forceReset=true);
  static uint64_t getAssignedInstrID(Instruction *I,
                                     std::string metadataNamespace=ANNOTATION_NAMESPACE);
  static Constant* getStringConstantArray(Module &M, const std::string &string);
};

inline uint64_t Annotation::assignIDs(Module &M,
                          std::vector<Instruction*> *elementsList,
                          std::string metadataNamespace,
                          std::string lastIDHolderSuffix, uint64_t idStart, bool forceReset) {
    uint64_t lastAssignedId = idStart;

  // Initialize the enumeration process
  if (!forceReset) {
    uint64_t existingLastId;
    NamedMDNode *nmdn = M.getNamedMetadata(metadataNamespace + lastIDHolderSuffix);
    if ( nmdn == NULL || nmdn->getNumOperands() < 1 ) {
      existingLastId = 0;
    } else {
      MDNode *N = nmdn->getOperand(0);
      assert(NULL != N && N->getNumOperands() >= 1);
      ConstantInt *I = dyn_cast_or_null<ConstantInt>((
                          (ConstantAsMetadata *)((Metadata *)(N->getOperand(0))))->getValue());
      assert(NULL != I);
      existingLastId = I->getZExtValue();
    }
    if (0 != existingLastId)
      lastAssignedId = existingLastId;
  }

  MDNode *N;
  for (std::vector<Instruction*>::iterator IV=elementsList->begin(), EV=elementsList->end();
       IV != EV; IV++) {
    Instruction *targetInst = *IV;
    if (NULL == targetInst)
      continue;
    IntegerType *T = IntegerType::get(M.getContext(), 64);
    ConstantInt *I = ConstantInt::get(T, ++lastAssignedId);
    ArrayRef<Metadata *> *arrayRefMetadata = new ArrayRef<Metadata *>(ConstantAsMetadata::get(I));
    N = MDNode::get(M.getContext(), *arrayRefMetadata);
    assert(NULL != N);
    targetInst->setMetadata(metadataNamespace, N);
  }

  // save the lastAssignedId
  std::string lastIDHolderName = (std::string)metadataNamespace + (std::string)lastIDHolderSuffix;
  NamedMDNode *nmdn = M.getOrInsertNamedMetadata(lastIDHolderName);
  nmdn->dropAllReferences();
  nmdn->addOperand(N); // has the lastAssignedId MDNode
  return lastAssignedId;
}

inline uint64_t Annotation::getAssignedInstrID(Instruction *I,
                                                      std::string metadataNamespace) {
  assert(NULL != I && "Instruction argument cannot be NULL.");
  MDNode *N = I->getMetadata(metadataNamespace);
  if (NULL == N || (N->getNumOperands() < 1))
    return 0;
  ConstantInt *constInt = dyn_cast_or_null<ConstantInt>((
                          (ConstantAsMetadata *)((Metadata *)(N->getOperand(0))))->getValue());
  if (NULL == constInt)
    return 0;
  return constInt->getZExtValue();
}

inline Constant* Annotation::getStringConstantArray(Module &M, const std::string &string)
{
  std::vector<Constant*> elements;
  elements.reserve(string.size() + 1);
  for (unsigned i = 0; i < string.size(); ++i)
    elements.push_back(ConstantInt::get(Type::getInt8Ty(M.getContext()), string[i]));

  // Add a null terminator to the string...
  elements.push_back(ConstantInt::get(Type::getInt8Ty(M.getContext()), 0));

  ArrayType *ATy = ArrayType::get(Type::getInt8Ty(M.getContext()), elements.size());
  return ConstantArray::get(ATy, elements);
}

#endif
