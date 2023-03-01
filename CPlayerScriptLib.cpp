#include "CPlayer.h"
#include "irrHelpers.h"
#include "EFlaceSceneNodeTypes.h"
#include "CFlaceAnimatorExtensionScript.h"
#include "CFlaceAnimatorCollisionResponse.h"
#include "CFlaceAnimatorRigidPhysicsBody.h"
#include "CFlaceAnimatorGameAI.h"
#include "CCCActionHandler.h"
#include "INetworkSupport.h"
#include "SteamSupport.h"
#include "os.h"
#include "CFlaceTerrainSceneNode.h"

CPlayer* CPlayer::LastPlayer = 0;

void CPlayer::registerPlayerScripts()
{
	if (!Scripting)
		return;

	Scripting->addGlobalFunction(ccbCloneSceneNode,					"ccbCloneSceneNode");
	Scripting->addGlobalFunction(ccbRegisterBehaviorEventReceiver,	"ccbRegisterBehaviorEventReceiver"); // internal use only
	Scripting->addGlobalFunction(ccbRegisterKeyDownEvent,			"ccbRegisterKeyDownEvent");
	Scripting->addGlobalFunction(ccbRegisterKeyUpEvent,				"ccbRegisterKeyUpEvent");
	Scripting->addGlobalFunction(ccbRegisterMouseDownEvent,			"ccbRegisterMouseDownEvent");
	Scripting->addGlobalFunction(ccbRegisterMouseUpEvent,			"ccbRegisterMouseUpEvent");
	//Scripting->addGlobalFunction(ccbRegisterOnFrameEvent,			"ccbRegisterOnFrameEvent");
	Scripting->addGlobalFunction(ccbSwitchToScene,					"ccbSwitchToScene");
	Scripting->addGlobalFunction(ccbGetMousePosX,					"ccbGetMousePosX");
	Scripting->addGlobalFunction(ccbGetMousePosY,					"ccbGetMousePosY");
	Scripting->addGlobalFunction(ccbGetScreenWidth,					"ccbGetScreenWidth");
	Scripting->addGlobalFunction(ccbGetScreenHeight,				"ccbGetScreenHeight");
	Scripting->addGlobalFunction(ccbInvokeAction,					"ccbInvokeAction");
	Scripting->addGlobalFunction(ccbDrawColoredRectangle,			"ccbDrawColoredRectangle");
	Scripting->addGlobalFunction(ccbDrawTextureRectangle,			"ccbDrawTextureRectangle");
	Scripting->addGlobalFunction(ccbDrawTextureRectangleWithAlpha,	"ccbDrawTextureRectangleWithAlpha");
	Scripting->addGlobalFunction(ccbEndProgram,						"ccbEndProgram");
	Scripting->addGlobalFunction(ccbSetCloseOnEscapePressed,		"ccbSetCloseOnEscapePressed");
	Scripting->addGlobalFunction(ccbSetCursorVisible,				"ccbSetCursorVisible");
	Scripting->addGlobalFunction(ccbSetActiveCamera,				"ccbSetActiveCamera");
	Scripting->addGlobalFunction(ccbGetActiveCamera,				"ccbGetActiveCamera");
	Scripting->addGlobalFunction(ccbGet3DPosFrom2DPos,				"ccbGet3DPosFrom2DPos");
	Scripting->addGlobalFunction(ccbGet2DPosFrom3DPos,				"ccbGet2DPosFrom3DPos");
	Scripting->addGlobalFunction(ccbGetCopperCubeVariable,			"ccbGetCopperCubeVariable");	
	Scripting->addGlobalFunction(ccbSetCopperCubeVariable,			"ccbSetCopperCubeVariable");	
	Scripting->addGlobalFunction(ccbDoHTTPRequestImpl,				"ccbDoHTTPRequestImpl");	
	Scripting->addGlobalFunction(ccbCancelHTTPRequest,				"ccbCancelHTTPRequest");
	Scripting->addGlobalFunction(ccbCreateMaterialImpl,				"ccbCreateMaterialImpl");	
	Scripting->addGlobalFunction(ccbSetShaderConstant,				"ccbSetShaderConstant");	
	Scripting->addGlobalFunction(ccbSetPhysicsVelocity,				"ccbSetPhysicsVelocity");		
	Scripting->addGlobalFunction(ccbUpdatePhysicsGeometry,			"ccbUpdatePhysicsGeometry");		
	Scripting->addGlobalFunction(ccbAICommand,						"ccbAICommand");		
	Scripting->addGlobalFunction(ccbSteamSetAchievement,			"ccbSteamSetAchievement");		
	Scripting->addGlobalFunction(ccbSteamResetAchievements,			"ccbSteamResetAchievements");		
	Scripting->addGlobalFunction(ccbGetCurrentNode,					"ccbGetCurrentNode");	
	Scripting->addGlobalFunction(ccbSwitchToFullscreen,				"ccbSwitchToFullscreen");		
	Scripting->addGlobalFunction(ccbSaveScreenshot,					"ccbSaveScreenshot");			
	Scripting->addGlobalFunction(ccbSwitchToCCBFile,				"ccbSwitchToCCBFile");	
	
	Scripting->addGlobalFunction(scriptEditorPrint,					"print");

	//Added by Vazahat (just_in_case)
	Scripting->addGlobalFunction(ccbSetMousePos,					"ccbSetMousePos");
	Scripting->addGlobalFunction(ccbRenderToTexture,				"ccbRenderToTexture");
	Scripting->addGlobalFunction(ccbSplitScreen,					"ccbSplitScreen");
	Scripting->addGlobalFunction(ccbSetGameTimerSpeed,				"ccbSetGameTimerSpeed");
	Scripting->addGlobalFunction(ccbEmulateKey,						"ccbEmulateKey");
	
	// Added by Robbo
	Scripting->addGlobalFunction(ccbSetTerrainTexHeight,			"ccbSetTerrainTexHeight");
	Scripting->addGlobalFunction(ccbSetTerrainBlending,			"ccbSetTerrainBlending");
		
	

	// also, implement the new ccbRegisterOnFrameEvent() functionality:

	const char* registerFrameFunctionality = 
		"var ccbRegisteredFunctionArray = new Array(); \n"\
		"function ccbRegisterOnFrameEvent(fobj) {	ccbRegisteredFunctionArray.push(fobj); }  \n"\
		"function ccbUnregisterOnFrameEvent(fobj) {	var pos = ccbRegisteredFunctionArray.indexOf(fobj); if (pos == -1) return; ccbRegisteredFunctionArray.splice(pos, 1); }  \n"\
		"function ccbInternalCallFrameEventFunctions() {	for (var i=0; i<ccbRegisteredFunctionArray.length; ++i) ccbRegisteredFunctionArray[i](); } \n";

	Scripting->executeCode(registerFrameFunctionality);

	// implement ccbDoHTTPRequest with callback functionality

	const char* doHTTPrequestFunctionality = 
		"var ccbRegisteredHTTPCallbackArray = new Array(); \n"\
		"function ccbDoHTTPRequest(url, fobj) { var id = ccbDoHTTPRequestImpl(url); if (fobj != null) { var f=new Object(); f.id=id; f.func = fobj; ccbRegisteredHTTPCallbackArray.push(f); } return id; } \n"\
		"function ccbDoHTTPRequestFinishImpl(id, data) { for (var i=0; i<ccbRegisteredHTTPCallbackArray.length; ++i) if (ccbRegisteredHTTPCallbackArray[i].id == id) { ccbRegisteredHTTPCallbackArray[i].func(data); ccbRegisteredHTTPCallbackArray.splice(i, 1); break; } } \n";

	Scripting->executeCode(doHTTPrequestFunctionality);

	// implement ccbCreateMaterial with callback functionality

	const char* ccbCreateMaterialFunctionality = 
		"var ccbShaderCallbackArray = new Array(); \n"\
		"function ccbCreateMaterial(vertexShader, fragmentShader, baseMaterialType, shaderCallback) { var id = -1; if (shaderCallback != null) { id = ccbShaderCallbackArray.length; ccbShaderCallbackArray.push(shaderCallback); } return ccbCreateMaterialImpl(vertexShader, fragmentShader, baseMaterialType, id); }\n"\
		"function ccbCallShaderCallbackImpl(idx) { ccbShaderCallbackArray[idx](); }\n";

	Scripting->executeCode(ccbCreateMaterialFunctionality);
}


