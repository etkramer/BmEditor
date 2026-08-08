// BM
class AkBank extends AkAssetBase
	native
	dependson(AkWwise);

enum EAkBankLoadType
{
	AK_BANK_LOAD_NONE,
	AK_BANK_LOAD_NORMAL,
	AK_BANK_LOAD_PREPARE,
	AK_BANK_LOAD_DEFERRED,
	AK_BANK_LOAD_MAX
};

var() bool				IncludeBankInAssetPack;
var() bool				MaxStreamPriority;
var const transient bool ShouldBeLoaded;
var() bool				DoNotFreeBulkData;
var() EAkBankLoadType	BankLoadType;
var const transient int	LoadCounter;
var const transient int	HookCounter;
var const transient int	PreloadStreamsCounter;
var const transient native pointer LockedMemory;

/** Payload for the language that matches the running one; unused, kept for layout. */
var native const UntypedBulkData_Mirror LoadedBankData{FByteBulkData};
/** One payload per cooked language; read on load and written back verbatim on save. */
var native const array<UntypedBulkData_Mirror> StoredBankData{FByteBulkData};

var array<string>		Languages;
var int					BanksCooked;

native final function bool LoadBank(bool performDeferredLoad);
native final function UnloadBank(bool performDeferredUnload);
native final function ForceUnloadBank();
native final function bool IsBankLoadComplete();
native final function bool PreloadBankStreams(bool preload, optional AkWwise.EAkIOStreamPriorities Priority = AKIO_PRIORITY_NORMAL);
native final function bool IsBankStreamPreloaded();

cpptext
{
	// UObject interface.
	virtual void Serialize( FArchive& Ar );
}

defaultproperties
{
	IncludeBankInAssetPack=true
	BankLoadType=AK_BANK_LOAD_NORMAL
}
