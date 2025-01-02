#ifndef TestFxList_H__
#define TestFxList_H__

#include "FxList.h"
#include "FxArchive.h"
#include "FxArchiveStoreNull.h"
#include "FxArchiveStoreMemory.h"

#define DEBUG_LIST_SORT 0

#if DEBUG_LIST_SORT
	#include <iostream>
	using std::cout;
	using std::endl;
#endif // DEBUG_LIST_SORT

UNITTEST( FxList, DefaultConstructor )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	CHECK( test.Length() == 0 && test.IsEmpty() );

#if defined(_WIN64)
	CHECK( test.Allocated() == 24 );
#else // _WIN32
	CHECK( test.Allocated() == 16 );
#endif // defined(_WIN64)
}

UNITTEST( FxList, CopyConstructor )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.PushBack( 1 );
	test.PushBack( 2 );
	test.PushBack( 3 );

	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> copy( test );
	CHECK( copy.Length() == 3 );
	CHECK( copy.Front() == 1 && copy.Back() == 3 );

#if defined(_WIN64)
	CHECK( copy.Allocated() == 84 && copy.Allocated() == test.Allocated() ); 
#else // _WIN32
	CHECK( copy.Allocated() == 52 && copy.Allocated() == test.Allocated() ); 
#endif // defined(_WIN64)
}

UNITTEST( FxList, AssignmentOperator )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.PushBack( 1 );
	test.PushBack( 2 );
	test.PushBack( 3 );

	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> copy;
	copy = test;

	CHECK( copy.Length() == 3 );
	CHECK( copy.Front() == 1 && copy.Back() == 3 );
}

UNITTEST( FxList, PushFront )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.PushFront( 0 );
	CHECK( test.Length() == 1 );
	CHECK( *test.Begin() == 0 );
	CHECK( test.Front() == 0 );

	test.PushFront( 1 );
	CHECK( test.Length() == 2 );
	CHECK( *test.Begin() == 1 );
	CHECK( test.Front() == 1 );
}

UNITTEST( FxList, PushBack )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.PushBack( 0 );
	CHECK( test.Length() == 1 );
	CHECK( *test.Begin() == 0 );
	CHECK( test.Front() == 0 );

	test.PushBack( 1 );
	CHECK( test.Length() == 2 );
	CHECK( *++test.Begin() == 1 );
	CHECK( test.Front() == 0 );
}

UNITTEST( FxList, PopFront )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.PushFront( 2 );
	test.PushFront( 1 );
	test.PushFront( 0 );
	test.PopFront();
	CHECK( test.Front() == 1 );
	test.PopFront();
	CHECK( test.Front() == 2 );
	test.PopFront();
	CHECK( test.Length() == 0 );
}

UNITTEST( FxList, PopBack )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.PushFront( 2 );
	test.PushFront( 1 );
	test.PushFront( 0 );
	CHECK( *--test.End() == 2 );
	test.PopBack();
	CHECK( *--test.End() == 1 );
	test.PopBack();
	CHECK( *--test.End() == 0 );
	test.PopBack();
	CHECK( test.Length() == 0 );
}

UNITTEST( FxList, Insert )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.Insert( 0, test.Begin() );
	CHECK( *test.Begin() == 0 );
	test.Insert( 1, test.End() );
	CHECK( *--test.End() == 1 );
	test.Insert( 5, ++test.Begin() );
	CHECK( *++test.Begin() == 5 );
	CHECK( test.Length() == 3 );
}

UNITTEST( FxList, Remove )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.Insert( 0, test.End() );
	test.Insert( 1, test.End() );
	test.Insert( 2, test.End() );
	test.Insert( 3, test.End() );
	test.Remove( test.Begin() );
	CHECK( *test.Begin() == 1 );
	test.Remove( --test.End() );
	CHECK( *--test.End() == 2 );
	test.PushBack( 10 );
	test.Remove( ++test.Begin() );
	CHECK( *test.Begin() == 1 && *++test.Begin() == 10 && test.Length() == 2 );
	test.Remove( test.Begin() );
	CHECK( *test.Begin() == 10 && test.Length() == 1 );
	test.Remove( test.Begin() );
	CHECK( test.Length() == 0 );
}

UNITTEST( FxListIterator, Advance )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.Insert( 0, test.End() );
	test.Insert( 1, test.End() );
	test.Insert( 2, test.End() );
	test.Insert( 3, test.End() );
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32>::Iterator testIter;

	testIter = test.Begin();
	testIter.Advance( 0 );
	CHECK( *testIter == 0 );

	testIter.Advance( 2 );
	CHECK( *testIter == 2 );
	
}

UNITTEST( FxListReverseIterator, Functionality )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.Insert( 0, test.End() );
	test.Insert( 1, test.End() );
	test.Insert( 2, test.End() );
	test.Insert( 3, test.End() );

	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32>::ReverseIterator testIter = test.ReverseBegin();
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32>::ReverseIterator end = test.ReverseEnd();

	CHECK( *testIter == 3 );
	++testIter;
	CHECK( *testIter == 2 );
	testIter.Advance(2);
	CHECK( *testIter == 0 );
	--testIter;
	CHECK( *testIter == 1 );
	testIter.Advance(2);
	CHECK( testIter == end );
}

