/**
 * BM2 physical material texture: per-texel mask sampled by collision to choose a
 * physical material based on the surface's albedo. Stub for serialization only.
 */
class RPhysicalMaterialTexture extends Object
	native(Material);

struct native RColourPhysicalMaterialPair
{
	var() color					Colour;
	var() PhysicalMaterial		PhysMaterial;
};

var int SizeX;
var int SizeY;
var byte Encoding;
var array<int> Texture;
var array<RColourPhysicalMaterialPair> ColourPhysicalMaterialMap;

defaultproperties
{
}
