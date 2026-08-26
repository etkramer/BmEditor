/*=============================================================================
	BmEditorCommandlets.cpp: Bm editor commandlet definitions.
=============================================================================*/

#include "UnrealEd.h"
#include "BmEditorClasses.h"

IMPLEMENT_CLASS(UExtractPackagesCommandlet);

void LoadStartupPackages();

// Commandlets only load startup packages when running with -user, so UnrealEd.u
// is never loaded. The rest matches UMakeCommandlet::CreateCustomEngine().
void UExtractPackagesCommandlet::CreateCustomEngine()
{
	// disable this flag to make package loading work as expected internally
	GIsUCC = FALSE;
	LoadStartupPackages();
	GIsUCC = TRUE;

	UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_None, NULL );

	// must do this here so that the engine object that we create on the next line receives the correct property values
	UObject* DefaultEngine = EditorEngineClass->GetDefaultObject(TRUE);

	EditorEngineClass->ConditionalLink();
	DefaultEngine->LoadConfig();

	GEngine = GEditor = ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->InitEditor();
}

// A subpackage we found inside a cooked package, along with where it'll be written out.
struct FExtractedPackage
{
	UPackage*	Package;
	FString		FilePath;
};

// Total heap use, including the allocator's own per-allocation overhead.
static SIZE_T GetAllocatedMemory()
{
	FMemoryAllocationStats MemStats;
	GMalloc->GetAllocationInfo(MemStats);
	return MemStats.TotalAllocated;
}

// Map each file to the package it was cooked as a sublevel of, or INDEX_NONE. A sublevel is
// named after its parent plus a suffix (BaneSS_B1_Static_2 -> BaneSS_B1 -> BaneSS), so strip
// trailing underscore-delimited components until we hit a package that exists. _SF packages are
// standalone, and script packages never contain an underscore.
static void BuildParentIndices(const TArray<FString>& Files, TArray<INT>& ParentIndex)
{
	TMap<FString,INT> IndexByName;
	for (INT FileIndex = 0; FileIndex < Files.Num(); FileIndex++)
	{
		IndexByName.Set(FFilename(Files(FileIndex)).GetBaseFilename(), FileIndex);
	}

	ParentIndex.Empty(Files.Num());
	ParentIndex.AddZeroed(Files.Num());

	for (INT FileIndex = 0; FileIndex < Files.Num(); FileIndex++)
	{
		ParentIndex(FileIndex) = INDEX_NONE;

		FString Name = FFilename(Files(FileIndex)).GetBaseFilename();
		if (Name.EndsWith(TEXT("_SF")))
		{
			continue;
		}

		for (INT Underscore = Name.InStr(TEXT("_"), TRUE); Underscore != INDEX_NONE; Underscore = Name.InStr(TEXT("_"), TRUE))
		{
			Name = Name.Left(Underscore);

			INT* Found = IndexByName.Find(Name);
			if (Found)
			{
				ParentIndex(FileIndex) = *Found;
				break;
			}
		}
	}
}

// Load a cooked package into the current batch, bringing in its parent maps first. A sublevel is
// cooked assuming its parent is already loaded, so imports pointing into the parent only resolve if
// the whole chain is resident - otherwise they come through as NULL.
static void LoadForBatch(INT FileIndex, const TArray<FString>& Files, const TArray<INT>& ParentIndex, TArray<UBOOL>& LoadedThisBatch, TArray<UPackage*>& Roots)
{
	if (LoadedThisBatch(FileIndex))
	{
		return;
	}
	LoadedThisBatch(FileIndex) = TRUE;

	if (ParentIndex(FileIndex) != INDEX_NONE)
	{
		LoadForBatch(ParentIndex(FileIndex), Files, ParentIndex, LoadedThisBatch, Roots);
	}

	warnf(TEXT("  Loading '%s'"), *Files(FileIndex));

	UPackage* Package = UObject::LoadPackage(NULL, *Files(FileIndex), LOAD_None);
	if (Package)
	{
		Roots.AddItem(Package);
	}
	else
	{
		warnf(NAME_Warning, TEXT("  Failed to load '%s'"), *Files(FileIndex));
	}
}