UNITTEST( FxList, Find )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	test.PushBack(0);
	test.PushBack(1);
	test.PushBack(2);
	test.PushBack(3);

	CHECK(*(test.Find(0))==0);
	CHECK(*(test.Find(1))==1);
	CHECK(*(test.Find(2))==2);
	CHECK(*(test.Find(3))==3);
	CHECK(test.Find(4)==test.End());
}

#if DEBUG_LIST_SORT
	#define PRINT_LIST(list, label) \
		{ \
			cout << label; \
			OC3Ent::Face::FxList<OC3Ent::Face::FxInt32>::Iterator iter = list.Begin(); \
			OC3Ent::Face::FxList<OC3Ent::Face::FxInt32>::Iterator iterEnd = list.End(); \
			for( ; iter != iterEnd; ++iter ) \
			{ \
				cout << *iter << " "; \
			} \
			cout << endl; \
		}
#else
	#define PRINT_LIST(list, label)
#endif // DEBUG_LIST_SORT

#define CHECK_LIST(list, length) \
	{ \
		CHECK(list.Length() == length); \
		OC3Ent::Face::FxList<OC3Ent::Face::FxInt32>::Iterator iter = list.Begin(); \
		OC3Ent::Face::FxList<OC3Ent::Face::FxInt32>::Iterator iterEnd = list.End(); \
		for( FxInt32 test = 0; iter != iterEnd; ++iter, ++test ) \
		{ \
			CHECK(test == *iter); \
		} \
	}

UNITTEST( FxList, Sort )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> test;
	
	PRINT_LIST(test, "Before Sort(): ");
	
	test.Sort();
	
	PRINT_LIST(test, "After Sort(): ");
	
	CHECK_LIST(test, 0);
	
	test.Clear();
	
	test.PushBack(0);
	
	PRINT_LIST(test, "Before Sort(): ");
	
	test.Sort();
	
	PRINT_LIST(test, "After Sort(): ");
	
	CHECK_LIST(test, 1);
	
	test.Clear();

	test.PushBack(0);
	test.PushBack(1);
	test.PushBack(2);
	test.PushBack(3);
	test.PushBack(4);
	test.PushBack(5);
	test.PushBack(6);
	test.PushBack(7);
	test.PushBack(8);
	test.PushBack(9);
	test.PushBack(10);
	test.PushBack(11);
	test.PushBack(12);
	
	PRINT_LIST(test, "Before Sort(): ");

	test.Sort();

	PRINT_LIST(test, "After Sort(): ");
	
	CHECK_LIST(test, 13);
	
	test.Clear();
	
	test.PushBack(6);
	test.PushBack(2);
	test.PushBack(8);
	test.PushBack(4);
	test.PushBack(11);
	test.PushBack(1);
	test.PushBack(12);
	test.PushBack(7);
	test.PushBack(3);
	test.PushBack(9);
	test.PushBack(5);
	test.PushBack(0);
	test.PushBack(10);
	
	PRINT_LIST(test, "Before Sort(): ");
	
	test.Sort();
	
	PRINT_LIST(test, "After Sort(): ");
	
	CHECK_LIST(test, 13);
	
	test.Clear();
	
	test.PushBack(12);
	test.PushBack(11);
	test.PushBack(10);
	test.PushBack(9);
	test.PushBack(8);
	test.PushBack(7);
	test.PushBack(6);
	test.PushBack(5);
	test.PushBack(4);
	test.PushBack(3);
	test.PushBack(2);
	test.PushBack(1);
	test.PushBack(0);
	
	PRINT_LIST(test, "Before Sort(): ");
	
	test.Sort();
	
	PRINT_LIST(test, "After Sort(): ");
	
	CHECK_LIST(test, 13);
}

UNITTEST( FxList, Serialization )
{
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> toSave;
	toSave.PushBack( 5 );
	toSave.PushBack( 15 );
	toSave.PushBack( 123 );
	toSave.PushBack( 5934 );

	OC3Ent::Face::FxArchive directoryCreater(OC3Ent::Face::FxArchiveStoreNull::Create(), OC3Ent::Face::FxArchive::AM_CreateDirectory);
	directoryCreater.Open();
	directoryCreater << toSave;
	OC3Ent::Face::FxArchiveStoreMemory* savingMemoryStore = 
		OC3Ent::Face::FxArchiveStoreMemory::Create( NULL, 0 );
	OC3Ent::Face::FxArchive savingArchive( savingMemoryStore, OC3Ent::Face::FxArchive::AM_Save );
	savingArchive.Open();
	savingArchive.SetInternalDataState(directoryCreater.GetInternalData());
	savingArchive << toSave;
	
	OC3Ent::Face::FxArchiveStoreMemory* loadingMemoryStore = 
		OC3Ent::Face::FxArchiveStoreMemory::Create( savingMemoryStore->GetMemory(), savingMemoryStore->GetSize() );
	OC3Ent::Face::FxArchive loadingArchive( loadingMemoryStore, OC3Ent::Face::FxArchive::AM_Load );
	loadingArchive.Open();

#ifdef NO_SAVE_VERSION
	CHECK(loadingArchive.IsValid() == FxFalse);
#else
	OC3Ent::Face::FxList<OC3Ent::Face::FxInt32> loaded;
	loadingArchive << loaded;

	CHECK( toSave.Length() == loaded.Length() );
	CHECK( *toSave.Begin() == *loaded.Begin() && *++toSave.Begin() == *++loaded.Begin()
		&& *++++toSave.Begin() == *++++loaded.Begin() && toSave.Back() == loaded.Back() );
#endif
}

#endif