long CPlayer::ccbRegisterKeyDownEvent(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount()>0)
	{
		irr::core::stringc name = attr->getAttributeAsString(0);
		LastPlayer->scriptRegisteredKeyDownFunction = name;
	}

	if (attr)
		attr->drop();
	
	return 0;
}


long CPlayer::ccbRegisterKeyUpEvent(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount()>0)
	{
		irr::core::stringc name = attr->getAttributeAsString(0);
		LastPlayer->scriptRegisteredKeyUpFunction = name;
	}

	if (attr)
		attr->drop();
	
	return 0;
}


long CPlayer::ccbRegisterMouseDownEvent(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount()>0)
	{
		irr::core::stringc name = attr->getAttributeAsString(0);
		LastPlayer->scriptRegisteredMouseDownFunction = name;
	}

	if (attr)
		attr->drop();
	
	return 0;
}


long CPlayer::ccbRegisterMouseUpEvent(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount()>0)
	{
		irr::core::stringc name = attr->getAttributeAsString(0);
		LastPlayer->scriptRegisteredMouseUpFunction = name;
	}

	if (attr)
		attr->drop();
	
	return 0;
}

/*long CPlayer::ccbRegisterOnFrameEvent(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount()>0)
	{
		irr::core::stringc name = attr->getAttributeAsString(0);
		LastPlayer->scriptRegisteredOnFrameFunction = name;
	}

	if (attr)
		attr->drop();
	
	return 0;
}*/

long CPlayer::ccbSwitchToScene(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount()>0)
	{
		irr::core::stringc name = attr->getAttributeAsString(0);
		LastPlayer->switchToScene(name.c_str());
	}

	if (attr)
		attr->drop();
	
	return 0;
}

long CPlayer::ccbGetMousePosX(irr::ScriptFunctionParameterObject obj)
{
	int pos = LastPlayer->Device->getCursorControl()->getPosition().X;

	LastPlayer->Scripting->setReturnValue(pos);
	return 1;
}

long CPlayer::ccbGetMousePosY(irr::ScriptFunctionParameterObject obj)
{
	int pos = LastPlayer->Device->getCursorControl()->getPosition().Y;

	LastPlayer->Scripting->setReturnValue(pos);
	return 1;
}

