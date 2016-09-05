#ifndef LINK_LIST_H
#define LINK_LIST_H

#include "NetworkLayerTypeDefines.h"

#ifndef  node
typedef struct LinkList{
	void* dataPoint;
	//uint8_t index;
	struct LinkList* next;
	//struct LinkList* prev;
}node;
#endif

void AddNode(node* header , node* listNode);
void DeleteNode(node* header , int index);
void* GetNodeData(node* header , int index);

#endif
