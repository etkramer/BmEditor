/*=============================================================================
	BmEditorCommandlets.cpp: Bm editor commandlet definitions.
=============================================================================*/

#include "UnrealEd.h"
#include "BmEditorClasses.h"

IMPLEMENT_CLASS(UExtractPackagesCommandlet);

void LoadStartupPackages();

// BM: Commandlets only load startup packages when running with -user, so UnrealEd.u
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

// BM: A subpackage we found inside a cooked package, along with where it'll be written out.
struct FExtractedPackage
{
	UPackage*	Package;
	FString		FilePath;
};

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
	// batches, merging each subpackage with whatever we've already written out for it.
	// TODO: Start a new batch after ~3GB memory use rather than some fixed number of upks
	INT BatchSize = 400;
	Parse(*Params, TEXT("BATCH="), BatchSize);
	BatchSize = Max(BatchSize, 1);

	FString Filter;
	Parse(*Params, TEXT("FILTER="), Filter);

	TArray<FString> Files;
	appFindFilesInDirectory(Files, *SourceDir, TRUE, FALSE);

	if (Filter.Len() > 0)
	{
		for (INT FileIndex = Files.Num() - 1; FileIndex >= 0; FileIndex--)
		{
			if (Files(FileIndex).InStr(Filter, FALSE, TRUE) == INDEX_NONE)
			{
				Files.Remove(FileIndex);
			}
		}
	}

	warnf(TEXT("Extracting subpackages from %i cooked packages in batches of %i"), Files.Num(), BatchSize);
	GFileManager->MakeDirectory(*OutputDir, TRUE);

	for (INT BatchStart = 0; BatchStart < Files.Num(); BatchStart += BatchSize)
	{
		const INT BatchEnd = Min(BatchStart + BatchSize, Files.Num());
		warnf(TEXT("Batch %i-%i of %i"), BatchStart + 1, BatchEnd, Files.Num());

		// Load the cooked packages, remembering their roots so we don't extract them over themselves.
		TArray<UPackage*> Roots;
		for (INT FileIndex = BatchStart; FileIndex < BatchEnd; FileIndex++)
		{
			warnf(TEXT("  Loading '%s'"), *Files(FileIndex));

			UPackage* Package = LoadPackage(NULL, *Files(FileIndex), LOAD_None);
			if (Package)
			{
				Roots.AddItem(Package);
			}
			else
			{
				warnf(NAME_Warning, TEXT("  Failed to load '%s'"), *Files(FileIndex));
			}
		}

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

			// Skip non-BM packages and maps, which are extracted as-is
			if (!Package->IsBmCooked() || Package->ContainsMap())
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
		// Save as VER_BATMAN2 too, so these load through the retail paths rather than the editor ones.
		const UBOOL OldIsCooking = GIsCooking;
		const UE3::EPlatformType OldCookingTarget = GCookingTarget;
		const INT OldLicenseeVersion = GPackageFileLicenseeVersion;

		GIsCooking = TRUE;
		GCookingTarget = UE3::PLATFORM_WindowsConsole;
		GPackageFileLicenseeVersion = VER_BATMAN2;

		for (INT SubIndex = 0; SubIndex < Subpackages.Num(); SubIndex++)
		{
			const FExtractedPackage& Subpackage = Subpackages(SubIndex);
			warnf(TEXT("  Saving '%s'"), *Subpackage.FilePath);

			// Save as cooked, uncompressed
			Subpackage.Package->PackageFlags |= PKG_Cooked;
			Subpackage.Package->PackageFlags &= ~PKG_StoreCompressed;

			SavePackage(Subpackage.Package, NULL, RF_Standalone, *Subpackage.FilePath, GWarn);
		}

		GIsCooking = OldIsCooking;
		GCookingTarget = OldCookingTarget;
		GPackageFileLicenseeVersion = OldLicenseeVersion;

		// Free the batch before moving on.
		UObject::CollectGarbage(RF_Native);
	}

	return 0;
}