long CPlayer::ccbGetScreenWidth(irr::ScriptFunctionParameterObject obj)
{
	int pos = LastPlayer->Device->getVideoDriver()->getScreenSize().Width;

	LastPlayer->Scripting->setReturnValue(pos);
	return 1;
}


long CPlayer::ccbGetScreenHeight(irr::ScriptFunctionParameterObject obj)
{
	int pos = LastPlayer->Device->getVideoDriver()->getScreenSize().Height;

	LastPlayer->Scripting->setReturnValue(pos);
	return 1;
}


long CPlayer::ccbDrawColoredRectangle(irr::ScriptFunctionParameterObject obj)
{
	if (!LastPlayer->IsInScriptDrawCallback)
		return 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);


	if (attr && attr->getAttributeCount()==5)
	{
		irr::s32 clr = attr->getAttributeAsInt(0);
		irr::s32 x1 = attr->getAttributeAsInt(1);
		irr::s32 y1 = attr->getAttributeAsInt(2);
		irr::s32 x2 = attr->getAttributeAsInt(3);
		irr::s32 y2 = attr->getAttributeAsInt(4);

		irr::video::IVideoDriver* driver = LastPlayer->Device->getVideoDriver();
		driver->draw2DRectangle(clr, irr::core::rect<irr::s32>(x1,y1,x2,y2));
	}

	if (attr)
		attr->drop();

	return 0;
}



long CPlayer::ccbDrawTextureRectangle(irr::ScriptFunctionParameterObject obj)
{
	if (!LastPlayer->IsInScriptDrawCallback)
		return 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);


	if (attr && attr->getAttributeCount()==5)
	{
		irr::core::stringc tname = attr->getAttributeAsString(0);
		irr::s32 x1 = attr->getAttributeAsInt(1);
		irr::s32 y1 = attr->getAttributeAsInt(2);
		irr::s32 x2 = attr->getAttributeAsInt(3);
		irr::s32 y2 = attr->getAttributeAsInt(4);

		irr::video::IVideoDriver* driver = LastPlayer->Device->getVideoDriver();

		irr::video::ITexture* tex = driver->getTexture(tname);		
		if (tex)
		{
			driver->draw2DImage(tex, irr::core::rect<irr::s32>(x1,y1,x2,y2),
				irr::core::rect<irr::s32>(0, 0, tex->getSize().Width, tex->getSize().Height));
		}
	}

	if (attr)
		attr->drop();

	return 0;
}



long CPlayer::ccbDrawTextureRectangleWithAlpha(irr::ScriptFunctionParameterObject obj)
{
	if (!LastPlayer->IsInScriptDrawCallback)
		return 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);


	if (attr && attr->getAttributeCount()==5)
	{
		irr::core::stringc tname = attr->getAttributeAsString(0);
		irr::s32 x1 = attr->getAttributeAsInt(1);
		irr::s32 y1 = attr->getAttributeAsInt(2);
		irr::s32 x2 = attr->getAttributeAsInt(3);
		irr::s32 y2 = attr->getAttributeAsInt(4);

		irr::video::IVideoDriver* driver = LastPlayer->Device->getVideoDriver();

		irr::video::ITexture* tex = driver->getTexture(tname);		
		if (tex)
		{
			driver->draw2DImage(tex, irr::core::rect<irr::s32>(x1,y1,x2,y2),
				irr::core::rect<irr::s32>(0, 0, tex->getSize().Width, tex->getSize().Height), 0, 0, true);
		}
	}

	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbEndProgram(irr::ScriptFunctionParameterObject obj)
{
	LastPlayer->Device->closeDevice();
	return 0;
}

long CPlayer::ccbSetCloseOnEscapePressed(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 1)
	{
		LastPlayer->CloseOnEscape = attr->getAttributeAsBool(0);
		LastPlayer->updateTitle();
	}

	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbSetCursorVisible(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 1)
	{
		LastPlayer->Device->getCursorControl()->setVisible(attr->getAttributeAsBool(0));
	}

	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbSetActiveCamera(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 1)
	{
		irr::scene::ISceneNode* node = (irr::scene::ISceneNode*)attr->getAttributeAsUserPointer(0);
		if (node && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, node))
		{
			if (node->getType() == irr::scene::ESNT_CAMERA ||
				node->getType() == (irr::scene::ESCENE_NODE_TYPE)EFSNT_FLACE_CAMERA)
			{
				irr::scene::ICameraSceneNode* cam = (irr::scene::ICameraSceneNode*)node;
				LastPlayer->CurrentSceneManager->setActiveCamera(cam);
			}
		}
	}

	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbGet3DPosFrom2DPos(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 2 &&
		LastPlayer && LastPlayer->CurrentSceneManager)
	{
		int x = attr->getAttributeAsInt(0);		
		int y = attr->getAttributeAsInt(1);		

		irr::core::line3df line = 
			LastPlayer->CurrentSceneManager->getSceneCollisionManager()->getRayFromScreenCoordinates(irr::core::position2di(x,y));

		LastPlayer->Scripting->setReturnValue(irr::core::vector3df(line.end));
		returnCount = 1;
	}

	if (attr)
		attr->drop();

	return returnCount;
}



