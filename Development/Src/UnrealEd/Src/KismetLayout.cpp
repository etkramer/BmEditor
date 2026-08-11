/*=============================================================================
	KismetLayout.cpp: Automatic layout for Kismet sequences
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// BM: BM2 cooked packages strip sequence object positions, leaving every object at (0,0).
// This lays such sequences out using a layered (Sugiyama-style) graph drawing pass.

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "Kismet.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"

#if BATMAN

static const INT LAYER_GAP			= 96;
static const INT NODE_GAP			= 32;
static const INT VAR_GAP			= 16;
static const INT VAR_SPACING		= 8;
static const INT COMPONENT_GAP		= 128;
static const INT MAX_COLUMN_HEIGHT	= 8000;
static const INT ORDER_PASSES		= 8;
static const INT COORD_PASSES		= 8;
static const INT MAX_CROSSING_EDGES	= 1500;
static const INT LAYOUT_MAX_INT		= 0x7FFFFFFF;

namespace
{

struct FLayoutNode
{
	USequenceOp*	Op;
	FIntPoint		Size;
	INT				Component;
	INT				Layer;
	INT				Order;
	INT				VarBandHeight;
	FLOAT			PosY;
	TArray<INT>		Succ;
	TArray<INT>		Pred;
	TArray<USequenceVariable*> Vars;

	INT GetTotalHeight() const
	{
		return Size.Y + VarBandHeight;
	}
};

/** Stable insertion sort of Indices by a parallel array of keys. Keys are permuted alongside. */
static void SortByKey(TArray<INT>& Indices, TArray<FLOAT>& Keys)
{
	for (INT i = 1; i < Indices.Num(); i++)
	{
		const INT Value = Indices(i);
		const FLOAT Key = Keys(i);

		INT j = i - 1;
		while (j >= 0 && Keys(j) > Key)
		{
			Indices(j + 1) = Indices(j);
			Keys(j + 1) = Keys(j);
			j--;
		}
		Indices(j + 1) = Value;
		Keys(j + 1) = Key;
	}
}

class FSequenceLayout
{
public:
	explicit FSequenceLayout(USequence* InSequence)
		: Sequence(InSequence)
	{}

	void Apply();

private:
	void BuildGraph();
	void FindComponents();
	void BreakCycles();
	void AssignLayers(const TArray<INT>& CompNodes, TArray< TArray<INT> >& OutLayers);
	void OrderLayers(TArray< TArray<INT> >& Layers);
	INT CountCrossings(const TArray< TArray<INT> >& Layers) const;
	void AssignVariableOwners(const TArray< TArray<INT> >& Layers);
	FIntRect AssignCoordinates(const TArray< TArray<INT> >& Layers);
	void PlaceObjects(INT OffsetX, INT OffsetY, const TArray<INT>& CompNodes);
	void PlaceLeftovers(INT OriginX, INT BottomY);

	USequence*					Sequence;
	TArray<FLayoutNode>			Nodes;
	TMap<USequenceOp*, INT>		OpToNode;
	TArray<USequenceVariable*>	AllVars;
	TSet<USequenceVariable*>	OwnedVars;
	TSet<USequenceVariable*>	PlacedVars;
	TArray<USequenceObject*>	Frames;
};

/*-----------------------------------------------------------------------------
	Graph construction.
-----------------------------------------------------------------------------*/

