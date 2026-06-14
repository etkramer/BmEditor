/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryLight extends ActorFactory
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

var editoronly export PointLightComponent LightComponent;

defaultproperties
{
	MenuName="Add Light (Point)"
	NewActorClass=class'Engine.PointLight'
	bShowInEditorQuickMenu=true
}