long CPlayer::ccbGet2DPosFrom3DPos(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 3 &&
		LastPlayer && LastPlayer->CurrentSceneManager)
	{
		irr::f32 x = attr->getAttributeAsFloat(0);		
		irr::f32 y = attr->getAttributeAsFloat(1);		
		irr::f32 z = attr->getAttributeAsFloat(2);		

		irr::core::position2di pos2d = 
			LastPlayer->CurrentSceneManager->getSceneCollisionManager()->getScreenCoordinatesFrom3DPosition(
			irr::core::vector3df(x,y,z));

		LastPlayer->Scripting->setReturnValue(irr::core::vector3df((irr::f32)pos2d.X, (irr::f32)pos2d.Y, 0));
		returnCount = 1;
	}

	if (attr)
		attr->drop();

	return returnCount;
}


long CPlayer::ccbRegisterBehaviorEventReceiver(irr::ScriptFunctionParameterObject obj)
{
	// parameters: bool forMouseEvents, bool forKeyboardEvents

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 2)
	{
		bool bForMouse = attr->getAttributeAsBool(0);
		bool bForKeyboard = attr->getAttributeAsBool(1);
		
		if (LastPlayer->CurrentlyRunningExtensionScript)
			LastPlayer->CurrentlyRunningExtensionScript->setAcceptsEvents(bForMouse, bForKeyboard);
	}

	if (attr)
		attr->drop();

	return 0;
}

long CPlayer::ccbCloneSceneNode(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 1)
	{
		irr::scene::ISceneNode* node = (irr::scene::ISceneNode*)attr->getAttributeAsUserPointer(0);
		if (node && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, node))
		{
			irr::scene::ISceneNode* clonedNode = node->clone();

			if (clonedNode)
			{
				setUniqueIdsAndNamesForClonedNodeAndItsChildren(node, clonedNode, clonedNode, LastPlayer->CurrentSceneManager, false);

				
				// also clone collision detection of the node in the world

				irr::scene::ITriangleSelector* selector = node->getTriangleSelector();
				if (selector)
				{
					irr::scene::ITriangleSelector* newSelector = selector->createClone(clonedNode);
					if (newSelector)
					{
						// set to node

						clonedNode->setTriangleSelector(newSelector);

						// also, copy into world

						if (LastPlayer->CurrentSceneManager->getWorldSceneCollision()) 
							LastPlayer->CurrentSceneManager->getWorldSceneCollision()->addTriangleSelector(newSelector);

						// done

						newSelector->drop();
					}
				}

				// return new node

				LastPlayer->Scripting->setReturnValue((void*)clonedNode);
				returnCount = 1;
			}
		}
	}

	if (attr)
		attr->drop();

	return returnCount;
}


long CPlayer::ccbGetActiveCamera(irr::ScriptFunctionParameterObject obj)
{
	if (!LastPlayer || !LastPlayer->CurrentSceneManager)
		return 0;

	LastPlayer->Scripting->setReturnValue((void*)LastPlayer->CurrentSceneManager->getActiveCamera());
	return 1;
}



long CPlayer::ccbGetCopperCubeVariable(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	if (!LastPlayer)
		return 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 1)
	{
		irr::core::stringc varname = attr->getAttributeAsString(0);

		ICCVariable* pVar = LastPlayer->getVariable(varname.c_str());
		if (pVar)
		{
			if (pVar->isString())
				LastPlayer->Scripting->setReturnValue(pVar->getValueAsString());
			else
			if (pVar->isFloat())
				LastPlayer->Scripting->setReturnValue(pVar->getValueAsFloat());
			else
			if (pVar->isInt())
				LastPlayer->Scripting->setReturnValue(pVar->getValueAsInt());
			else
				LastPlayer->Scripting->setReturnValue(0);

			pVar->drop();
		}
		else
			LastPlayer->Scripting->setReturnValue(0);

		returnCount = 1;
	}

	if (attr)
		attr->drop();

	return returnCount;
}


long CPlayer::ccbSetCopperCubeVariable(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	if (!LastPlayer)
		return 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 2)
	{
		irr::core::stringc varname = attr->getAttributeAsString(0);
		irr::io::E_ATTRIBUTE_TYPE t = attr->getAttributeType(1);
		ICCVariable* pVar = LastPlayer->getVariable(varname.c_str(), true);

		if (pVar)
		{
			if (t == irr::io::EAT_INT)
				pVar->setValueAsInt(attr->getAttributeAsInt(1));
			else
			if (t == irr::io::EAT_FLOAT)
				pVar->setValueAsFloat(attr->getAttributeAsFloat(1));
			else
				pVar->setValueAsString(attr->getAttributeAsString(1).c_str());


			LastPlayer->saveContentOfPotentialTemporaryVariableIntoSource(pVar);

			pVar->drop();
		}
	}

	if (attr)
		attr->drop();

	return returnCount;
}


//! C++ implementation of the script 'print'
long CPlayer::scriptEditorPrint(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr)
	{
		irr::core::stringw str = attr->getAttributeAsStringW(0);

		LastPlayer->debugPrintLine(str.c_str(), true);

		attr->drop();
	}

	return 0;
}