// BM
INT UExtractPackagesCommandlet::Main(const FString& Params)
{
	// Root everything loaded at startup - script classes aren't RF_Native, so the collection between
	// batches would purge them. Same reason UCookPackagesCommandlet roots its script packages.
	for (FObjectIterator It; It; ++It)
	{
		It->AddToRoot();
	}

	const FString SourceDir = appGameDir() * TEXT("CookedPCConsole");
	const FString OutputDir = appGameDir() * TEXT("Packages");

	// There are far too many cooked packages to hold in memory at once, so work through them in
	// batches, merging each subpackage with whatever we've already written out for it. We're a 32-bit
	// process, so the batch ends once we're near enough to running out of address space. Merging and
	// saving grow it further on top, so this needs to leave a good deal of headroom.
	INT MemoryLimitMB = 2048;
	Parse(*Params, TEXT("MEMORY="), MemoryLimitMB);
	const SIZE_T MemoryLimit = (SIZE_T)Max(MemoryLimitMB, 64) * 1024 * 1024;

	TArray<FString> Files;
	appFindFilesInDirectory(Files, *SourceDir, TRUE, FALSE);

	TArray<INT> ParentIndex;
	BuildParentIndices(Files, ParentIndex);

	warnf(TEXT("Extracting subpackages from %i cooked packages, %i MB per batch"), Files.Num(), MemoryLimitMB);
	GFileManager->MakeDirectory(*OutputDir, TRUE);

	INT FileIndex = 0;
	while (FileIndex < Files.Num())
	{
		const INT BatchStart = FileIndex;

		// Load the cooked packages, remembering their roots so we don't extract them over themselves.
		TArray<UPackage*> Roots;
		TArray<UBOOL> LoadedThisBatch;
		LoadedThisBatch.AddZeroed(Files.Num());

		while (FileIndex < Files.Num())
		{
			LoadForBatch(FileIndex, Files, ParentIndex, LoadedThisBatch, Roots);

			FileIndex++;
			if (GetAllocatedMemory() >= MemoryLimit)
			{
				break;
			}
		}

		warnf(TEXT("Batch %i-%i of %i (%i MB allocated)"), BatchStart + 1, FileIndex, Files.Num(), (INT)(GetAllocatedMemory() / 1024 / 1024));

		// Gather the forced-export subpackages the cooked packages brought in with them.
		TArray<FExtractedPackage> Subpackages;
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			UPackage* Package = *It;

			// Skip "group" (not top-level) packages, and the cooked packages themselves
			if (Package->GetOuter() || Roots.ContainsItem(Package))
			{
				continue;
			}

			// Skip non-forced export packages and maps, which are extracted as-is
			if (Package->GetForcedExportBasePackageName() == NAME_None || Package->ContainsMap())
			{
				continue;
			}

			FExtractedPackage Subpackage;
			Subpackage.Package = Package;
			Subpackage.FilePath = OutputDir * Package->GetName();
			Subpackage.FilePath += (Package->PackageFlags & PKG_ContainsScript) ? TEXT(".u") : TEXT(".upk");
			Subpackages.AddItem(Subpackage);
		}

		// Merge in anything we've already extracted, since each cooked package only holds part of a
		// subpackage. Objects already in memory win, so this fills in the gaps instead of overwriting.
		for (INT SubIndex = 0; SubIndex < Subpackages.Num(); SubIndex++)
		{
			const FExtractedPackage& Subpackage = Subpackages(SubIndex);
			if (GFileManager->FileSize(*Subpackage.FilePath) >= 0)
			{
				LoadPackage(NULL, *Subpackage.FilePath, LOAD_None);
			}
		}

		// Set GIsCooking. Without this, SavePackage() will strip the PKG_Cooked flag.
		const UBOOL OldIsCooking = GIsCooking;
		const UE3::EPlatformType OldCookingTarget = GCookingTarget;

		GIsCooking = TRUE;
		GCookingTarget = UE3::PLATFORM_WindowsConsole;

		// Never collect in here - a package holds no references to its own contents, so there's no way
		// to keep the objects we still have to save alive across one.
		for (INT SubIndex = 0; SubIndex < Subpackages.Num(); SubIndex++)
		{
			const FExtractedPackage& Subpackage = Subpackages(SubIndex);
			warnf(TEXT("  Saving '%s'"), *Subpackage.FilePath);

			// Save as cooked, uncompressed
			Subpackage.Package->PackageFlags |= PKG_Cooked;
			Subpackage.Package->PackageFlags &= ~PKG_StoreCompressed;

			SavePackage(Subpackage.Package, NULL, RF_Standalone, *Subpackage.FilePath, GWarn, NULL, FALSE, FALSE);
		}

		GIsCooking = OldIsCooking;
		GCookingTarget = OldCookingTarget;

		// Free the batch before moving on.
		UObject::CollectGarbage(RF_Native);
	}

	return 0;
}