void FSequenceLayout::BuildGraph()
{
	for (INT Idx = 0; Idx < Sequence->SequenceObjects.Num(); Idx++)
	{
		USequenceObject* Obj = Sequence->SequenceObjects(Idx);
		if (Obj == NULL)
		{
			continue;
		}

		if (USequenceOp* Op = Cast<USequenceOp>(Obj))
		{
			const INT NodeIdx = Nodes.AddZeroed();
			Nodes(NodeIdx).Op = Op;
			Nodes(NodeIdx).Size = Op->GetLayoutSize();
			Nodes(NodeIdx).Component = INDEX_NONE;
			OpToNode.Set(Op, NodeIdx);
		}
		else if (USequenceVariable* Var = Cast<USequenceVariable>(Obj))
		{
			AllVars.AddItem(Var);
			OwnedVars.Add(Var);
		}
		else
		{
			Frames.AddItem(Obj);
		}
	}

	// Output links and event links both flow left to right
	for (INT NodeIdx = 0; NodeIdx < Nodes.Num(); NodeIdx++)
	{
		FLayoutNode& Node = Nodes(NodeIdx);

		for (INT OutIdx = 0; OutIdx < Node.Op->OutputLinks.Num(); OutIdx++)
		{
			const FSeqOpOutputLink& Out = Node.Op->OutputLinks(OutIdx);
			for (INT LinkIdx = 0; LinkIdx < Out.Links.Num(); LinkIdx++)
			{
				const INT* DestIdx = OpToNode.Find(Out.Links(LinkIdx).LinkedOp);
				if (DestIdx != NULL && *DestIdx != NodeIdx)
				{
					Node.Succ.AddUniqueItem(*DestIdx);
					Nodes(*DestIdx).Pred.AddUniqueItem(NodeIdx);
				}
			}
		}

		for (INT EventIdx = 0; EventIdx < Node.Op->EventLinks.Num(); EventIdx++)
		{
			const FSeqEventLink& EventLink = Node.Op->EventLinks(EventIdx);
			for (INT LinkIdx = 0; LinkIdx < EventLink.LinkedEvents.Num(); LinkIdx++)
			{
				const INT* SrcIdx = OpToNode.Find(EventLink.LinkedEvents(LinkIdx));
				if (SrcIdx != NULL && *SrcIdx != NodeIdx)
				{
					Nodes(*SrcIdx).Succ.AddUniqueItem(NodeIdx);
					Node.Pred.AddUniqueItem(*SrcIdx);
				}
			}
		}
	}
}

void FSequenceLayout::FindComponents()
{
	INT NumComponents = 0;

	TArray<INT> Stack;
	for (INT StartIdx = 0; StartIdx < Nodes.Num(); StartIdx++)
	{
		if (Nodes(StartIdx).Component != INDEX_NONE)
		{
			continue;
		}

		const INT Component = NumComponents++;
		Stack.Empty();
		Stack.AddItem(StartIdx);
		Nodes(StartIdx).Component = Component;

		while (Stack.Num() > 0)
		{
			const INT NodeIdx = Stack.Pop();
			const FLayoutNode& Node = Nodes(NodeIdx);

			for (INT i = 0; i < Node.Succ.Num() + Node.Pred.Num(); i++)
			{
				const INT Neighbour = i < Node.Succ.Num() ? Node.Succ(i) : Node.Pred(i - Node.Succ.Num());
				if (Nodes(Neighbour).Component == INDEX_NONE)
				{
					Nodes(Neighbour).Component = Component;
					Stack.AddItem(Neighbour);
				}
			}
		}
	}
}

