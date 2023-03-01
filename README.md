# CopperCube 6 - Source Code improvements
source file improvements for testing purposes before annual update

User requires the game engine source code to work (Studio version). 
If you don't have this then the files wont be of any good to you...
This is NOT the source code, but rather a few small snippets of the whole.

If you don't want to wait a whole year for these to be added into the engine by Niko then get them here and try them out in the meantime:

- FPS Camera - mouse speed 
- FPS Camera - jump speed
- FPS Camera - can fly (on/off)
- Spot Light - full control over direction and brightness etc dynamically
- Terrain textures - adjust heights



FPS CAMERA Usage

Rotation Speed
Javascript - setCopperCubeVariable(#CameraName.rotationspeed, speed wanted);
or use drop down menu and use the "Set or Change a Variable"
- VariableName = #CameraName.rotationspeed, and Value = speed wanted

Jump Speed
Javascript - setCopperCubeVariable(#CameraName.jumpspeed, speed wanted);
or use drop down menu and use the "Set or Change a Variable"
- VariableName = #CameraName.jumpspeed, and Value = speed wanted

Can Fly
Javascript - setCopperCubeVariable(#CameraName.canfly, true/false);
or use drop down menu and use the "Set or Change a Variable"
- VariableName = #CameraName.canfly, and Value = true/false



SPOT LIGHT
use existing API - ccbSetSceneNodeProperty(spot light node, "Direction", x,y,z);
player torch effect can be run with this code (every 20ms or better):

var mouseX = ccbGetMousePosX();
var mouseY = ccbGetMousePosY();
var end = ccbGet3DPosFrom2DPos(mouseX, mouseY);
end.y -=1500;
end.normalize();
ccbSetSceneNodeProperty(spot, "Direction", end);

You also dynamically adjust these parameters:
ccbSetSceneNodeProperty(spot, "Attenuation", att);
ccbSetSceneNodeProperty(spot, "OuterCone", oC);
ccbSetSceneNodeProperty(spot, "InnerCone", iC);
ccbSetSceneNodeProperty(spot, "Falloff", falloff);


TERRAIN TEXTURES
new API - ccbSetTerrainTexHeight(node, 0.1, 0.8);
First get Terrain scene node then apply 1st and 2nd texture as percentage of total height of terrain as a decimal value (above would be 10% and 80%)
The 3rd texture will be the remining height above 2nd texture (ie 20%)
