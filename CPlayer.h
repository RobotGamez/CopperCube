#pragma once

#include "irrlicht.h"
#include "SFlaceWin32PlayerInfo.h"
//#include "CSquirrelScriptEngine.h"
#include "CSpidermonkeyScriptEngine.h"
#include "irrklang.h"
#include "CIrrEditServices.h"
#include "ICCControlInterface.h"
#include "INetworkSupport.h"

class CFlaceDocument;
class CFlaceScene;
class COculusRiftSupport;
class SteamSupport;
class CFlaceWarningAndErrorReceiver;

namespace irr
{
	namespace scene
	{
		class CFlaceAnimatorExtensionScript;
	}
}

class CPlayer : public irr::IEventReceiver, 
	            public ICCControlInterface, 
				public irr::scene::ISceneNodeDeletionQueueClearCallback,
				public irr::net::INetworkRequestCallback,
				public irr::video::IShaderConstantSetCallBack
{
public:

	CPlayer(bool debugMode, bool forceWindowed, bool bForceOculusRift, const char* scriptsource = 0, irr::core::dimension2di* pForceResolution = 0);
	~CPlayer();

	void run();

	virtual bool OnEvent(const irr::SEvent& event);

	virtual void switchToScene(const char* name); // implements ICCControlInterface

	//! reloads a scene from disk
	virtual void reloadScene(const irr::c8* name); // implements ICCControlInterface

	//! closes the current file and loads and starts another one
	virtual void switchToCCBFile(const irr::c8* name); // implements ICCControlInterface

	virtual void setActiveCameraNextFrame(irr::scene::ICameraSceneNode* cam); // implements ICCControlInterface

	//! finds a variable by name
	virtual ICCVariable* getVariable(const irr::c8* name, bool createIfNotExisting=false); // implements ICCControlInterface

	//! saves the content of a potential temporary variable into the source (where it came from) again
	virtual void saveContentOfPotentialTemporaryVariableIntoSource(ICCVariable* var); // implements ICCControlInterface

	//! finds a variable by name
	virtual irr::IScriptEngine* getScriptEngine();  // implements ICCControlInterface

	//! serializes (saves or loads) a variable
	virtual void serializeVariable(ICCVariable* var, bool load=false);

	//! sets the currently running extension script behavior
	virtual void setCurrentlyRunningExtensionScriptAnimator(irr::scene::ISceneNodeAnimator* anim);  // implements ICCControlInterface

	//! registers the action handler of an extension script and returns a unique id for it, so it can be referenced from the scripts
	virtual irr::s32 registerExtensionScriptActionHandler(ICCActionHandler* actionhandler);

	//! returns currently used phyiscs engine
	virtual irr::physics::IPhysicsSimulation* getPhysicsEngine();  // implements ICCControlInterface

	// implements ISceneNodeDeletionQueueClearCallback
	virtual void onNodeAndSelectorRemoved(irr::scene::ITriangleSelector* ts, irr::scene::ISceneNode* node); 

	//! returns video stream control, implements ICCControlInterface
	virtual irr::video::IVideoStreamControl* getActiveVideoStreamControlForFile(const irr::c8* filename, bool createIfNotFound, ICCActionHandler* actionOnVideoEnded);

	//! returns network support, implements ICCControlInterface
	virtual irr::net::INetworkSupport* getNetworkSupport();

	//! sets 'current' node executing for scripting
	virtual void setCurrentNode(irr::scene::ISceneNode* node);

	//! gets 'current' node executing for scripting
	virtual irr::scene::ISceneNode* getCurrentNode();

	// implements INetworkRequestCallback
	virtual void OnRequestFinished(irr::s32 requestId, const irr::c8* pDataReceived, irr::s32 nDataSize);

	// implements IShaderConstantSetCallBack
	virtual void OnSetConstants(irr::video::IMaterialRendererServices* services, irr::s32 userData);

	//! valve's steam
	SteamSupport* getSteamSupport();

protected:

	void loadWin32PlayerInfo();
	void changeResolutionSettingsToScreenResolution();
	void switchToScene(CFlaceScene* scene);
	void debugPrintLine(const wchar_t* line, bool bForcePrintAlsoIfLineIsRepeating=false);
	void registerPlayerScripts();
	void updateTitle(bool loadingText=false);
	void drawLicenseOverlay();
	void setCollisionWorldForAllSceneNodes(irr::scene::ISceneNode* node);
	void setNextActiveCameraIfNecessary();
	void clearVariables();
	ICCVariable* createTemporaryVariableIfPossible(irr::core::stringc& varname);
	bool getSceneNodeAndAttributeNameFromTemporaryVariableName(irr::core::stringc& varname, irr::scene::ISceneNode** pOutSceneNode, irr::core::stringc& outAttributeName);
	irr::io::IReadFile* createReadFileForDocumentLoading();
	void printFPSCountToDebugLog();
	void endAllVideoStreams(bool bOnlyEndedVideos);
	void updateAllVideoStreams();
	bool isVideoPlaying();
	void setAppIcon();
	void doDXSetupIfNeeded();
	void updateDebugConsoleSize();

	void registerExtensionScripts(CFlaceWarningAndErrorReceiver* warningReceiver);

	static long ccbRegisterKeyDownEvent(irr::ScriptFunctionParameterObject obj);
	static long ccbRegisterKeyUpEvent(irr::ScriptFunctionParameterObject obj);
	static long ccbRegisterMouseDownEvent(irr::ScriptFunctionParameterObject obj);
	static long ccbRegisterMouseUpEvent(irr::ScriptFunctionParameterObject obj);
	//static long ccbRegisterOnFrameEvent(irr::ScriptFunctionParameterObject obj);
	static long ccbSwitchToScene(irr::ScriptFunctionParameterObject obj);
	static long ccbGetMousePosX(irr::ScriptFunctionParameterObject obj);
	static long ccbGetMousePosY(irr::ScriptFunctionParameterObject obj);
	static long ccbGetScreenWidth(irr::ScriptFunctionParameterObject obj);
	static long ccbGetScreenHeight(irr::ScriptFunctionParameterObject obj);	
	static long ccbInvokeAction(irr::ScriptFunctionParameterObject obj);	
	static long ccbDrawColoredRectangle(irr::ScriptFunctionParameterObject obj);
	static long ccbDrawTextureRectangle(irr::ScriptFunctionParameterObject obj);
	static long ccbDrawTextureRectangleWithAlpha(irr::ScriptFunctionParameterObject obj);
	static long ccbEndProgram(irr::ScriptFunctionParameterObject obj);
	static long ccbSetCloseOnEscapePressed(irr::ScriptFunctionParameterObject obj);	
	static long ccbSetCursorVisible(irr::ScriptFunctionParameterObject obj);	
	static long ccbSetActiveCamera(irr::ScriptFunctionParameterObject obj);	
	static long ccbGetActiveCamera(irr::ScriptFunctionParameterObject obj);	
	static long ccbGet3DPosFrom2DPos(irr::ScriptFunctionParameterObject obj);	
	static long ccbGet2DPosFrom3DPos(irr::ScriptFunctionParameterObject obj);	
	static long ccbCloneSceneNode(irr::ScriptFunctionParameterObject obj);	
	static long ccbRegisterBehaviorEventReceiver(irr::ScriptFunctionParameterObject obj);	
	static long ccbDoesLineCollideWithBoundingBoxOfSceneNode(irr::ScriptFunctionParameterObject obj);		
	static long ccbGetCollisionPointOfWorldWithLine(irr::ScriptFunctionParameterObject obj);		
	static long ccbGetCopperCubeVariable(irr::ScriptFunctionParameterObject obj);		
	static long ccbSetCopperCubeVariable(irr::ScriptFunctionParameterObject obj);	
	static long scriptEditorPrint(irr::ScriptFunctionParameterObject obj);
	static long ccbDoHTTPRequestImpl(irr::ScriptFunctionParameterObject obj);
	static long ccbCancelHTTPRequest(irr::ScriptFunctionParameterObject obj);	
	static long ccbCreateMaterialImpl(irr::ScriptFunctionParameterObject obj);		
	static long ccbSetShaderConstant(irr::ScriptFunctionParameterObject obj);		
	static long ccbSetPhysicsVelocity(irr::ScriptFunctionParameterObject obj);		
	static long ccbUpdatePhysicsGeometry(irr::ScriptFunctionParameterObject obj);			
	static long ccbAICommand(irr::ScriptFunctionParameterObject obj);		
	static long ccbSteamSetAchievement(irr::ScriptFunctionParameterObject obj);		
	static long ccbSteamResetAchievements(irr::ScriptFunctionParameterObject obj);		
	static long ccbGetCurrentNode(irr::ScriptFunctionParameterObject obj);			
	static long ccbSwitchToFullscreen(irr::ScriptFunctionParameterObject obj);				
	static long ccbSaveScreenshot(irr::ScriptFunctionParameterObject obj);
	static long ccbSwitchToCCBFile(irr::ScriptFunctionParameterObject obj);
	
	//Added by Vazahat (just_in_case)
	static long ccbSetMousePos(irr::ScriptFunctionParameterObject obj);
	static long ccbRenderToTexture(irr::ScriptFunctionParameterObject obj);
	static long ccbSplitScreen(irr::ScriptFunctionParameterObject obj);
	static long ccbSetGameTimerSpeed(irr::ScriptFunctionParameterObject obj);
	static long ccbEmulateKey(irr::ScriptFunctionParameterObject obj);
	
	//Added by  Robbo
	static long ccbSetTerrainTexHeight(irr::ScriptFunctionParameterObject obj);
	static long ccbSetTerrainBlending(irr::ScriptFunctionParameterObject obj);

	
	irr::IrrlichtDevice* Device;
	SFlaceWin32PlayerInfo Win32PlayerInfo;
	irr::io::IReadFile* ArchiveFile;
	CFlaceDocument* Document;
	irr::CSpidermonkeyScriptEngine* Scripting;
	irrklang::ISoundEngine* SoundEngine;
	irr::irredit::CIrrEditServices* Services;

	irr::gui::IGUIListBox* DebugConsoleList;

	bool CloseOnEscape;
	bool DebugMode;
	static CPlayer* LastPlayer;
	bool IsInScriptDrawCallback;
	bool IsUsingOcculusRift;

	irr::video::ITexture* LicenseOverlay;
	irr::u32 LastLicenseOverLayPosChange;
	irr::core::position2di LicenseOverLayPosition;
	irr::core::stringc confuseCrackers;
	irr::u32 FirstFrameTime;
	irr::core::stringc lastPrintedWindowTitle;

	struct SInitializedSceneData
	{
		CFlaceScene* scene;
		irr::scene::IMetaTriangleSelector* selector;
		irr::physics::IPhysicsSimulation* physics;
	};

	struct SVideoStreamData
	{
		irr::video::IVideoStreamControl* VideoStream;
		ICCActionHandler* ActionOnEnd;
		irr::scene::ISceneManager* ActiveScene;
	};

	irr::scene::ISceneManager* CurrentSceneManager;
	irr::scene::IMetaTriangleSelector* CollisionWorld;
	irr::core::array<SInitializedSceneData> InitializedScenes;
	irr::scene::ICameraSceneNode* NextCameraToSetActive;
	irr::core::array<ICCVariable*> Variables;

	irr::core::stringc scriptRegisteredKeyDownFunction;
	irr::core::stringc scriptRegisteredKeyUpFunction;
	irr::core::stringc scriptRegisteredMouseDownFunction;
	irr::core::stringc scriptRegisteredMouseUpFunction;

	irr::scene::CFlaceAnimatorExtensionScript* CurrentlyRunningExtensionScript;

	irr::core::array<ICCActionHandler*> StoredExtensionScriptActionHandlers; // array with action handlers of scripted extensions. 
																// they are stored in this order to be referenced later with the id as index.

	COculusRiftSupport* OculusRiftSupport;

	bool UsePhysics;

	irr::physics::IPhysicsSimulation* CurrentPhysics;
	irr::net::INetworkSupport* NetworkSupport;
	irr::scene::ISceneNode* CurrentNodeForScripting; // note: this is a handle, may not be valid pointer

	irr::core::array<SVideoStreamData> ActiveVideoStreamControls;

	irr::video::IMaterialRendererServices* CurrentMaterialRenderServices;

	SteamSupport* TheSteamSupport;
};