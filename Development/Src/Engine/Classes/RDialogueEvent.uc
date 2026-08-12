// BM
class RDialogueEvent extends Object
	native
	dependson(AkWwise);

enum EPriority
{
	PRI_UNKNOWN,
	PRI_EMOTE,
	PRI_LOW,
	PRI_NORMAL,
	PRI_HIGH,
	PRI_MAX
};

enum EPlaceHolder
{
	PH_UNKNOWN,
	PH_PLACEHOLDER,
	PH_TEMPDIALOGUE,
	PH_FINAL,
	PH_SOUND_NOT_FOUND,
	PH_DIALOGUE_TRACK_NEEDED,
	PH_NO_CHARACTER,
	PH_DIALOGUEBANK_NOT_FOUND,
	PH_DIALOGUESTREAMS_NOT_LOADED,
	PH_MAX
};

struct native RLocalizedSubtitle
{
	var string Language;
	var array<SubtitleCue> Subtitles;
};

struct native AnimTriggerFaceFXTag
{
	var() name TagName;
	var() editconst array<string> TagParams;
	var() AnimSet TagAnimSet;
	var() float AtTime;
};

var(Info) AkEvent WwiseDialogueEvent;
var(Info) AkParameterName WwiseDuckingParameter;
var(Info) editconst string CharacterName;
var(Info) bool bUsesTTS;
var() bool bSyncFaceFX;
var() bool NotInDemo;
var() bool Is2D;
var() bool bEffect_IsRadio;
var() bool bIgnoreForSurviellance;
var(Info) AkWwise.EDialogueHelperType DialogueEvent_Type;
var(Info) const editconst EPriority Priority;
var() EPlaceHolder PlaceHolder;
var(Info) const editconst float MaxRange;
var(Info) const editconst float Duration;
var(Info) const editconst array<float> OtherLangDuration;
var(Subtitles) editconst array<SubtitleCue> Subtitles;
var(Info) editconst string Effect;
var() FaceFXAnimSet FaceFXAnimSetRef;
var() editconst string FaceFXGroupName;
var() editconst string FaceFXAnimName;
var() editconst array<AnimTriggerFaceFXTag> AnimTriggers;
var() editconst array<RLocalizedSubtitle> LocalizedSubtitles;
var() editoronly string TaggedText;
var() editconst string SubtitleCharacterName;
var() notforconsole editconst string debugSubtitleCharacterName;
var int ImportRef;
var() editconst string LocDirect_StringID;
var() int LocDirect_Hash;
var transient int ConversationID;

native function float GetCueDuration();

defaultproperties
{
	MaxRange=10000.000000
}