/** Strips back edges so layering can run on a DAG. Kismet graphs are full of loops. */
void FSequenceLayout::BreakCycles()
{
	enum EColor { White, Gray, Black };

	TArray<BYTE> Color;
	Color.AddZeroed(Nodes.Num());

	TArray<INT> NodeStack;
	TArray<INT> EdgeStack;

	for (INT StartIdx = 0; StartIdx < Nodes.Num(); StartIdx++)
	{
		if (Color(StartIdx) != White)
		{
			continue;
		}

		NodeStack.Empty();
		EdgeStack.Empty();
		NodeStack.AddItem(StartIdx);
		EdgeStack.AddItem(0);
		Color(StartIdx) = Gray;

		while (NodeStack.Num() > 0)
		{
			const INT NodeIdx = NodeStack.Last();
			INT& EdgeIdx = EdgeStack(EdgeStack.Num() - 1);

			if (EdgeIdx >= Nodes(NodeIdx).Succ.Num())
			{
				Color(NodeIdx) = Black;
				NodeStack.Pop();
				EdgeStack.Pop();
				continue;
			}

			const INT Child = Nodes(NodeIdx).Succ(EdgeIdx);
			if (Color(Child) == Gray)
			{
				// back edge - drop it from both endpoints
				Nodes(NodeIdx).Succ.Remove(EdgeIdx);
				Nodes(Child).Pred.RemoveItem(NodeIdx);
				continue;
			}

			EdgeIdx++;

			if (Color(Child) == White)
			{
				Color(Child) = Gray;
				NodeStack.AddItem(Child);
				EdgeStack.AddItem(0);
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Layering and ordering.
-----------------------------------------------------------------------------*/

/** Longest-path layering, so sources (events) land in layer 0. */
void FSequenceLayout::AssignLayers(const TArray<INT>& CompNodes, TArray< TArray<INT> >& OutLayers)
{
	TMap<INT, INT> Remaining;
	TArray<INT> Ready;

	for (INT i = 0; i < CompNodes.Num(); i++)
	{
		const INT NodeIdx = CompNodes(i);
		Nodes(NodeIdx).Layer = 0;
		Remaining.Set(NodeIdx, Nodes(NodeIdx).Pred.Num());
		if (Nodes(NodeIdx).Pred.Num() == 0)
		{
			Ready.AddItem(NodeIdx);
		}
	}

	INT NumProcessed = 0;
	while (Ready.Num() > 0)
	{
		const INT NodeIdx = Ready.Pop();
		NumProcessed++;

		for (INT i = 0; i < Nodes(NodeIdx).Succ.Num(); i++)
		{
			const INT Child = Nodes(NodeIdx).Succ(i);
			Nodes(Child).Layer = Max(Nodes(Child).Layer, Nodes(NodeIdx).Layer + 1);

			INT* Count = Remaining.Find(Child);
			if (Count != NULL && --(*Count) == 0)
			{
				Ready.AddItem(Child);
			}
		}
	}

	// shouldn't happen now that back edges are gone, but don't drop nodes on the floor if it does
	if (NumProcessed < CompNodes.Num())
	{
		warnf(NAME_Warning, TEXT("Kismet auto-layout: %d nodes left unlayered in %s"), CompNodes.Num() - NumProcessed, *Sequence->GetPathName());
	}

	INT MaxLayer = 0;
	for (INT i = 0; i < CompNodes.Num(); i++)
	{
		MaxLayer = Max(MaxLayer, Nodes(CompNodes(i)).Layer);
	}

	OutLayers.Empty(MaxLayer + 1);
	OutLayers.AddZeroed(MaxLayer + 1);
	for (INT i = 0; i < CompNodes.Num(); i++)
	{
		const INT NodeIdx = CompNodes(i);
		Nodes(NodeIdx).Order = OutLayers(Nodes(NodeIdx).Layer).Num();
		OutLayers(Nodes(NodeIdx).Layer).AddItem(NodeIdx);
	}
}

INT FSequenceLayout::CountCrossings(const TArray< TArray<INT> >& Layers) const
{
	INT Crossings = 0;

	for (INT LayerIdx = 1; LayerIdx < Layers.Num(); LayerIdx++)
	{
		// each edge as (upper order, lower order)
		TArray<INT> Upper;
		TArray<INT> Lower;

		const TArray<INT>& Layer = Layers(LayerIdx);
		for (INT i = 0; i < Layer.Num(); i++)
		{
			const FLayoutNode& Node = Nodes(Layer(i));
			for (INT p = 0; p < Node.Pred.Num(); p++)
			{
				const FLayoutNode& Parent = Nodes(Node.Pred(p));
				if (Parent.Layer == LayerIdx - 1)
				{
					Upper.AddItem(Parent.Order);
					Lower.AddItem(Node.Order);
				}
			}
		}

		if (Upper.Num() > MAX_CROSSING_EDGES)
		{
			continue;
		}

		for (INT a = 0; a < Upper.Num(); a++)
		{
			for (INT b = a + 1; b < Upper.Num(); b++)
			{
				if ((Upper(a) - Upper(b)) * (Lower(a) - Lower(b)) < 0)
				{
					Crossings++;
				}
			}
		}
	}

	return Crossings;
}

/** Median heuristic sweeps, keeping the best ordering seen. */
void FSequenceLayout::OrderLayers(TArray< TArray<INT> >& Layers)
{
	TArray< TArray<INT> > Best = Layers;
	INT BestCrossings = CountCrossings(Layers);

	TArray<FLOAT> Keys;

	for (INT Pass = 0; Pass < ORDER_PASSES && BestCrossings > 0; Pass++)
	{
		const UBOOL bDownward = (Pass % 2) == 0;

		for (INT i = 0; i < Layers.Num(); i++)
		{
			const INT LayerIdx = bDownward ? i : Layers.Num() - 1 - i;
			const INT AdjacentLayer = bDownward ? LayerIdx - 1 : LayerIdx + 1;
			if (AdjacentLayer < 0 || AdjacentLayer >= Layers.Num())
			{
				continue;
			}

			TArray<INT>& Layer = Layers(LayerIdx);

			Keys.Empty(Layer.Num());
			for (INT n = 0; n < Layer.Num(); n++)
			{
				const FLayoutNode& Node = Nodes(Layer(n));
				const TArray<INT>& Neighbours = bDownward ? Node.Pred : Node.Succ;

				TArray<FLOAT> Positions;
				for (INT k = 0; k < Neighbours.Num(); k++)
				{
					if (Nodes(Neighbours(k)).Layer == AdjacentLayer)
					{
						Positions.AddItem((FLOAT)Nodes(Neighbours(k)).Order);
					}
				}

				if (Positions.Num() == 0)
				{
					// no anchor - leave where it is
					Keys.AddItem((FLOAT)Node.Order);
				}
				else
				{
					FLOAT Total = 0.f;
					for (INT k = 0; k < Positions.Num(); k++)
					{
						Total += Positions(k);
					}
					Keys.AddItem(Total / Positions.Num());
				}
			}

			SortByKey(Layer, Keys);

			for (INT n = 0; n < Layer.Num(); n++)
			{
				Nodes(Layer(n)).Order = n;
			}
		}

		const INT Crossings = CountCrossings(Layers);
		if (Crossings < BestCrossings)
		{
			BestCrossings = Crossings;
			Best = Layers;
		}
	}

	Layers = Best;
	for (INT LayerIdx = 0; LayerIdx < Layers.Num(); LayerIdx++)
	{
		for (INT n = 0; n < Layers(LayerIdx).Num(); n++)
		{
			Nodes(Layers(LayerIdx)(n)).Order = n;
		}
	}
}

/*-----------------------------------------------------------------------------
	Coordinates.
-----------------------------------------------------------------------------*/

/** Each variable is drawn under the first op that references it, in layer order. */
void FSequenceLayout::AssignVariableOwners(const TArray< TArray<INT> >& Layers)
{
	for (INT LayerIdx = 0; LayerIdx < Layers.Num(); LayerIdx++)
	{
		for (INT n = 0; n < Layers(LayerIdx).Num(); n++)
		{
			FLayoutNode& Node = Nodes(Layers(LayerIdx)(n));

			for (INT VarLinkIdx = 0; VarLinkIdx < Node.Op->VariableLinks.Num(); VarLinkIdx++)
			{
				const FSeqVarLink& VarLink = Node.Op->VariableLinks(VarLinkIdx);
				for (INT i = 0; i < VarLink.LinkedVariables.Num(); i++)
				{
					USequenceVariable* Var = VarLink.LinkedVariables(i);
					if (Var != NULL && OwnedVars.Contains(Var) && !PlacedVars.Contains(Var))
					{
						PlacedVars.Add(Var);
						Node.Vars.AddItem(Var);
					}
				}
			}

			if (Node.Vars.Num() > 0)
			{
				Node.VarBandHeight = LO_MIN_SHAPE_SIZE + VAR_GAP;
			}
		}
	}
}

/** Packs each layer vertically, then relaxes towards the median of connected neighbours. */
FIntRect FSequenceLayout::AssignCoordinates(const TArray< TArray<INT> >& Layers)
{
	TArray<INT> LayerX;
	LayerX.AddZeroed(Layers.Num());

	INT RunningX = 0;
	for (INT LayerIdx = 0; LayerIdx < Layers.Num(); LayerIdx++)
	{
		LayerX(LayerIdx) = RunningX;

		INT MaxWidth = 0;
		for (INT n = 0; n < Layers(LayerIdx).Num(); n++)
		{
			const FLayoutNode& Node = Nodes(Layers(LayerIdx)(n));
			INT Width = Node.Size.X;
			if (Node.Vars.Num() > 0)
			{
				Width = Max(Width, Node.Vars.Num() * (LO_MIN_SHAPE_SIZE + VAR_SPACING) - VAR_SPACING);
			}
			MaxWidth = Max(MaxWidth, Width);
		}

		RunningX += MaxWidth + LAYER_GAP;
	}

	for (INT LayerIdx = 0; LayerIdx < Layers.Num(); LayerIdx++)
	{
		FLOAT Y = 0.f;
		for (INT n = 0; n < Layers(LayerIdx).Num(); n++)
		{
			FLayoutNode& Node = Nodes(Layers(LayerIdx)(n));
			Node.PosY = Y;
			Y += Node.GetTotalHeight() + NODE_GAP;
		}
	}

	for (INT Pass = 0; Pass < COORD_PASSES; Pass++)
	{
		const UBOOL bDownward = (Pass % 2) == 0;

		for (INT i = 0; i < Layers.Num(); i++)
		{
			const INT LayerIdx = bDownward ? i : Layers.Num() - 1 - i;
			const TArray<INT>& Layer = Layers(LayerIdx);

			TArray<FLOAT> Desired;
			for (INT n = 0; n < Layer.Num(); n++)
			{
				const FLayoutNode& Node = Nodes(Layer(n));
				const TArray<INT>& Neighbours = bDownward ? Node.Pred : Node.Succ;

				FLOAT Total = 0.f;
				INT Count = 0;
				for (INT k = 0; k < Neighbours.Num(); k++)
				{
					const FLayoutNode& Neighbour = Nodes(Neighbours(k));
					Total += Neighbour.PosY + Neighbour.Size.Y * 0.5f;
					Count++;
				}

				Desired.AddItem(Count > 0 ? (Total / Count) - Node.Size.Y * 0.5f : Node.PosY);
			}

			for (INT n = 0; n < Layer.Num(); n++)
			{
				FLayoutNode& Node = Nodes(Layer(n));
				if (n == 0)
				{
					Node.PosY = Desired(n);
				}
				else
				{
					const FLayoutNode& Prev = Nodes(Layer(n - 1));
					Node.PosY = Max(Desired(n), Prev.PosY + Prev.GetTotalHeight() + NODE_GAP);
				}
			}
		}
	}

	FIntRect Bounds(LAYOUT_MAX_INT, LAYOUT_MAX_INT, -LAYOUT_MAX_INT, -LAYOUT_MAX_INT);
	for (INT LayerIdx = 0; LayerIdx < Layers.Num(); LayerIdx++)
	{
		for (INT n = 0; n < Layers(LayerIdx).Num(); n++)
		{
			const FLayoutNode& Node = Nodes(Layers(LayerIdx)(n));
			const INT X = LayerX(LayerIdx);
			const INT Y = appRound(Node.PosY);

			Bounds.Min.X = Min(Bounds.Min.X, X);
			Bounds.Min.Y = Min(Bounds.Min.Y, Y);
			Bounds.Max.X = Max(Bounds.Max.X, X + Node.Size.X);
			Bounds.Max.Y = Max(Bounds.Max.Y, Y + Node.GetTotalHeight());
		}
	}

	// bake the layer X back onto the nodes so PlaceObjects only needs the offsets
	for (INT LayerIdx = 0; LayerIdx < Layers.Num(); LayerIdx++)
	{
		for (INT n = 0; n < Layers(LayerIdx).Num(); n++)
		{
			Nodes(Layers(LayerIdx)(n)).Op->ObjPosX = LayerX(LayerIdx);
		}
	}

	return Bounds;
}

void FSequenceLayout::PlaceObjects(INT OffsetX, INT OffsetY, const TArray<INT>& CompNodes)
{
	for (INT i = 0; i < CompNodes.Num(); i++)
	{
		FLayoutNode& Node = Nodes(CompNodes(i));

		Node.Op->ObjPosX += OffsetX;
		Node.Op->ObjPosY = appRound(Node.PosY) + OffsetY;

		INT VarX = Node.Op->ObjPosX;
		const INT VarY = Node.Op->ObjPosY + Node.Size.Y + VAR_GAP;
		for (INT v = 0; v < Node.Vars.Num(); v++)
		{
			Node.Vars(v)->ObjPosX = VarX;
			Node.Vars(v)->ObjPosY = VarY;
			VarX += LO_MIN_SHAPE_SIZE + VAR_SPACING;
		}
	}
}

/** Variables nothing in this sequence links to, plus comment frames, get parked out of the way. */
void FSequenceLayout::PlaceLeftovers(INT OriginX, INT BottomY)
{
	INT X = OriginX;
	INT Y = BottomY + COMPONENT_GAP;

	for (INT i = 0; i < AllVars.Num(); i++)
	{
		if (PlacedVars.Contains(AllVars(i)))
		{
			continue;
		}

		AllVars(i)->ObjPosX = X;
		AllVars(i)->ObjPosY = Y;

		X += LO_MIN_SHAPE_SIZE + VAR_SPACING;
		if (X - OriginX > 1600)
		{
			X = OriginX;
			Y += LO_MIN_SHAPE_SIZE + VAR_GAP;
		}
	}

	X = OriginX;
	Y += LO_MIN_SHAPE_SIZE + COMPONENT_GAP;
	for (INT i = 0; i < Frames.Num(); i++)
	{
		Frames(i)->ObjPosX = X;
		Frames(i)->ObjPosY = Y;

		USequenceFrame* Frame = Cast<USequenceFrame>(Frames(i));
		Y += Max(Frame != NULL ? Frame->SizeY : 0, LO_MIN_SHAPE_SIZE) + NODE_GAP;
	}
}

void FSequenceLayout::Apply()
{
	BuildGraph();

	if (Nodes.Num() == 0)
	{
		PlaceLeftovers(0, 0);
		return;
	}

	FindComponents();
	BreakCycles();

	// group nodes by component, biggest first so the interesting graph is at the top
	TArray< TArray<INT> > Components;
	for (INT NodeIdx = 0; NodeIdx < Nodes.Num(); NodeIdx++)
	{
		const INT Component = Nodes(NodeIdx).Component;
		if (Components.Num() <= Component)
		{
			Components.AddZeroed(Component + 1 - Components.Num());
		}
		Components(Component).AddItem(NodeIdx);
	}

	for (INT i = 0; i < Components.Num(); i++)
	{
		for (INT j = i + 1; j < Components.Num(); j++)
		{
			if (Components(j).Num() > Components(i).Num())
			{
				Exchange(Components(i), Components(j));
			}
		}
	}

	INT ColumnX = 0;
	INT ColumnY = 0;
	INT ColumnWidth = 0;
	INT MaxX = 0;
	INT MaxY = 0;

	for (INT i = 0; i < Components.Num(); i++)
	{
		TArray< TArray<INT> > Layers;
		AssignLayers(Components(i), Layers);
		OrderLayers(Layers);
		AssignVariableOwners(Layers);

		const FIntRect Bounds = AssignCoordinates(Layers);
		const INT Width = Bounds.Max.X - Bounds.Min.X;
		const INT Height = Bounds.Max.Y - Bounds.Min.Y;

		// wrap into a new column rather than running off the bottom of the valid region
		if (ColumnY > 0 && ColumnY + Height > MAX_COLUMN_HEIGHT)
		{
			ColumnX += ColumnWidth + COMPONENT_GAP;
			ColumnY = 0;
			ColumnWidth = 0;
		}

		PlaceObjects(ColumnX - Bounds.Min.X, ColumnY - Bounds.Min.Y, Components(i));

		ColumnWidth = Max(ColumnWidth, Width);
		MaxX = Max(MaxX, ColumnX + Width);
		MaxY = Max(MaxY, ColumnY + Height);
		ColumnY += Height + COMPONENT_GAP;
	}

	PlaceLeftovers(0, MaxY);
}

} // anonymous namespace

/**
 * Assigns positions to every object in a sequence whose objects are all stacked at the origin,
 * as they are in BM2 cooked packages. Does not mark the package dirty.
 */
void AutoLayoutSequence(USequence* InSequence, INT MaxSequenceSize)
{
	if (InSequence == NULL || InSequence->SequenceObjects.Num() < 2)
	{
		return;
	}

	for (INT Idx = 0; Idx < InSequence->SequenceObjects.Num(); Idx++)
	{
		const USequenceObject* Obj = InSequence->SequenceObjects(Idx);
		if (Obj != NULL && (Obj->ObjPosX != 0 || Obj->ObjPosY != 0))
		{
			return;
		}
	}

	FSequenceLayout Layout(InSequence);
	Layout.Apply();

	for (INT Idx = 0; Idx < InSequence->SequenceObjects.Num(); Idx++)
	{
		if (InSequence->SequenceObjects(Idx) != NULL)
		{
			InSequence->SequenceObjects(Idx)->SnapPosition(KISMET_GRIDSIZE, MaxSequenceSize);
		}
	}

	debugf(NAME_Log, TEXT("Kismet auto-layout: arranged %d objects in %s"), InSequence->SequenceObjects.Num(), *InSequence->GetPathName());
}

#endif // BATMAN
