// BM
class RExternalHook extends AkHash
	native;

/** One cooked payload, tagged with the platform and the language it was cooked for. */
struct native ExternalHookDataEntry
{
	var() string	Entry;
	var() string	Platform;
	var() string	Tag;
	var native const UntypedBulkData_Mirror BulkStoredData{FByteBulkData};
	var transient bool bPreallocated;
};

var() array<ExternalHookDataEntry> ExternalEntries;
var bool				bExternalEntriesInitialized;
var bool				bCooked;
var transient int		LockedIndex;
var const transient native pointer LockedMemory;

cpptext
{
	// UObject interface.
	virtual void FinishDestroy();
	virtual void Serialize( FArchive& Ar );
}

defaultproperties
{
}