long CPlayer::ccbInvokeAction(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() >= 1)
	{
		irr::scene::ISceneNode* currentNode = 0;

		if (attr->getAttributeCount() >= 2)
		{
			irr::scene::ISceneNode* node = (irr::scene::ISceneNode*)attr->getAttributeAsUserPointer(1);
			if (node && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, node))
			{
				currentNode = node;
			}
		}

		if (!currentNode)
		{
			// unfortunately, all scripts test if current node is null, but we want this parameter to be optional.
			// so make the root scene node the current node if possible.
			if (LastPlayer->CurrentSceneManager)
				currentNode = LastPlayer->CurrentSceneManager->getRootSceneNode();
		}

		int storedActionId = attr->getAttributeAsInt(0);

		if (storedActionId >= 0 && 
			storedActionId < (int)LastPlayer->StoredExtensionScriptActionHandlers.size())
		{
			if (LastPlayer->StoredExtensionScriptActionHandlers[storedActionId])
				LastPlayer->StoredExtensionScriptActionHandlers[storedActionId]->execute(currentNode, LastPlayer->CurrentSceneManager);
		}
	}

	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbCancelHTTPRequest(irr::ScriptFunctionParameterObject obj)
{
	if (!LastPlayer || !LastPlayer->NetworkSupport)
		return 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 1)
	{
		int id = attr->getAttributeAsInt(0);	

		LastPlayer->NetworkSupport->cancelHTTPRequest(id);
	}

	if (attr)
		attr->drop();

	return 0;	
}

long CPlayer::ccbDoHTTPRequestImpl(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	if (!LastPlayer || !LastPlayer->NetworkSupport)
		return 0;

	irr::core::stringc url;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);
	if (!attr)
		return 0;

	if (attr && attr->getAttributeCount() >= 1)
	{
		url = attr->getAttributeAsString(0);
	}

	int ret = 0;

	if (url.size() > 0 && LastPlayer && LastPlayer->NetworkSupport)
	{
		irr::s32 id = LastPlayer->NetworkSupport->startHTTPRequest(url.c_str(), LastPlayer);

		LastPlayer->Scripting->setReturnValue(id);
		ret = 1;
	}

	if (attr)
		attr->drop();

	return ret;
}

long CPlayer::ccbCreateMaterialImpl(irr::ScriptFunctionParameterObject obj)
{
	// ccbCreateMaterial(vertexShader, fragmentShader, baseMaterialType, shaderCallbackIndex)
	// ccbSetShaderConstant(type, name, value1, value2, value3, value4)

	int returnCount = 0;

	if (!LastPlayer || !LastPlayer->NetworkSupport)
		return 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);
	if (!attr)
		return 0;

	int ret = 0;

	if (attr && attr->getAttributeCount() == 4)
	{
		irr::core::stringc strVShader = attr->getAttributeAsString(0);
		irr::core::stringc strPShader = attr->getAttributeAsString(1);
		irr::s32 baseMaterial = attr->getAttributeAsInt(2);
		irr::s32 shaderCallbackIndex = attr->getAttributeAsInt(3);

		// create material

		irr::s32 newMaterialType = -1;
		irr::video::IShaderConstantSetCallBack* callback = LastPlayer;

		newMaterialType = LastPlayer->Device->getVideoDriver()->getGPUProgrammingServices()->addHighLevelShaderMaterial(
			strVShader.size() == 0 ? 0 : strVShader.c_str(),
			"main",
			irr::video::EVST_VS_3_0,
			strPShader.size() == 0 ? 0 : strPShader.c_str(),
			"main",
			EPST_PS_3_0,
			callback,
			baseMaterial == -1 ? irr::video::EMT_SOLID : (irr::video::E_MATERIAL_TYPE)baseMaterial,
			shaderCallbackIndex);

		LastPlayer->Scripting->setReturnValue(newMaterialType);

		ret = 1;
	}
	else
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Wrong amount of arguments for ccbCreateMaterial().");

	if (attr)
		attr->drop();

	return ret;
}

long CPlayer::ccbSetShaderConstant(irr::ScriptFunctionParameterObject obj)
{
	if (!LastPlayer->CurrentMaterialRenderServices)
	{
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Your can call ccbSetShaderConstant() only during a shader callback.");
		return 0;
	}

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);
	if (!attr)
		return 0;

	// parameters:
	// type: 1/2
	// name: string
	// float1, float2, float3, float 4

	if (attr && attr->getAttributeCount() == 6)
	{
		irr::s32 type = attr->getAttributeAsInt(0);
		irr::core::stringc strVarName = attr->getAttributeAsString(1);
		irr::f32 f[4];
		f[0] = attr->getAttributeAsFloat(2);
		f[1] = attr->getAttributeAsFloat(3);
		f[2] = attr->getAttributeAsFloat(4);
		f[3] = attr->getAttributeAsFloat(5);

		if (type == 1)
			LastPlayer->CurrentMaterialRenderServices->setVertexShaderConstant(strVarName.c_str(), f, 4);
		else
		if (type == 2)
			LastPlayer->CurrentMaterialRenderServices->setPixelShaderConstant(strVarName.c_str(), f, 4);
		else
			LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Invalid shader type set for ccbSetShaderConstant().");
	}
	else
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Wrong amount of arguments for ccbSetShaderConstant().");

	if (attr)
		attr->drop();

	return 0;
}

