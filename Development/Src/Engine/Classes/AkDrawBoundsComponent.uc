// BM
class AkDrawBoundsComponent extends PrimitiveComponent
	native
	editinlinenew
	collapsecategories;

var() Color				DrawBoundsColor;
var BoxSphereBounds		DrawBounds;
var Vector				X_LMH;
var Vector				Y_LMH;
var Vector				Z_LMH;
var bool				X_Enabled;
var bool				Y_Enabled;
var bool				Z_Enabled;

defaultproperties
{
	DrawBoundsColor=(R=84,G=167,B=196,A=0)
	HiddenGame=true
	AlwaysLoadOnClient=false
	AlwaysLoadOnServer=false
}
