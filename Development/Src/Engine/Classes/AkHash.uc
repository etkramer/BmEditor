// BM
class AkHash extends Object
	native;

var int HashValue;

native final function int GetHashValue();
native final function string GetHashString(optional string NotFoundResult = "");
native final function string GetOriginalNameString();
private native final function DetermineHashValue();

native static final function string AkHashToString(int HashValueToLookup, optional string ResultIfNotFound = "");
native static final function int StringToAkHash(string StringToHash);

defaultproperties
{
}
