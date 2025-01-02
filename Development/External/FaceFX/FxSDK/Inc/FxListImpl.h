//------------------------------------------------------------------------------
// This class implements a circular doubly-linked list.
//
// Owner: John Briggs
//
// Copyright (c) 2002-2010 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#ifndef FxListImpl_H__
#define FxListImpl_H__

namespace OC3Ent
{

namespace Face
{

//------------------------------------------------------------------------------
// FxList.
//------------------------------------------------------------------------------

// Handy macro for checking head node validity.
#define FX_LIST_CHECK_HEAD() \
	if( !_head ) \
	{ \
		_head = static_cast<FxListNode*>(FxAlloc(sizeof(FxListNode), "ListHead")); \
		FxDefaultConstruct(_head); \
		_head->next = _head; \
		_head->prev = _head; \
	}

// Constructor.
template<typename FxListElem>
FxList<FxListElem>::FxList()
	: _head(NULL)
	, _size(0)
{
}

template<typename FxListElem>
FxList<FxListElem>::FxList( const FxList<FxListElem>& other )
	: _head(0)
	, _size(0)
{
	_head = static_cast<FxListNode*>(FxAlloc(sizeof(FxListNode), "ListHead"));
	FxDefaultConstruct(_head);
	_head->next = _head;
	_head->prev = _head;

	ConstIterator curr = other.Begin();
	ConstIterator end  = other.End();
	for( ; curr != end; ++curr )
	{
		PushBack(*curr);
	}
}

template<typename FxListElem>
FxList<FxListElem>& 
FxList<FxListElem>::operator=( const FxList<FxListElem>& other )
{
	if( _size )
	{
		Clear();
	}

	ConstIterator curr = other.Begin();
	ConstIterator end  = other.End();
	for( ; curr != end; ++curr )
	{
		PushBack(*curr);
	}

	return *this;
}

// Destructor.
template<typename FxListElem>
FxList<FxListElem>::~FxList()
{
	Clear();
	if( _head )
	{
		FxDelete(_head);
	}
}

template<typename FxListElem> FX_INLINE
typename FxList<FxListElem>::Iterator 
FxList<FxListElem>::Begin( void )
{
	FX_LIST_CHECK_HEAD();
	return ++Iterator(_head);
}

template<typename FxListElem> FX_INLINE
typename FxList<FxListElem>::ConstIterator 
FxList<FxListElem>::Begin( void ) const
{
	FX_LIST_CHECK_HEAD();
	return ++ConstIterator(_head);
}

template<typename FxListElem> FX_INLINE
typename FxList<FxListElem>::Iterator 
FxList<FxListElem>::End( void )
{
	FX_LIST_CHECK_HEAD();
	return Iterator(_head);
}

template<typename FxListElem> FX_INLINE
typename FxList<FxListElem>::ConstIterator 
FxList<FxListElem>::End( void ) const
{
	FX_LIST_CHECK_HEAD();
	return ConstIterator(_head);
}

template<typename FxListElem>
typename FxList<FxListElem>::Iterator 
FxList<FxListElem>::Find( const FxListElem& toFind )
{
	Iterator finder = Begin();
	Iterator end    = End();
	for( ; finder != end; ++finder )
	{
		if( *finder == toFind ) break;
	}
	return finder;
}

template<typename FxListElem>
typename FxList<FxListElem>::ConstIterator 
FxList<FxListElem>::Find( const FxListElem& toFind ) const
{
	ConstIterator finder = Begin();
	ConstIterator end    = End();
	for( ; finder != end; ++finder )
	{
		if( *finder == toFind ) break;
	}
	return finder;
}

template<typename FxListElem> FX_INLINE
typename FxList<FxListElem>::ReverseIterator
FxList<FxListElem>::ReverseBegin( void ) const
{
	FX_LIST_CHECK_HEAD();
	return ++ReverseIterator(_head);
}

template<typename FxListElem> FX_INLINE
typename FxList<FxListElem>::ReverseIterator
FxList<FxListElem>::ReverseEnd( void ) const
{
	FX_LIST_CHECK_HEAD();
	return ReverseIterator(_head);
}

template<typename FxListElem>
void
FxList<FxListElem>::Clear( void )
{
	if( _head )
	{
		FxListNode* pNode = _head->next;
		while( pNode != _head )
		{
			FxListNode* temp = pNode;
			pNode = pNode->next;
			FxDelete(temp);
		}
		FxDelete(_head);
		_size = 0;
	}
}

template<typename FxListElem> FX_INLINE
FxBool
FxList<FxListElem>::IsEmpty( void ) const
{
	return _size == 0;
}

template<typename FxListElem> FX_INLINE
FxSize
FxList<FxListElem>::Length( void ) const
{
	return _size;
}

template<typename FxListElem> FX_INLINE
FxSize
FxList<FxListElem>::Allocated( void ) const
{
	return (_size + 1) * (sizeof(FxListElem) + 2 * sizeof(FxListNode*)) 
		   + sizeof(_size);
}

template<typename FxListElem> FX_INLINE
FxListElem& 
FxList<FxListElem>::Back( void )
{
	return *--End();
}

template<typename FxListElem> FX_INLINE
const FxListElem& 
FxList<FxListElem>::Back( void ) const
{
	return *--End();
}

template<typename FxListElem>
void 
FxList<FxListElem>::PushBack( const FxListElem& element )
{
	Insert(element, End());
}

template<typename FxListElem>
void 
FxList<FxListElem>::PopBack( void )
{
	RemoveIterator(--End());
}

template<typename FxListElem> FX_INLINE
FxListElem& 
FxList<FxListElem>::Front( void )
{
	return *Begin();
}

template<typename FxListElem> FX_INLINE
const FxListElem& 
FxList<FxListElem>::Front( void ) const
{
	return *Begin();
}

template<typename FxListElem>
void 
FxList<FxListElem>::PushFront( const FxListElem& element )
{
	Insert(element, Begin());
}

template<typename FxListElem>
void 
FxList<FxListElem>::PopFront( void )
{
	RemoveIterator(Begin());
}

template<typename FxListElem>
typename FxList<FxListElem>::Iterator
FxList<FxListElem>::Insert( const FxListElem& element, Iterator iter )
{
	FxListNode* insertNode = static_cast<FxListNode*>(FxAlloc(sizeof(FxListNode), "List Node"));
	FxConstruct(insertNode, FxListNode(element, iter.GetNode(), iter.GetNode()->prev));
	insertNode->prev->next = insertNode;
	insertNode->next->prev = insertNode;
	++_size;
	return Iterator(insertNode);
}

template<typename FxListElem>
void
FxList<FxListElem>::Remove( Iterator iter )
{
	RemoveIterator(iter);
}

template<typename FxListElem>
void
FxList<FxListElem>::RemoveIterator( Iterator iter )
{
	FxListNode* toRemove = iter.GetNode();
	FxAssert( toRemove != _head );
	if( toRemove != _head )
	{
		toRemove->prev->next = toRemove->next;
		toRemove->next->prev = toRemove->prev;
		FxDelete(toRemove);
		--_size;
	}
}

template<typename FxListElem>
void FxList<FxListElem>::RemoveIfEqual( const FxListElem& element )
{
	if( _head )
	{
		FxListNode* curr = _head->next;
		while( curr != _head )
		{
			if( curr->element == element )
			{
				FxListNode* temp = curr;
				curr = curr->next;

				temp->prev->next = temp->next;
				temp->next->prev = temp->prev;
				FxDelete(temp);
				--_size;
			}
			else
			{
				curr = curr->next;
			}
		}
	}
}

template<typename FxListElem>
void
FxList<FxListElem>::Sort( void )
{
	if( !IsEmpty() && _head )
	{
		// Initially set the merged list's head to the current 'real' head.
		FxListNode* mergedListHead = _head->next;
		FxListNode* mergedListTail = 0;
		
		FxListNode* leftList = 0;
		FxSize leftListLength = 0;
		
		FxListNode* rightList = 0;
		FxSize rightListLength = 0;
		
		FxSize numMerges = 0;
		
		FxSize initialSubListLength = 1;
		
		// Detach the head node and make the list non-circular in
		// preparation for an in-place merge sort.
		if( _head->next )
		{
			_head->next->prev = 0;
		}
		
		if( _head->prev )
		{
			_head->prev->next = 0;
		}
		
		_head->next = 0;
		_head->prev = 0;
		
		// In-place merge sort algorithm:
		//
		// In each pass, merge lists of size initialSubListLength into lists of size 2*initialSubListLength
		// (initialSubListLength starts at 1). Start by pointing rightList at the head of the list and
		// prepare the empty mergedListHead to hold 'output' nodes. Then:
		//
		// * If leftList is null, the pass is terminated.
		//
		// * Else there is at least one node in the next pair of initialSubListLength lists, so increment the
		//   number of merges performed this pass.
		//
		// * Point rightList at leftList. Step rightList along by initialSubListLength nodes, or until the end
		//   of the list, whichever comes first. Set leftListLength to the number of nodes passed when stepping
		//   rightList along.
		//
		// * Set rightListLength to initialSubListLength. Now merge leftList with rightList (rightList will be
		//   at most rightListLength in length since it could have stepped off the end of the list).
		//
		// * As long as leftList is non-empty (leftListLength > 0) or rightList is non-empty
		//   (rightList != 0 && rightListLength > 0):
		//
		//     * Determine which list the next node comes from. If either list is empty, take a node from the
		//       other one (at least one of the lists should be non-empty). If both lists are non-empty,
		//       compare the first node in each and take the lowest valued one. If the values in the first
		//       node of each list are equal, pick the node from leftList (this makes sure that the sort is
		//       stable).
		//
		//     * Remove selectedNode from the start of its list by advancing that list's head node to the
		//       next element and decrement the corresponding length.
		//
		//     * Push selectedNode to the end of the list defined by mergedListHead.
		//
		// * Now leftList is where rightList started and rightList is pointing at the next pair of initialSubListLength
		//   lists to merge. Point leftList at rightList and go back to the start of the loop.
		//
		// When a pass is completed that only did one merge the algorithm is complete (the list defined by mergeListHead
		// is sorted). Otherwise, double initialSubListLength and start over.

		while( true )
		{
			// Set up for the next pass.
			leftList = mergedListHead;
			mergedListHead = 0;
			mergedListTail = 0;
			numMerges = 0;
			
			// The pass keeps going while leftList is non-null.
			while( leftList )
			{
				// There is at least one node in the next pair of initialSubListLength lists, so increment
				// the number of merges performed this pass.
				++numMerges;
				
				// Point rightList at leftList.
				rightList = leftList;
				
				// Step rightList along by initialSubListLength nodes, or until the end of the list, whichever
				// comes first. Set leftListLength to the number of nodes passed when stepping rightList along.
				leftListLength = 0;
				
				while( rightList && leftListLength < initialSubListLength )
				{
					++leftListLength;
					rightList = rightList->next;
				}
				
				// Set rightListLength to intialSubListLength.
				rightListLength = initialSubListLength;
				
				// Now merge leftList with rightList. As long as leftList is non-empty or rightList is non-empty:
				while( leftListLength > 0 || (rightList && rightListLength > 0) )
				{
					// Determine which list to select a node from:
					FxListNode* selectedNode = 0;
					
					if( 0 == leftListLength )
					{
						// If leftList is empty we have to select the first node
						// in rightList.
						selectedNode = rightList;
						rightList = rightList->next;
						--rightListLength;
					}
					else if( 0 == rightListLength || !rightList )
					{
						// If rightList is empty we have to select the first node
						// in leftList.
						selectedNode = leftList;
						leftList = leftList->next;
						--leftListLength;
					}
					else if( leftList->element == rightList->element )
					{
						// If the first element of leftList is == the first element
						// of rightList we have to select the first node in leftList
						// to keep the sort stable.
						selectedNode = leftList;
						leftList = leftList->next;
						--leftListLength;
					}
					else if( leftList->element < rightList->element )
					{
						// Note: this wasn't written as <= because FxListElem may not
						// have an operator<=().
						
						// If the first element of leftList is < the first element
						// of rightList we have to select the first node in leftList.
						selectedNode = leftList;
						leftList = leftList->next;
						--leftListLength;
					}
					else
					{
						// If the first element of rightList is < the first element
						// of leftList we have to select the first node in rightList.
						selectedNode = rightList;
						rightList = rightList->next;
						--rightListLength;
					}
					
					// selectedNode to end of mergedList.
					FxAssert(selectedNode);
					
					if( mergedListTail )
					{
						mergedListTail->next = selectedNode;
					}
					else
					{
						mergedListHead = selectedNode;
					}

					selectedNode->prev = mergedListTail;
					mergedListTail = selectedNode;
				}
				
				// Now leftList is where rightList started and rightList is pointing at the next
				// pair of initialSubListLength lists to merge. Point leftList at rightList and go
				// back to the start of the loop.
				leftList = rightList;
			}
			
			// Clamp off the merged list so it isn't circular.
			mergedListTail->next = 0;
			
			// If a pass did zero or one merges (zero for the empty case), the algorithm is complete.
			if( numMerges <= 1 )
			{
				// Reattach the head node and make the list circular again.
				_head->next = mergedListHead;
				_head->prev = mergedListTail;
				
				if( mergedListHead )
				{
					mergedListHead->prev = _head;
				}
				
				if( mergedListTail )
				{
					mergedListTail->next = _head;
				}
				
				return;
			}
			
			// Otherwise, double initialSubListLength and start over.
			initialSubListLength *= 2;
		}
	}
}

} // namespace Face

} // namespace OC3Ent

#endif
