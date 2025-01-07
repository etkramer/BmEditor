class MaterialExpressionLightingSpecularPhong extends MaterialExpression within Material
    native
    collapsecategories;

var ExpressionInput Power;
var ExpressionInput PowerMin;
var ExpressionInput PowerMax;
var ExpressionInput FresnelPower;
var ExpressionInput FresnelStraight;
var ExpressionInput FresnelGlancing;
var ExpressionInput FalloffPower;
var ExpressionInput FalloffStraight;
var ExpressionInput FalloffGlancing;
var ExpressionInput Colour;
var() Texture Lookup;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}