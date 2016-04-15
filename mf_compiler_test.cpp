#include <llvm/Pass.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Constants.h> 
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Type.h>
#include <llvm/Bitcode/BitcodeWriterPass.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/CodeGen/MachineFunction.h>
#include <llvm/CodeGen/AsmPrinter.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetLoweringObjectFile.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/SystemUtils.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/Transforms/Utils/CodeExtractor.h>
#include <llvm/CodeGen/MachineFunction.h>
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/CodeGen/MIRParser/MIRParser.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include <llvm/Transforms/Utils/CodeExtractor.h>
#include "llvm/Support/TargetSelect.h"

// TODO cleanup includes

#include <string>

#include "mf_compiler.h"
#include "mf_instrument.h"

using namespace llvm;

int main()
{
  InitializeAllTargets();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();

  std::string TestFile = "add.bc";
  std::string FnName = "add";

  // create a template module from user's testcase
  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err; 
  std::unique_ptr<Module> M = parseIRFile(TestFile, Err, Context);

  legacy::PassManager PM;
  std::vector<GlobalValue *> ToKeep { M->getFunction(FnName) };
  PM.add(createGVExtractionPass(ToKeep, false));
  PM.run(*M);

  ////////////////
  Triple TheTriple = Triple(M->getTargetTriple());

  if (TheTriple.getTriple().empty())
    TheTriple.setTriple(sys::getDefaultTargetTriple());

  // create target machine
  TargetOptions Options = InitTargetOptionsFromCodeGenFlags();
  std::string CPUStr = getCPUStr(), FeaturesStr = getFeaturesStr();
  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget(TheTriple.str(), Error);
  assert(TheTarget);
  std::unique_ptr<TargetMachine> TM(
    TheTarget->createTargetMachine(TheTriple.getTriple(), CPUStr, FeaturesStr,
                                   Options, Reloc::PIC_));
  ////////////////////////

  MachineModuleInfo *MMI = new MachineModuleInfo(
    *TM->getMCAsmInfo(), *TM->getMCRegisterInfo(), TM->getObjFileLowering());


  // create a machine function
  MachineFunction MF(M->getFunction(FnName), *TM, 0, *MMI);
  auto MBB = MF.CreateMachineBasicBlock();
  auto Instrumenter = getInstrumenter(TM.get());
  MF.push_back(MBB);
  //std::vector<unsigned> Regs{ 35, 36 };
  std::vector<unsigned> Regs{ Instrumenter->getRegister("ESI") };
  Instrumenter->dumpRegisters(*M, *MBB, Regs);
  Instrumenter->instrumentToReturn(MF, 0x1073031a0);

  compileToObjectFile(*M, MF, "x.o", TM.get(), false);
  emitDumpRegistersModule(TM.get(), Regs, "server_dump_regs.o");
}