// implements IShaderConstantSetCallBack
void CPlayer::OnSetConstants(irr::video::IMaterialRendererServices* services, irr::s32 userData)
{
	CurrentMaterialRenderServices = services;

	irr::video::IVideoDriver* driver = LastPlayer->Scripting->getIrrlichtDevice()->getVideoDriver();

	if (true) // needed for all drivers
	{
		// set default constants for D3D9/HLSL

		// set inverted world matrix

		core::matrix4 invWorld = driver->getTransform(video::ETS_WORLD);
		invWorld.makeInverse();
		services->setVertexShaderConstant("mInvWorld", invWorld.pointer(), 16, false);

		// set clip matrix

		core::matrix4 worldViewProj;
		worldViewProj = driver->getTransform(video::ETS_PROJECTION);
		worldViewProj *= driver->getTransform(video::ETS_VIEW);
		worldViewProj *= driver->getTransform(video::ETS_WORLD);

		services->setVertexShaderConstant("mWorldViewProj", worldViewProj.pointer(), 16, false);

		// set transposed world matrix

		core::matrix4 world = driver->getTransform(video::ETS_WORLD);
		world = world.getTransposed();
		services->setVertexShaderConstant("mTransWorld", world.pointer(), 16, false); 
		
		// set world matrix

		core::matrix4 mworld = driver->getTransform(video::ETS_WORLD);
		services->setVertexShaderConstant("mWorld", mworld.pointer(), 16, false);
		services->setVertexShaderConstant("vTangent", mworld.pointer(), 16, false);
	}

	// set constants for user created materials via scripting

	if (userData != -1)
		LastPlayer->Scripting->executeFunctionWithIntParam("ccbCallShaderCallbackImpl", userData);		

	CurrentMaterialRenderServices = 0;
}

long CPlayer::ccbSetPhysicsVelocity(irr::ScriptFunctionParameterObject obj)
{
	// parameters: sceneNode, x, y, z

	int returnCount = 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() == 4)
	{
		irr::scene::ISceneNode* node = (irr::scene::ISceneNode*)attr->getAttributeAsUserPointer(0);
		if (node && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, node))
		{
			irr::core::vector3df vel(attr->getAttributeAsFloat(1), attr->getAttributeAsFloat(2), attr->getAttributeAsFloat(3));

			CFlaceAnimatorCollisionResponse* colResp = (CFlaceAnimatorCollisionResponse*)node->findAnimator((irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_COLLISION_RESPONSE);
			CFlaceAnimatorRigidPhysicsBody* phys = (CFlaceAnimatorRigidPhysicsBody*)node->findAnimator((irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_RIGID_PHYSICS_BODY);

			if (colResp)
				colResp->setPhyiscLinearVelocity(vel);
			if (phys)
				phys->setPhyiscLinearVelocity(vel);
		}
	}

	if (attr)
		attr->drop();

	return 0;
}

long CPlayer::ccbUpdatePhysicsGeometry(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	irr::physics::IPhysicsSimulation* sim = LastPlayer->getPhysicsEngine();
	if (sim)
		sim->updateWorld();

	return 0;
}

long CPlayer::ccbAICommand(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() >= 2)
	{
		irr::scene::ISceneNode* node = (irr::scene::ISceneNode*)attr->getAttributeAsUserPointer(0);
		if (node && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, node))
		{
			irr::core::stringc strCommand = attr->getAttributeAsString(1);

			CFlaceAnimatorGameAI* ai = (CFlaceAnimatorGameAI*)node->findAnimator((irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_GAME_AI);

			if (strCommand == "cancel")
			{
				ai->aiCommandCancel(node);
			}
			else
			if (strCommand == "moveto")
			{
				irr::core::vector3df pos = attr->getAttributeAsVector3d(2);				
				ai->aiCommandMoveTo(node, pos);
			}
			else
			if (strCommand == "attack")
			{
				irr::scene::ISceneNode* targetnode = (irr::scene::ISceneNode*)attr->getAttributeAsUserPointer(2);
				if (targetnode && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, targetnode))
					ai->aiCommandAttack(node, targetnode);
			}
		}
	}

	if (attr)
		attr->drop();

	return 0;
}

long CPlayer::ccbSteamSetAchievement(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() >= 1)
	{
		irr::core::stringc strAc = attr->getAttributeAsString(0);
		if (strAc.size() > 0)
		{
			SteamSupport* pS = LastPlayer->getSteamSupport();
			if (pS)
				pS->SetAchievement(strAc.c_str());
		}
	}

	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbSteamResetAchievements(irr::ScriptFunctionParameterObject obj)
{
	SteamSupport* pS = LastPlayer->getSteamSupport();
	if (!pS)
		return 0;

	pS->ResetAchievements();

	return 0;
}


long CPlayer::ccbGetCurrentNode(irr::ScriptFunctionParameterObject obj)
{
	int returnCount = 0;

	irr::scene::ISceneNode* n = LastPlayer->getCurrentNode();

	if (n)
	{
		// return new node
		LastPlayer->Scripting->setReturnValue((void*)n);
		returnCount = 1;
	}

	return returnCount;
}

