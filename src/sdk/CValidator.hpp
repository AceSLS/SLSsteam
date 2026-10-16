#pragma once

#include "types.hpp"
#include <cstdint>


//TODO: Update for 64 bits
//SDK_Class CValNode
//{
//public:
//	//const char** typeName;				//0x0
//	//const char** name;					//0x4
//	const char* typeName;				//0x0
//	const char* name;					//0x4
//	void* pClassObject;					//0x8
//	CValNode* parent;					//0xC
//	CValNode* next;						//0x10
//	uint8_t __pad0x14[0x20];			//0x14
//}; //0x34
//
//SDK_Class CValidator
//{
//public:
//	CValNode* firstNode;				//0x0
//	CValNode* lastNode;					//0x4
//	CValNode* currentNode;				//0x8
//	uint8_t __pad0xC[0x4];				//0xC
//	CValNode* nodes;					//0x10
//	uint32_t allocatedNodeCount;		//0x14
//	uint32_t nodeCount;					//0x18
//	uint8_t __pad0x1C[0x848];			//0x1C
//}; //0x864
