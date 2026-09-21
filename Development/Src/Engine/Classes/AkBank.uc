// BM
class AkBank extends AkBankExternalHook
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

var() const editconst EAkBankLoadType	BankLoadType;
var() const editconst bool				bContainsXMA;
var() const editconst notforconsole array<AkParameterName> ReferencedParameters;
var const transient int	ShouldLoadCounter;
var const transient int	LoadCounter;
var const transient int	DeferredLoadCounter;
var const transient int	PreloadStreamsCounter;

native final function bool LoadBank(bool performDeferredLoad);
native final function UnloadBank(bool performDeferredUnload);
native final function ReloadBank();
native final function bool IsBankLoadComplete();
native final function bool PreloadBankStreams(bool preload, optional AkWwise.EAkIOStreamPriorities Priority = AKIO_PRIORITY_NORMAL);
native final function bool IsBankStreamPreloaded();

defaultproperties
{
	BankLoadType=AK_BANK_LOAD_NORMAL
}