long CPlayer::ccbSwitchToFullscreen(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() >= 3)
	{
		bool bFullScreen = !attr->getAttributeAsBool(2);
		irr::core::dimension2di windowedSize;
		windowedSize.Width = LastPlayer->Win32PlayerInfo.ScreenSizeX;
		windowedSize.Height = LastPlayer->Win32PlayerInfo.ScreenSizeY;

		LastPlayer->Device->setResizeAble(!bFullScreen, bFullScreen, windowedSize);
	}

	if (attr)
		attr->drop();

	return 0;
}

long CPlayer::ccbSwitchToCCBFile(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() >= 1)
	{
		irr::core::stringc str = attr->getAttributeAsString(0);
		if (str.size())
		{
			LastPlayer->switchToCCBFile(str.c_str());			
		}
	}

	if (attr)
		attr->drop();

	return 0;
}

long CPlayer::ccbSaveScreenshot(irr::ScriptFunctionParameterObject obj)
{
	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() >= 1)
	{
		irr::core::stringc str = attr->getAttributeAsString(0);
		if (str.size())
		{
			irr::video::IImage* img = LastPlayer->Device->getVideoDriver()->createScreenShot();
			if (img)
			{
				LastPlayer->Device->getVideoDriver()->writeImageToFile(img, str.c_str());
				img->drop();
			}
		}
	}

	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbSetMousePos(irr::ScriptFunctionParameterObject obj)
{
	// this extension function was contributed by just_in_case for the Coppercube game engine and can be used freely

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount()==2)
	{
		
		int X = attr->getAttributeAsInt(0);
		int width = LastPlayer->Device->getVideoDriver()->getScreenSize().Width;
		int Y = attr->getAttributeAsInt(1);
		int height = LastPlayer->Device->getVideoDriver()->getScreenSize().Height;
		// Clamp to window size to prevent setting cursor outside current window
		if (X > width){X = width;}
		if (Y > height){Y = height;}
		LastPlayer->Device->getCursorControl()->setPosition(X,Y);
	}
	
	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbRenderToTexture(irr::ScriptFunctionParameterObject obj)
{
	// this extension function was contributed by just_in_case for the Coppercube game engine and can be used freely

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() >= 5)
	{
		irr::video::ITexture* rt = 0;
		irr::scene::ISceneNode* node = (irr::scene::ISceneNode*)attr->getAttributeAsUserPointer(0);
		irr::scene::ICameraSceneNode* cam = (irr::scene::ICameraSceneNode*)attr->getAttributeAsUserPointer(1);
		irr::scene::ICameraSceneNode* oldcam = LastPlayer->CurrentSceneManager->getActiveCamera();
		int texResX = attr->getAttributeAsInt(3);
		int texResY = attr->getAttributeAsInt(4);
		int matIndex = attr->getAttributeAsInt(2);
		if (node && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, node))
		{
			if (cam && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, cam))
			{
				if (cam->getType() == irr::scene::ESNT_CAMERA ||
					cam->getType() == (irr::scene::ESCENE_NODE_TYPE)EFSNT_FLACE_CAMERA)
					{
						if (LastPlayer->Device->getVideoDriver()->queryFeature(video::EVDF_RENDER_TO_TARGET))
						{
							rt = LastPlayer->Device->getVideoDriver()->addRenderTargetTexture(core::dimension2di(texResX,texResY));
							node->getMaterial(matIndex).setTexture(0, rt); // set material of cube to render target
						}
						else 
						LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not render to Target, current video driver doesn't support it");
						if (rt)
						{
							// draw scene into render target
							// set render target texture
							LastPlayer->Device->getVideoDriver()->setRenderTarget(rt, true, true,LastPlayer->CurrentSceneManager->getBackgroundColor());
							// make node invisible
							node->setVisible(false);
							LastPlayer->CurrentSceneManager->setActiveCamera(cam);
							// draw whole scene into render buffer
							LastPlayer->CurrentSceneManager->drawAll();
							// set back old render target
							
							LastPlayer->Device->getVideoDriver()->setRenderTarget(0, true, true,LastPlayer->CurrentSceneManager->getBackgroundColor());
							// make the node visible 
							node->setVisible(true);
							LastPlayer->CurrentSceneManager->setActiveCamera(oldcam);
							LastPlayer->CurrentSceneManager->drawAll();
						}
						LastPlayer->Device->getVideoDriver()->removeTexture(rt); //prevent from going out of video memory
					}
					else 
						LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not render to Target, provided camera is invalid");
			}
			else 
				LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not render to Target, provided camera is invalid");
		}
		else 
			LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not render to Target, provided scenenode is invalid");
	}
	else 
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not render to Target, Wrong amount of parameters");
	if (attr)
		attr->drop();
	return 0;
}


