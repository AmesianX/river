#ifndef _CODEGEN_H
#define _CODEGEN_H

#include "river.h"
#include "cb.h"
#include "runtime.h"

#include "RiverX86Disassembler.h"
#include "RiverX86Assembler.h"

#include "RiverReverseTranslator.h"
#include "RiverSaveTranslator.h"

#include "SymbopTranslator.h"

#define RIVER_TRANSLATE_INSTRUCTIONS				128
#define RIVER_FORWARD_INSTRUCTIONS					512
#define RIVER_BACKWARD_INSTRUCTIONS					512

#define SYMBOP_TRACK_INSTRUCTIONS					128

/* A resettable river code translator */
class RiverCodeGen {
private :
	RiverHeap *heap;
	
	RiverX86Disassembler disassembler;
	RiverX86Assembler assembler;

	RiverReverseTranslator revTranslator;
	RiverSaveTranslator saveTranslator;

	SymbopTranslator symbopTranslator;

	DWORD RiverCodeGen::TranslateBasicBlock(BYTE *px86, DWORD &dwInst);
public :
	//struct RiverInstruction trRiverInst[RIVER_TRANSLATE_INSTRUCTIONS]; // to be removed in the near future
	struct RiverInstruction fwRiverInst[RIVER_FORWARD_INSTRUCTIONS];
	struct RiverInstruction bkRiverInst[RIVER_BACKWARD_INSTRUCTIONS];
	struct RiverInstruction symbopInst[SYMBOP_TRACK_INSTRUCTIONS];

	struct RiverAddress32 trRiverAddr[512];
	
	DWORD trInstCount, fwInstCount, bkInstCount, addrCount, outBufferSize;
	DWORD symbopInstCount;

	unsigned char *outBuffer;
	unsigned int regVersions[8];
public :
	RiverCodeGen();
	~RiverCodeGen();

	bool Init(RiverHeap *hp, RiverRuntime *rt, DWORD buffSz);
	bool Destroy();
	void Reset();

	struct RiverAddress *AllocAddr(WORD flags);
	struct RiverAddress *CloneAddress(const RiverAddress &mem, WORD flags);

	unsigned int GetCurrentReg(unsigned char regName) const;
	unsigned int GetPrevReg(unsigned char regName) const;

	unsigned int NextReg(unsigned char regName);

	bool Translate(RiverBasicBlock *pCB, DWORD dwTranslationFlags);
};

#endif