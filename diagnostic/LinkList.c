#include "Cpu.h"
#include "LinkList.h"

void AddNode(node* header , node* listNode)
{
	node* temp = header;
	while(temp->next != NULL)
	{
		temp = temp->next;
	}
	temp->next = listNode;
	temp->next->next = NULL;
}

void DeleteNode(node* header , int index)
{
	uint32_t i;
	node* temp = header;
	node* prevNode;
	for(i = 0; i < index ; i++)
	{
		prevNode = temp;
		temp = temp->next;
	}
	prevNode->next = temp->next;
	free(temp);
}

void* GetNodeData(node* header , int index)//header->next is 0 index,header not mean only index
{
	uint32_t i;
	node* temp = header;
	for(i = 0; i < index ; i++)
	{
		temp = temp->next;
	}
	return temp->dataPoint;
}