long CPlayer::ccbSplitScreen(irr::ScriptFunctionParameterObject obj)
{
	// this extension function was contributed by just_in_case for the Coppercube game engine and can be used freely

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);

	if (attr && attr->getAttributeCount() >= 10)
	{
		
		irr::scene::ICameraSceneNode* cam = (irr::scene::ICameraSceneNode*)attr->getAttributeAsUserPointer(0);
		irr::scene::ICameraSceneNode* cam2 = (irr::scene::ICameraSceneNode*)attr->getAttributeAsUserPointer(5);
		int ResX = LastPlayer->Device->getVideoDriver()->getScreenSize().Width;
		int ResY = LastPlayer->Device->getVideoDriver()->getScreenSize().Height;
		int X1 = attr->getAttributeAsInt(1);
		int Y1 = attr->getAttributeAsInt(2);
		int X2 = attr->getAttributeAsInt(3);
		int Y2 = attr->getAttributeAsInt(4);
		int X3 = attr->getAttributeAsInt(6);
		int Y3 = attr->getAttributeAsInt(7);
		int X4 = attr->getAttributeAsInt(8);
		int Y4 = attr->getAttributeAsInt(9);

		if (cam && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, cam))
		{
			if (cam2 && isSceneNodePointerValid(LastPlayer->CurrentSceneManager, cam2))
			{
				if (cam->getType() == irr::scene::ESNT_CAMERA ||
					cam->getType() == (irr::scene::ESCENE_NODE_TYPE)EFSNT_FLACE_CAMERA)
					{
						if (cam2->getType() == irr::scene::ESNT_CAMERA ||
						cam2->getType() == (irr::scene::ESCENE_NODE_TYPE)EFSNT_FLACE_CAMERA)
						{
							LastPlayer->Device->getVideoDriver()->setViewPort(irr::core::rect<s32>(0,0,ResX,ResY));
							LastPlayer->Device->getVideoDriver()->beginScene(true,true,SColor(255,100,100,100));
							
							LastPlayer->CurrentSceneManager->setActiveCamera(cam);
							LastPlayer->Device->getVideoDriver()->setViewPort(irr::core::rect<s32>(X1,Y1,X2,Y2));
							LastPlayer->CurrentSceneManager->drawAll();
							LastPlayer->Device->getVideoDriver()->setViewPort(irr::core::rect<s32>(X3,Y3,X4,Y4));
							
							LastPlayer->CurrentSceneManager->setActiveCamera(cam2);
							LastPlayer->CurrentSceneManager->drawAll();
						}
						else 
							LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not splitscreen, provided second scene node is not a camera.");
					}
					else 
						LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not splitscreen, provided first scene node is not a camera.");
			}
			else 
			LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not splitscreen, provided second scene node is invalid.");
		}
		else 
			LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not splitscreen, provided first scene node is invalid.");
		
		
	}
	else 
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not splitscreen, Wrong amount of parameters");
	if (attr)
		attr->drop();

	return 0;
}


long CPlayer::ccbSetGameTimerSpeed(irr::ScriptFunctionParameterObject obj)
{
	// this extension function was contributed by just_in_case for the Coppercube game engine and can be used freely

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);
	if (attr && attr->getAttributeCount() >= 1)
	{
		float Speed = attr->getAttributeAsFloat(0);
		ITimer* timer = (ITimer*)LastPlayer->Device->getTimer();
		timer->setSpeed(Speed);
	}
	else 
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not set game timer speed, Wrong amount of parameters");
	if (attr)
		attr->drop();
	return 0;
}

long CPlayer::ccbEmulateKey(irr::ScriptFunctionParameterObject obj)
{
	// this extension function was contributed by just_in_case for the Coppercube game engine and can be used freely

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);
	if (attr && attr->getAttributeCount() >= 2)
	{
		int keyCode = attr->getAttributeAsInt(0);
		bool pressDown = attr->getAttributeAsBool(1);
		SEvent emulate;
		emulate.EventType = irr::EET_KEY_INPUT_EVENT;
		emulate.KeyInput.PressedDown = pressDown;
		emulate.KeyInput.Key = irr::EKEY_CODE(keyCode);
		LastPlayer->Device->postEventFromUser(emulate);
	}
	else 
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("Can not set game timer speed, Wrong amount of parameters");
	if (attr)
		attr->drop();
	return 0;
}


long CPlayer::ccbSetTerrainTexHeight(irr::ScriptFunctionParameterObject obj)
{
	// this extension function was contributed by Robbo for the Coppercube game engine and can be used freely

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);
	if (attr && attr->getAttributeCount() == 3)
	{
		float TexLow = attr->getAttributeAsFloat(1); // Tex1 height
		float TexMed = attr->getAttributeAsFloat(2); // Tex2 height
		
		if (TexLow > 0.0f && TexLow < TexMed && TexMed < 1.0f)
		{
			CFlaceTerrainSceneNode* Terrain = (CFlaceTerrainSceneNode*)attr->getAttributeAsUserPointer(0);
			Terrain->setTextureHeights(TexLow, TexMed);
		}
		else
			LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("ERROR: heights must be a decimal value > 0 and < 1");
	}
	else
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("ERROR: requires three inputs - terrain node, tex0 height & tex1 height");

	if (attr)
		attr->drop();
	
	return 0;
}

long CPlayer::ccbSetTerrainBlending(irr::ScriptFunctionParameterObject obj)
{
	// this extension function was contributed by Robbo for the Coppercube game engine and can be used freely

	irr::io::IAttributes* attr = LastPlayer->Scripting->createParameterListFromScriptObject(obj);
	if (attr && attr->getAttributeCount() == 2)
	{
		float TexBlend = attr->getAttributeAsFloat(1);
		
			CFlaceTerrainSceneNode* Terrain = (CFlaceTerrainSceneNode*)attr->getAttributeAsUserPointer(0);
			Terrain->setTextureBlend(TexBlend);
	}
	else
		LastPlayer->Scripting->getIrrlichtDevice()->getLogger()->log("ERROR: requires 2 inputs");

	if (attr)
		attr->drop();
	
	return 0;
}