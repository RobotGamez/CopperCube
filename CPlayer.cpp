#include "CPlayer.h"

#ifdef WIN32
#define _WIN32_WINNT 0x0400 // for IsDebuggerPresent
#include <windows.h> // for GetModuleFileName
#else
#include <mach-o/dyld.h>
#endif 

#include "CFlaceDocument.h"
#include "CLimitReadFile.h"
#include "CFlace3DScene.h"
#include "CFlaceCameraSceneNode.h"
#include "CFlaceAnimatorCollisionResponse.h"
#include "CFlaceAnimatorCameraFPS.h"
#include "CFlaceAnimatorOnClick.h"
#include "CFlaceAnimatorGameAI.h"
#include "CFlaceAnimatorCameraModelViewer.h"
#include "CFlaceAnimator3rdPersonCamera.h"
#include "CFlaceAnimatorKeyboardControlled.h"
#include "CFlaceAnimatorExtensionScript.h"
#include "CCCActionShoot.h"
#include "CIrrEditServices.h"
#include "CFlaceVariable.h"
#include "irrHelpers.h"
#include "registryHelper.h"
#include "irrUnicodeSupport.h"
#include "CFlaceWarningAndErrorReceiver.h"
#include "IVideoStreamControl.h"
#include "INetworkSupport.h"
#include "steamsupport.h"
#include "CFlaceTerrainSceneNode.h"

#if defined(_IRR_SUPPORT_OCULUS_RIFT)
#include "COculusRiftSupport-0.3.h"
#endif // _IRR_SUPPORT_OCULUS_RIFT

//#define FLACE_DEBUG_RELEASE_BUILD

#if defined(_DEBUG) || defined(FLACE_DEBUG_RELEASE_BUILD)
	#define _FLACE_PLAYER_FORCE_LOAD_FROM_FILE
#endif

#if defined(_DEBUG) || defined(FLACE_DEBUG_RELEASE_BUILD)
	// #define _FLACE_PRINT_FRAMECOUNTER_TO_DEBUG_OUTPUT
#endif

#if !defined(_DEBUG) && defined(WIN32)
//#define _USE_DEBUGGER_PREVENTION 1
#endif

#ifndef WIN32
#include <irrklang.h>
void irrKlangPluginInit(irrklang::ISoundEngine* engine, const char* version);
#endif

CPlayer::CPlayer(bool debugMode, bool forceWindowed, bool bForceOculusRift, const char* scriptsource, irr::core::dimension2di* pForceResolution)
{
	LastPlayer = this;
	DebugMode = debugMode;
	ArchiveFile = 0;
	Device = 0;
	Document = 0;
	CurrentSceneManager = 0;
	NextCameraToSetActive = 0;
	CloseOnEscape = false;
	CollisionWorld = 0;
	DebugConsoleList = 0;
	Scripting = 0;
	SoundEngine = 0;
	IsInScriptDrawCallback = false;
	LicenseOverlay = 0;
	IsUsingOcculusRift = false;
	LastLicenseOverLayPosChange = 0;
	FirstFrameTime = 0;
	Services = 0;
	CurrentlyRunningExtensionScript = 0;
	OculusRiftSupport = 0;
	UsePhysics = false;
	CurrentPhysics = 0;
	NetworkSupport = 0;
	CurrentMaterialRenderServices = 0;
	TheSteamSupport = 0;
	CurrentNodeForScripting = 0;

	#ifdef _USE_DEBUGGER_PREVENTION
	// prevent people debugging this thing
	if (IsDebuggerPresent())
		return;
	#endif

	// check DX intaller

	doDXSetupIfNeeded();

	// init steam

	#ifdef COMPILE_WITH_STEAMSUPPORT
	TheSteamSupport = new SteamSupport();
	#endif // STEAMSUPPORT

	// load file meta data

	loadWin32PlayerInfo();


	// change settings to screenresolution if needed

	bool bTryScreenResolution = (Win32PlayerInfo.Flags1 & EWMTF_USE_DESKTOP_RESOLUTION) != 0;

	if (bTryScreenResolution && !forceWindowed && !pForceResolution)
		changeResolutionSettingsToScreenResolution();


	// find and initialize oculus rift device if wished

	#if defined(_IRR_SUPPORT_OCULUS_RIFT)
	if (bForceOculusRift || (Win32PlayerInfo.Flags1 & EWMTF_USE_OCULUS_RIFT) != 0)
	{
		COculusRiftSupport::globalOculusInit();

		OculusRiftSupport = new COculusRiftSupport();
		IsUsingOcculusRift = OculusRiftSupport->initBeforeWindowCreation();

		if (OculusRiftSupport->userWantedToQuit())
			return;
	}
	#endif // _IRR_SUPPORT_OCULUS_RIFT

	// disable post effects if needed

	if ((Win32PlayerInfo.Flags1 & EWMTF_DISABLE_POST_EFFECTS) != 0)
		irr::scene::Global_PostEffectsDisabled = true;


	// create device

	bool bRetryWithDifferentParametersIfFailed = true;

	irr::SIrrlichtCreationParameters params;
	params.AntiAlias = Win32PlayerInfo.AntiAlias != 0;
	params.Bits = Win32PlayerInfo.Bits;
	params.DriverType = (irr::video::E_DRIVER_TYPE)Win32PlayerInfo.DriverType;

	if (pForceResolution)
	{
		Win32PlayerInfo.ScreenSizeX = pForceResolution->Width;
		Win32PlayerInfo.ScreenSizeY = pForceResolution->Height;
	}

	if (params.DriverType == irr::video::EDT_SOFTWARE ||
		params.DriverType == irr::video::EDT_BURNINGSVIDEO)
	{
		#ifdef _WIN32
			params.DriverType = irr::video::EDT_DIRECT3D9;
		#else
			params.DriverType = irr::video::EDT_OPENGL;
		#endif
	}

	#ifndef _IRR_COMPILE_WITH_DIRECT3D_8_
	if (params.DriverType == irr::video::EDT_DIRECT3D8)
		params.DriverType = irr::video::EDT_DIRECT3D9;
	#endif

    params.Fullscreen = Win32PlayerInfo.FullScreen != 0;
	params.HighPrecisionFPU = true; // need to set it to true, otherwise Spidermonkey's Date() class stops working work after a D3D reset
	params.IgnoreInput = false;
	params.Stencilbuffer = Win32PlayerInfo.StencilBuffer != 0;
	params.Vsync = Win32PlayerInfo.VSync != 0;
	params.WindowSize.Width = Win32PlayerInfo.ScreenSizeX;
	params.WindowSize.Height = Win32PlayerInfo.ScreenSizeY;
	params.WithAlphaChannel = false;
	params.ZBufferBits = Win32PlayerInfo.ZBufferBits;
	params.EventReceiver = this;
	params.MaximizeButtonMeansSwitchToFullScreen = (Win32PlayerInfo.Flags1 & EWMTF_MAXIMIZE_MEANS_FULLSCREEN) != 0;
	
	if (forceWindowed)
		params.Fullscreen = false;

	if ((Win32PlayerInfo.Flags1 & EWMTF_USE_BORDERLESS_MODE) != 0)
		params.Fullscreen = false; // we switch to fullscreen after device has been created

	if (IsUsingOcculusRift)
	{
		// some forced settings needed to run with the oculus rift
		#if defined(_IRR_SUPPORT_OCULUS_RIFT)
			OculusRiftSupport->adjustIrrlichtCreationParameters(params);
			bRetryWithDifferentParametersIfFailed = false;
		#endif
	}

	Device = irr::createDeviceEx(params);

	if (!Device && bRetryWithDifferentParametersIfFailed)
	{
		for (int i=0; i<3; ++i)
		{
			if (i==1)
				params.Fullscreen = false;

			if (i==2)
				params.Stencilbuffer = false;

			#ifdef _IRR_COMPILE_WITH_DIRECT3D_8_
			params.DriverType = irr::video::EDT_DIRECT3D8;
			#else
			params.DriverType = irr::video::EDT_DIRECT3D9;
			#endif

			Device = irr::createDeviceEx(params);
			if (!Device)
			{
				params.DriverType = irr::video::EDT_OPENGL;
				Device = irr::createDeviceEx(params);
				if (!Device)
				{
					return;
				}
			}
		}
	}

	if (!Device)
	{
		#ifdef WIN32
			MessageBoxA(0, "Could not create device.\nUpdating your 3D drivers or using different settings might help.", "", MB_OK);
		#endif
		return;
	}		

	if ((Win32PlayerInfo.Flags1 & EWMTF_USE_BORDERLESS_MODE) != 0)
	{
		// switch to fullscreen / make resizable if necessary
		Device->setResizeAble(true,
							  Win32PlayerInfo.FullScreen != 0,
							  irr::core::dimension2di(Win32PlayerInfo.ScreenSizeX, Win32PlayerInfo.ScreenSizeY));
	}

	if (IsUsingOcculusRift)
	{
		// some forced settings needed to run with the oculus rift
		#if defined(_IRR_SUPPORT_OCULUS_RIFT)
		if (!OculusRiftSupport->onAfterDeviceCreated(Device))
		{
			IsUsingOcculusRift = false;
			#ifdef WIN32
				MessageBoxA(0, "The 3D hardware in this system doesn't seem to be capable of rendering for the Oculus Rift, sorry.", "", MB_OK);
			#endif
		}			
		#endif
	}

	// confuse crackers
	confuseCrackers = "Created with an unlicensed version of CopperCube";
	Device->getLogger()->log(confuseCrackers.c_str());


	// load loading image

	irr::video::ITexture* logoTexture = 0;

	if (Win32PlayerInfo.FlaceLoadingImageSize > 0)
	{
		ArchiveFile->seek(Win32PlayerInfo.OffsetFileStart + Win32PlayerInfo.FlaceDocumentSize);
		irr::io::CLimitReadFile* readFile = new irr::io::CLimitReadFile(ArchiveFile, Win32PlayerInfo.FlaceLoadingImageSize, "logo");
		if (readFile)
		{
			irr::video::IImage* img = Device->getVideoDriver()->createImageFromFile(readFile);
			if (img)
			{
				logoTexture = Device->getVideoDriver()->addTexture("#coppercube", img);

				img->drop();
			}

			readFile->drop();
		}
	}


	// set icon

	setAppIcon();


	// show "Loading..."

	irr::video::SColor bgColor;
	bgColor.color = Win32PlayerInfo.LoaderBackgroundColor;

	Device->getVideoDriver()->beginScene(true, true, bgColor);
	irr::core::dimension2di scrDim = Device->getVideoDriver()->getScreenSize();

	irr::core::stringw strLoadingTextC = Win32PlayerInfo.LoaderText;

	irr::s32 textPosY = scrDim.Height / 2;
	
	if (logoTexture)
	{
		irr::core::dimension2di dimLogo = logoTexture->getOriginalSize();

		if (false && dimLogo.Width > 800)
		{
			// full screen

			Device->getVideoDriver()->draw2DImage(logoTexture, 
				irr::core::rect<irr::s32>(0,0,scrDim.Width, scrDim.Height),
				irr::core::rect<irr::s32>(0,0,dimLogo.Width, dimLogo.Height));
		}
		else
		{
			// center below loading text

			irr::core::dimension2di dimLogoOnScreen = dimLogo;
			irr::core::position2di posLogo;
			posLogo.X = (scrDim.Width - dimLogoOnScreen.Width) / 2;			
			posLogo.Y = (scrDim.Height - dimLogoOnScreen.Height) / 2;		

			textPosY = posLogo.Y + dimLogoOnScreen.Height + 14;

			Device->getVideoDriver()->draw2DImage(logoTexture, 
				irr::core::rect<irr::s32>(posLogo.X,posLogo.Y, posLogo.X + dimLogoOnScreen.Width, posLogo.Y + dimLogoOnScreen.Height),
				irr::core::rect<irr::s32>(0,0,dimLogo.Width, dimLogo.Height), 0, 0, true, false);
		}
	}

	strLoadingTextC.replace("$PROGRESS$", "");
	irr::core::stringw strLoadingTextW = convertIrrC8ToW(strLoadingTextC);

	irr::video::SColor clrTxt = irr::video::SColor(255,255,255,255);
	if (bgColor.getLuminance() > 0.5f)
		clrTxt.set(255,0,0,0);

	Device->getGUIEnvironment()->getBuiltInFont()->draw(strLoadingTextW.c_str(), 
		irr::core::rect<irr::s32>(0, textPosY, scrDim.Width, textPosY), clrTxt, true, true);
	
	Device->getVideoDriver()->endScene();

	updateTitle(true); 

	irr::u32 loadingBeginTime = Device->getTimer()->getRealTime();


	// create audio

	SoundEngine = irrklang::createIrrKlangDevice();
	
	#ifdef __WXMAC__
	// mac: manually init mp3 plugin
	irrKlangPluginInit(soundEngine, IRR_KLANG_VERSION);
	#endif
	
	//int ikoptions = irrklang::ESEO_DEFAULT_OPTIONS | irrklang::ESEO_MUTE_IF_NOT_FOCUSED;
	//SoundEngine = irrklang::createIrrKlangDevice(irrklang::ESOD_AUTO_DETECT, ikoptions);


	// services

	Services = new irr::irredit::CIrrEditServices(Device, SoundEngine);


	// load flace document

	Document = new CFlaceDocument(Device, SoundEngine);

	CFlaceWarningAndErrorReceiver* warningReceiver = new CFlaceWarningAndErrorReceiver();
	Document->setWarningAndErrorReceiver(warningReceiver);

	irr::io::IReadFile* readFile = createReadFileForDocumentLoading();
	if (readFile)
	{
		Document->load(readFile);
		readFile->drop();
	}

	Document->setWarningAndErrorReceiver(0);

	

	// physics

	bool bForcePhysics = false;
	#if defined(_DEBUG) || defined(FLACE_DEBUG_RELEASE_BUILD)
		bForcePhysics = false;
	#endif

#ifdef _DEBUG
	Document->GetPublishSettings().Flags |= EFGS_SHOW_FPS_COUNTER; // force show fps
#endif

	#ifdef WIN32
	if (bForcePhysics || Document->GetPublishSettings().WindowsTarget.Flags & EWMTF_USE_PHYSICS_ENGINE)
	#else
	// macos
	if (Document->GetPublishSettings().MacOSXTarget.Flags & EWMTF_USE_PHYSICS_ENGINE)
	#endif
	{
		UsePhysics = true;
	}

	// network

	NetworkSupport = Services->createNetworkSupport();


	// set access points

	Services->setControlInterface(this);
	Services->setLastDocument(Document);


	// find license overlay
	LicenseOverlay = Device->getVideoDriver()->findTexture("#metal_42");


	// create console when in debug mode

	if (DebugMode)
	{
		int w = irr::core::min_(400,params.WindowSize.Width);
		int h = irr::core::min_(150,params.WindowSize.Height);
		int deltax = 0;
		int deltay = 0;
		if (IsUsingOcculusRift)
		{
			deltax -= (int)(params.WindowSize.Width * 0.5f);
			deltay = (int)(params.WindowSize.Height * 0.25f);
		}

		DebugConsoleList = Device->getGUIEnvironment()->addListBox(
			irr::core::rect<irr::s32>(
				params.WindowSize.Width - w + deltax,
				deltay, 
				params.WindowSize.Width + deltax, 
				h + deltay), 0, -1, true);

		DebugConsoleList->addItem(L"CopperCube Debug Console");
		DebugConsoleList->setVisible(false);
		DebugConsoleList->setAutoScrollEnabled(true);
	}


	// set title

	if (DebugMode)
		CloseOnEscape = true;

	updateTitle();


	// register scripts

	//Scripting = new irr::CSquirrelScriptEngine(Device, SoundEngine, true, true);
	Scripting = new irr::CSpidermonkeyScriptEngine(Device, SoundEngine, true, true, false);
	registerPlayerScripts();


	// show first scene
	switchToScene(Document->getCurrentScene());


	// run main script

	if (scriptsource)
	{
		// override main script from file
		FILE* file = fopen(scriptsource, "rb");
		if (file)
		{
			fseek(file, 0, SEEK_END);
			long fsize = ftell(file);
			fseek(file, 0, SEEK_SET);

			if (!fsize)
			{
				fclose(file);
			}
			else
			{
				char* tmp = new char[fsize+1];
				fread(tmp, fsize, 1, file);
				tmp[fsize] = 0x0;

				irr::core::stringc content = tmp;
				Document->setMainScript(content);
				delete [] tmp;
			}
		}
		else
		{
			irr::core::stringw str = L"Warning: could not open script file ";
			str += scriptsource;
			debugPrintLine(str.c_str());
		}
	}


	// load and execute main script from document
	if (Document->getMainScript().size())
		Scripting->executeCode(Document->getMainScript().c_str());


	// run extension scripts so they register themselves

	registerExtensionScripts(warningReceiver);

	
	warningReceiver->drop();


	// set oculus rift settings

	if (IsUsingOcculusRift)
	{
		#if defined(_IRR_SUPPORT_OCULUS_RIFT)
			OculusRiftSupport->setWorldUnitScale(Document->GetPublishSettings().OculusRiftSettings.WorldSizeScale);
		#endif
	}

	// show loading screen at least 4 seconds

	irr::u32 loadingEndTime = Device->getTimer()->getRealTime();
	irr::s32 remainingTimeToShowLoadingScreen = 4000 - (irr::s32)(loadingEndTime - loadingBeginTime);
	if (!DebugMode && remainingTimeToShowLoadingScreen > 0 && logoTexture && Device)
	{
		Device->sleep(irr::core::min_(4000, remainingTimeToShowLoadingScreen));
	}
}


CPlayer::~CPlayer()
{
	#ifdef COMPILE_WITH_STEAMSUPPORT
	if (TheSteamSupport)
		delete TheSteamSupport;
	TheSteamSupport = 0;
	#endif // STEAMSUPPORT

	if (Document)
		Document->drop();

	for (int i=0; i<(int)InitializedScenes.size(); ++i)
	{
		if (InitializedScenes[i].selector)
			InitializedScenes[i].selector->drop();	
	}

	for (int i=0; i<(int)StoredExtensionScriptActionHandlers.size(); ++i)
	{
		if (StoredExtensionScriptActionHandlers[i])
			StoredExtensionScriptActionHandlers[i]->drop();
	}
	StoredExtensionScriptActionHandlers.clear();

	setCurrentlyRunningExtensionScriptAnimator(0);

	clearVariables();

	endAllVideoStreams(false);

	#if defined(_IRR_SUPPORT_OCULUS_RIFT)
		delete OculusRiftSupport;
	#endif // _IRR_SUPPORT_OCULUS_RIFT

	if (SoundEngine)
		SoundEngine->drop();

	if (CurrentSceneManager)
		CurrentSceneManager->drop();
	CurrentSceneManager = 0;

	if (Scripting)
		Scripting->drop();

	if (Device)
		Device->drop();
	Device = 0;

	if (Services)
		Services->drop();
	
	if (NetworkSupport)
		NetworkSupport->drop();

	for (int i=0; i<(int)InitializedScenes.size(); ++i)
	{
		if (InitializedScenes[i].physics)
			InitializedScenes[i].physics->drop();			
	}

	if (ArchiveFile)
		ArchiveFile->drop();
	ArchiveFile = 0;
}



void CPlayer::registerExtensionScripts(CFlaceWarningAndErrorReceiver* warningReceiver)
{
	// run extension scripts so they register themselves

	for (int i=0; i<warningReceiver->getWarningCount(); ++i)
	{
		CFlaceWarningData* data = warningReceiver->getWarning(i);
		if (!data)
			continue;

		if (data->WarningId == EFEAWI_NON_EXISTING_EXTENSION_SCRIPT_FOUND)
		{
			Scripting->executeCode(data->HintText2.c_str(), data->HintText1.c_str());
		}
	}
}


void CPlayer::run()
{
	if (!Device)
		return;

	#ifdef _USE_DEBUGGER_PREVENTION
	// prevent people debugging this thing
	if (IsDebuggerPresent())
		return;
	#endif

	irr::video::IVideoDriver* driver = Device->getVideoDriver();

	FirstFrameTime = Device->getTimer()->getRealTime();
	irr::u32 wantedTimePerFrame = (irr::u32)(1000 / 60.0f);

	if (Win32PlayerInfo.WantedFPS > 0 && Win32PlayerInfo.WantedFPS < 300)
	// Win32PlayerInfo.WantedFPS = 60; Robbo
		wantedTimePerFrame = (irr::u32)(1000 / Win32PlayerInfo.WantedFPS);

	irr::u32 lastFrameBegin = FirstFrameTime - wantedTimePerFrame;
	irr::scene::ISceneManager* currentlyUsingSceneManager = 0; // we need this to grab() the currently scene, 
															// since it can be deleted in the middle of a draw() call by
															// an action (be reloadScene() for example)


	while(Device->run())
	{
		if (!Device->isWindowActive())
		{
			Device->sleep(100);
			continue;
		}

		// get time and limit frames

		irr::s32 currenttime = Device->getTimer()->getRealTime();

		irr::s32 wantedTimePerFrameNow = wantedTimePerFrame;
		if (IsUsingOcculusRift || isVideoPlaying()) // we need a much lower latency when using VR device or a playing video
			wantedTimePerFrameNow = (irr::s32)(wantedTimePerFrame / 2.0f);
		
		if (currenttime < (irr::s32)(lastFrameBegin + wantedTimePerFrameNow))
		{
			Device->sleep(0); // limit maximum frames per second, this is also useful for collision detection:
							  // if the movement lengths get too small, the player tends to get stuck
		}
		else 
		{
			lastFrameBegin = currenttime;


			// set background color

			irr::video::SColor backColor(255,0,0,0);

			if (Document)
			{
				CFlaceScene* scene = Document->getCurrentScene();
				if (scene)
					backColor = scene->getBackgroundColor();
			}

			currentlyUsingSceneManager = CurrentSceneManager;
			if (currentlyUsingSceneManager)
				currentlyUsingSceneManager->grab();


			// update network

			NetworkSupport->pumpMessages();


			// start drawing

			driver->beginScene(true, true, backColor);

			updateAllVideoStreams();

			if (CurrentPhysics)
				CurrentPhysics->calculateSimulationStep();

			if (IsUsingOcculusRift)
			{
				// override scene drawing with oculus drawing
				#if defined(_IRR_SUPPORT_OCULUS_RIFT)
				OculusRiftSupport->drawScene(CurrentSceneManager);
				#endif
			}
			else
			{
				if (CurrentSceneManager)
					CurrentSceneManager->drawAll();

				// draw scripting frame

				if (Scripting)
				{
					IsInScriptDrawCallback = true;
					Scripting->executeFunction("ccbInternalCallFrameEventFunctions");
					IsInScriptDrawCallback = false;
				}

				// license overlay

				drawLicenseOverlay();

				// end drawing

				Device->getGUIEnvironment()->drawAll();
			}
			

			driver->endScene();

			if (Scripting)
				Scripting->possiblyDoGC(); // give scripting engine some time for GC

			// update sound camera position

			irr::scene::ICameraSceneNode* cam = 0;
			if (CurrentSceneManager)
				cam = CurrentSceneManager->getActiveCamera();
			if (cam && SoundEngine)
			{
				irr::core::vector3df campos = cam->getAbsolutePosition();
				irr::core::vector3df camtarget = cam->getTarget();

				SoundEngine->setListenerPosition(campos, camtarget - campos, cam->getUpVector());
			}

			setNextActiveCameraIfNecessary();

			if (Document->GetPublishSettings().Flags & EFGS_SHOW_FPS_COUNTER)
				updateTitle(false);

			#ifdef _FLACE_PRINT_FRAMECOUNTER_TO_DEBUG_OUTPUT
			printFPSCountToDebugLog();
			#endif

			updateDebugConsoleSize();

			if (currentlyUsingSceneManager)
			{
				currentlyUsingSceneManager->drop();
				currentlyUsingSceneManager = 0;
			}
		}
	}
}


void CPlayer::loadWin32PlayerInfo()
{
	Win32PlayerInfo.setDefaults();

#ifdef WIN32
	//char filename[MAX_PATH];
	//if (!GetModuleFileName(NULL, filename, MAX_PATH))
	//	return;

	wchar_t filenamew[MAX_PATH];
	if (!GetModuleFileNameW(NULL, filenamew, MAX_PATH))
		return;

	irr::core::stringw strfilenamew = filenamew;
	irr::core::stringc strfilenamec = convertIrrWToC8(strfilenamew);
	char filename[MAX_PATH];
	strcpy(filename, strfilenamec.c_str());
#else
	// MACOS
	const int MAXPATHLENTT = 2048;
	char filename[MAXPATHLENTT+1];
	uint32_t path_len = MAXPATHLENTT;
	_NSGetExecutablePath(filename, &path_len);		
#endif 

	ArchiveFile = irr::io::createReadFile("test.exe");
	if (!ArchiveFile)
		return;
	
	ArchiveFile->seek(ArchiveFile->getSize() - sizeof(Win32PlayerInfo));
	ArchiveFile->read(&Win32PlayerInfo, sizeof(Win32PlayerInfo));

	if (Win32PlayerInfo.MagicId != Win32PlayerInfoMagicID)
		Win32PlayerInfo.setDefaults();

#ifdef _DEBUG
	// just for testing, force another driver
	//Win32PlayerInfo.DriverType =  irr::video::EDT_OPENGL;
	//Win32PlayerInfo.DriverType =  irr::video::EDT_DIRECT3D8;
	//Win32PlayerInfo.DriverType =  irr::video::EDT_BURNINGSVIDEO;
	
	Win32PlayerInfo.ScreenSizeX = 800;
	Win32PlayerInfo.ScreenSizeY = 480;

	Win32PlayerInfo.Flags1 |= EWMTF_USE_BORDERLESS_MODE;

	// Win32PlayerInfo.WantedFPS = 60;
#endif
}



void CPlayer::switchToScene(CFlaceScene* scene)
{
	if (!scene)
		return;

	CFlace3DScene* free3dscene = 0;
	if (scene->getType() == EFST_FREE_3D)
		free3dscene = (CFlace3DScene*)scene;

	if (CurrentSceneManager)
		CurrentSceneManager->drop();

	CurrentSceneManager = scene->getSceneManager();

	endAllVideoStreams(true);

	if (CurrentSceneManager)
	{
		CurrentSceneManager->grab();

		if (Scripting)
			Scripting->setSceneManager(CurrentSceneManager);

		// find if already initialized

		SInitializedSceneData* initData = 0;
		for (int i=0; i<(int)InitializedScenes.size(); ++i)
		{
			if (InitializedScenes[i].scene == scene)
			{
				initData = &InitializedScenes[i];
				break;
			}
		}

		// generate collision data if necessary

		if (initData)
		{
			CollisionWorld = initData->selector;
			CurrentPhysics = initData->physics;
		}
		else
		{
			CollisionWorld = scene->createCollisionGeometry(true);
			CurrentSceneManager->setWorldSceneCollision(CollisionWorld);
			setCollisionWorldForAllSceneNodes(CurrentSceneManager->getRootSceneNode());
		}


		// find a camera to activate. if there is none, create a new one

		if (!initData)
		{
			// create camera for new scene

			irr::scene::ICameraSceneNode* cam = scene->getFirstCameraToActivate();
			bool bCameraDoesNotWantCursor = false;
			
			if (cam)
			{
				// if there is a collision response animator, set the world

				const irr::core::list<irr::scene::ISceneNodeAnimator*>& animators = cam->getAnimators();
				for (irr::core::list<irr::scene::ISceneNodeAnimator*>::ConstIterator it = animators.begin();
					it != animators.end(); ++it)
				{
					irr::scene::ISceneNodeAnimator* anim = *it;

					if (anim->getType() == (irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_MODEL_VIEWER)
					{
						CFlaceAnimatorCameraModelViewer* animmodel = (CFlaceAnimatorCameraModelViewer*)anim;
						if (!animmodel->isMovementOnlyWhenMouseDown())
							bCameraDoesNotWantCursor = true;
					}
					else
					if (anim->getType() == (irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_FPS)
					{
						CFlaceAnimatorCameraFPS* animfps = (CFlaceAnimatorCameraFPS*)anim;
						if (!animfps->isMovementOnlyWhenMouseDown())
							bCameraDoesNotWantCursor = true;
					}
				}			

				cam->bindTargetAndRotation(true);
				CurrentSceneManager->setActiveCamera(cam);
			}
			else
			{
				// create a new camera with the wanted defaults

				irr::scene::ICameraSceneNode* node = 0;
				//	node = CurrentSceneManager->addCameraSceneNodeFPS(0, 300, scene->getType() == EFST_PANORAMA_CUBE ? 0 : 0.5f);

				{
					// we need to create a real flace camera for the correct properties.

					irr::scene::ICameraSceneNode* newcam = new CFlaceCameraSceneNode(0, CurrentSceneManager->getRootSceneNode(),
						CurrentSceneManager, -1);

					CFlaceAnimatorCameraFPS* anm = new CFlaceAnimatorCameraFPS(CurrentSceneManager, Device->getCursorControl());

					newcam->bindTargetAndRotation(true);
					newcam->addAnimator(anm);
					CurrentSceneManager->setActiveCamera(newcam);

					anm->drop();
					newcam->drop();

					node = newcam;
				}

				if (free3dscene)
				{
					node->setTarget(free3dscene->getDefaultCameraTarget());
					node->setPosition(free3dscene->getDefaultCameraPosition());
				}

				CurrentSceneManager->setActiveCamera(node);
				bCameraDoesNotWantCursor = true;
			}

			if (bCameraDoesNotWantCursor)
				Device->getCursorControl()->setVisible(false);
			else
				Device->getCursorControl()->setVisible(true);


			// create physics for this scene

			irr::physics::IPhysicsSimulation* physics = 0;
			
			if (UsePhysics)
				physics = Services->createPhysics(irr::physics::EPET_BULLET, CurrentSceneManager->getGravity());

			if ( physics )
			{
				// set world again, but without objects having a physics body attached

				irr::scene::IMetaTriangleSelector* physicsSelector = CurrentSceneManager->createMetaTriangleSelector();
				irr::s32 nCount = CollisionWorld->getTriangleSelectorCountInMetaTriangleSelector();
				for (irr::s32 iT=0; iT<nCount; ++iT)
				{
					irr::scene::ITriangleSelector* selector = CollisionWorld->getTriangleSelectorInMetaTriangleSelector(iT);
					irr::scene::ISceneNode* node = selector->getRelatedSceneNode();

					if (isSceneNodePointerValid(CurrentSceneManager, node) &&
						!node->findAnimator((irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_RIGID_PHYSICS_BODY) &&
						!node->findAnimator((irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_COLLISION_RESPONSE))
					{
						physicsSelector->addTriangleSelector(selector);
					}
				}

				physics->setWorld(physicsSelector);
				physicsSelector->drop();				
			}

			CurrentPhysics = physics;
			
			// store camera and physics for this new scene

			SInitializedSceneData data;
			data.scene = scene;
			data.selector = CollisionWorld;
			data.physics = physics;
			InitializedScenes.push_back(data);
		}
		else
		{
			// the scene was already initialized, activate again

			irr::scene::ICameraSceneNode* cam = CurrentSceneManager->getActiveCamera();
			bool bCameraDoesNotWantCursor = false;
			if (cam)
			{
				const irr::core::list<irr::scene::ISceneNodeAnimator*>& animators = cam->getAnimators();
				for (irr::core::list<irr::scene::ISceneNodeAnimator*>::ConstIterator it = animators.begin();
					it != animators.end(); ++it)
				{
					irr::scene::ISceneNodeAnimator* anim = *it;
					if (anim->getType() == irr::scene::ESNAT_CAMERA_FPS )
						bCameraDoesNotWantCursor = true;
					else
					if (anim->getType() == (irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_MODEL_VIEWER)
					{
						CFlaceAnimatorCameraModelViewer* animmodel = (CFlaceAnimatorCameraModelViewer*)anim;
						if (!animmodel->isMovementOnlyWhenMouseDown())
							bCameraDoesNotWantCursor = true;
					}
					else
					if (anim->getType() == (irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_FPS)
					{
						CFlaceAnimatorCameraFPS* animfps = (CFlaceAnimatorCameraFPS*)anim;
						if (!animfps->isMovementOnlyWhenMouseDown())
							bCameraDoesNotWantCursor = true;
					}
				}			
			}

			if (bCameraDoesNotWantCursor)
				Device->getCursorControl()->setVisible(false);
			else
				Device->getCursorControl()->setVisible(true);
		}
	}	
}

void CPlayer::debugPrintLine(const wchar_t* line, bool bForcePrintAlsoIfLineIsRepeating)
{
	#if defined(WIN32) && defined(_DEBUG)
		OutputDebugStringW(line);
		OutputDebugStringW(L"\n");
	#endif

	if (DebugConsoleList && line)
	{
		// check if this line is visible already in the last 10 lines

		if (!bForcePrintAlsoIfLineIsRepeating)
		{
			int count = DebugConsoleList->getItemCount();
			int lastItemsToCheck = 10;

			for (int i=irr::core::max_(0, count-lastItemsToCheck); i<count; ++i)
			{
				const wchar_t* pLine = DebugConsoleList->getListItem(i);
				if (pLine && !wcscmp(pLine, line))
					return;
			}
		}

		// add line

		if (DebugConsoleList->getItemCount() > 30)
			DebugConsoleList->removeItem(0);

		DebugConsoleList->addItem(line);
		DebugConsoleList->setSelected(DebugConsoleList->getItemCount() - 1);
		DebugConsoleList->setVisible(true);
	}
}

bool CPlayer::OnEvent(const irr::SEvent& event)
{
	if (event.EventType == irr::EET_LOG_TEXT_EVENT)
	{
		debugPrintLine(irr::core::stringw(event.LogEvent.Text).c_str());
		return true;
	}

	// on Escape pressed, close application 

	if (CloseOnEscape && event.EventType == irr::EET_KEY_INPUT_EVENT)
	{
		if (event.KeyInput.Key == irr::KEY_ESCAPE)
		{
			Device->closeDevice();
			return true;
		}
	}

	// special keys for oculus rift testing

	if (IsUsingOcculusRift &&
		event.EventType == irr::EET_KEY_INPUT_EVENT && !event.KeyInput.PressedDown )
	{
			#if defined(_IRR_SUPPORT_OCULUS_RIFT)
				if (OculusRiftSupport)
				{
					if (event.KeyInput.Key == irr::KEY_F11)
						OculusRiftSupport->toggleDistortionMode();
					#ifdef _DEBUG
					else
					if (event.KeyInput.Key == irr::KEY_F9)
					{
						// debug mode: toggle world unit scale
						float wus = OculusRiftSupport->getWorldUnitScale();
						float newwus = irr::core::equals(wus, 1.0f) ? (1.0f / 10.0f) : 1.0f;
						irr::core::stringc str = "new world scale: ";
						str += newwus;
						OculusRiftSupport->setWorldUnitScale(newwus);
						Device->getLogger()->log(str.c_str());
					}
					#endif
				}
				
			#endif
	}

	// send key event to the script handlers
	if (event.EventType == irr::EET_KEY_INPUT_EVENT &&
		Scripting)
	{
		if (event.KeyInput.PressedDown && scriptRegisteredKeyDownFunction.size())
			Scripting->executeFunctionWithIntParam(scriptRegisteredKeyDownFunction.c_str(), event.KeyInput.Key);

		if (!event.KeyInput.PressedDown && scriptRegisteredKeyUpFunction.size())
			Scripting->executeFunctionWithIntParam(scriptRegisteredKeyUpFunction.c_str(), event.KeyInput.Key);
	}

	// send mouse to the script handlers
	if (event.EventType == irr::EET_MOUSE_INPUT_EVENT &&
		Scripting)
	{
		if (scriptRegisteredMouseUpFunction.size() &&
			(event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP ||
			 event.MouseInput.Event == irr::EMIE_RMOUSE_LEFT_UP ||
			 event.MouseInput.Event == irr::EMIE_MMOUSE_LEFT_UP ) )
		{
			Scripting->executeFunctionWithIntParam(scriptRegisteredMouseUpFunction.c_str(), 
				(int)event.MouseInput.Event - irr::EMIE_LMOUSE_LEFT_UP);
		}
		else
		if (scriptRegisteredMouseDownFunction.size() &&
			(event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN ||
			 event.MouseInput.Event == irr::EMIE_RMOUSE_PRESSED_DOWN ||
			 event.MouseInput.Event == irr::EMIE_MMOUSE_PRESSED_DOWN ) )
		{
			Scripting->executeFunctionWithIntParam(scriptRegisteredMouseDownFunction.c_str(), 
				(int)event.MouseInput.Event - irr::EMIE_LMOUSE_PRESSED_DOWN);
		}
	}

	// send event to scene manager

	if (CurrentSceneManager && CurrentSceneManager->postEventFromUser(event))
		return true;	

	return false;
}


void CPlayer::switchToScene(const char* name)
{
	if (!Document || !name)
		return;

	for (int i=0; i<Document->getSceneCount(); ++i)
	{
		if (Document->getScene(i) && Document->getScene(i)->getName() == name)
		{
			switchToScene(Document->getScene(i));
			return;
		}
	}


	irr::core::stringw str = L"The scene does not exist: ";
	str += name;
	debugPrintLine(str.c_str());	
}


void CPlayer::updateTitle(bool loadingText)
{
	irr::core::stringw strTitle;
	
	if (Document)
		strTitle = Document->GetPublishSettings().ApplicationTitle;

	if (loadingText)
		strTitle += "CopperCube Engine, Loading...";
	else
	if (CloseOnEscape)
		strTitle += " - Testing. Press ESCAPE to close";

	if (Device && Document && 
		!loadingText && (Document->GetPublishSettings().Flags & EFGS_SHOW_FPS_COUNTER))
	{
		irr::s32 fps = Device->getVideoDriver()->getFPS();
		strTitle += " - FPS:";
		strTitle += fps;
	}

	if (Device && lastPrintedWindowTitle != strTitle)
	{
		Device->setWindowCaption(strTitle.c_str());
		lastPrintedWindowTitle = strTitle;
	}
}

void CPlayer::drawLicenseOverlay()
{
	if (!LicenseOverlay)
		return;

	irr::u32 now = Device->getTimer()->getRealTime();
	irr::video::IVideoDriver* driver = Device->getVideoDriver();

	if (!LastLicenseOverLayPosChange || 
		now - LastLicenseOverLayPosChange > 10000)
	{
		switch(rand()%3)
		{
		case 0:
			LicenseOverLayPosition.X = 0;
			LicenseOverLayPosition.Y = 0;
			break;
		case 1:
			LicenseOverLayPosition.X = 0;
			LicenseOverLayPosition.Y = 
				driver->getScreenSize().Height - LicenseOverlay->getOriginalSize().Height;
			break;
		default:
			LicenseOverLayPosition.X = 
				driver->getScreenSize().Width - LicenseOverlay->getOriginalSize().Width;
			LicenseOverLayPosition.Y = 
				driver->getScreenSize().Height - LicenseOverlay->getOriginalSize().Height;
			break;
		}
		
		LastLicenseOverLayPosChange = now;
	}

	if (now - FirstFrameTime > 5000)
	{
		irr::core::dimension2di size = LicenseOverlay->getOriginalSize();

		driver->draw2DImage(LicenseOverlay, irr::core::rect<irr::s32>(LicenseOverLayPosition.X,LicenseOverLayPosition.Y,
			LicenseOverLayPosition.X + size.Width, LicenseOverLayPosition.Y + size.Height),
			irr::core::rect<irr::s32>(0, 0, size.Width, size.Height), 0, 0, true);

		//driver->draw2DImage(LicenseOverlay, LicenseOverLayPosition);
	}
}


void CPlayer::setCollisionWorldForAllSceneNodes(irr::scene::ISceneNode* node)
{
	if (!node)
		return;

	const irr::core::list<irr::scene::ISceneNodeAnimator*>& animators = node->getAnimators();
	for (irr::core::list<irr::scene::ISceneNodeAnimator*>::ConstIterator it = animators.begin();
		it != animators.end(); ++it)
	{
		irr::scene::ISceneNodeAnimator* anim = *it;
		if (anim->getType() == (irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_3RD_PERSON)
		{
			irr::scene::CFlaceAnimator3rdPersonCamera* coll = (irr::scene::CFlaceAnimator3rdPersonCamera*)anim;
			coll->setWorld(CollisionWorld);
		}
		else
		if (anim->getType() == (irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_COLLISION_RESPONSE)
		{
			irr::scene::CFlaceAnimatorCollisionResponse* coll = (irr::scene::CFlaceAnimatorCollisionResponse*)anim;
			coll->setWorld(CollisionWorld);
		}
		else
		if (anim->getType() == (irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_ON_CLICK_DO_ACTION)
		{
			irr::scene::CFlaceAnimatorOnClick* coll = (irr::scene::CFlaceAnimatorOnClick*)anim;
			coll->setWorld(CollisionWorld);
		}
		else
		if (anim->getType() == (irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_GAME_AI)
		{
			irr::scene::CFlaceAnimatorGameAI* coll = (irr::scene::CFlaceAnimatorGameAI*)anim;
			coll->setWorld(CollisionWorld);
		}
	}		

	irr::core::list<irr::scene::ISceneNode*>::ConstIterator itChild = node->getChildren().begin();
	for (; itChild != node->getChildren().end(); ++itChild)
	{
		setCollisionWorldForAllSceneNodes((*itChild));
	}
}



void CPlayer::setActiveCameraNextFrame(irr::scene::ICameraSceneNode* cam)
{
	if (!cam)
		return;
	
	NextCameraToSetActive = cam;
}


void CPlayer::setNextActiveCameraIfNecessary()
{
	if (!NextCameraToSetActive || !CurrentSceneManager)
		return;

    if (!isSceneNodePointerValid(CurrentSceneManager, NextCameraToSetActive))
		return;

	CurrentSceneManager->setActiveCamera(NextCameraToSetActive);
	NextCameraToSetActive = 0;
}


//! finds a variable by name
ICCVariable* CPlayer::getVariable(const irr::c8* name, bool createIfNotExisting)
{
	if (!name)
		return 0;

	irr::core::stringc str = name;

	for (int i=0; i<(int)Variables.size(); ++i)
	{
		ICCVariable* var = Variables[i];
		if (var)
		{
			if (str.equals_ignore_case(var->getNameStr()))
			{
				var->grab();
				return var;
			}
		}
	}

	// for temporary virtual variables like "#player.health", create one now
	ICCVariable* tmpvar = createTemporaryVariableIfPossible(str);
	if (tmpvar)
		return tmpvar;

	// not found, create if needed
	if (createIfNotExisting)
	{
		CFlaceVariable* var = new CFlaceVariable();
		var->setName(name);
		Variables.push_back(var);
		var->grab();
		return var;
	}

	return 0;
}


void CPlayer::clearVariables()
{
	for (int i=0; i<(int)Variables.size(); ++i)
	{
		if (Variables[i])
			Variables[i]->drop();
	}

	Variables.clear();
}


bool CPlayer::getSceneNodeAndAttributeNameFromTemporaryVariableName(irr::core::stringc& varname, 
																	irr::scene::ISceneNode** pOutSceneNode,
																	irr::core::stringc& outAttributeName)
{
	if (varname.size() == 0)
		return false;

	if (!CurrentSceneManager)
		return false;

	// temporary virtual variables have the layout like "#player.health"
	if (varname[0] != '#')
		return false;

	irr::s32 pos = varname.findFirst('.');
	if (pos == -1)
		return false;

	// get attibute name

	outAttributeName = varname.subString(pos+1, varname.size()-pos);
	if (outAttributeName.size() == 0)
		return false;

	// find scene node

	irr::core::stringc sceneNodeName = varname.subString(1, pos-1);
	
	if (sceneNodeName == "system")
		return true;

	irr::scene::ISceneNode* node = CurrentSceneManager->getSceneNodeFromName(sceneNodeName.c_str());

	if (pOutSceneNode)
		*pOutSceneNode = node;

	return node != 0;
}

//! saves the content of a potential temporary variable into the source (where it came from) again
void CPlayer::saveContentOfPotentialTemporaryVariableIntoSource(ICCVariable* var)
{
	if (!var)
		return;

	irr::scene::ISceneNode* node = 0;
	irr::core::stringc attributeName;
	irr::core::stringc varname = var->getName();

	if (!getSceneNodeAndAttributeNameFromTemporaryVariableName(varname, &node, attributeName))
		return;

	if (attributeName == "health" && node)
	{
		irr::scene::CFlaceAnimatorGameAI* an = (irr::scene::CFlaceAnimatorGameAI*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_GAME_AI);
		if (an)
		{
			int healthBefore = an->getHealth();
			int healthNew = var->getValueAsInt();
			int damage = healthBefore - healthNew;
			if (damage > 0)
				an->OnHit(damage, node);
			else
				an->setHealth(healthNew);
		}
	}
	else
	if (attributeName == "movementspeed" && node)
	{
		irr::scene::CFlaceAnimatorGameAI* an = 
			(irr::scene::CFlaceAnimatorGameAI*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_GAME_AI);
		irr::scene::CFlaceAnimatorKeyboardControlled* an2 = 
			(irr::scene::CFlaceAnimatorKeyboardControlled*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_KEYBOARD_CONTROLLED);
		irr::scene::CFlaceAnimatorCameraFPS* an3 = 
			(irr::scene::CFlaceAnimatorCameraFPS*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_FPS);

		if (an3)
			an3->setMoveSpeed(var->getValueAsFloat());
		else
		if (an2)
			an2->setMoveSpeed(var->getValueAsFloat());
		else
		if (an)
			an->setMovementSpeed(var->getValueAsFloat());
	}
	else
	if (attributeName == "damage" && node)
	{
		const irr::core::list<irr::scene::ISceneNodeAnimator*>& animators = node->getAnimators();
		for (irr::core::list<irr::scene::ISceneNodeAnimator*>::ConstIterator it = animators.begin();
			it != animators.end(); ++it)
		{
			irr::scene::ISceneNodeAnimator* anim = *it;
			CCCActionShoot* shoot = (CCCActionShoot*)anim->findAction((int)ICCAT_SHOOT);
			if (shoot)
				shoot->setDamage(var->getValueAsInt());
		}
	}
	else
	if (attributeName == "colsmalldistance" && node)
	{
		CFlaceAnimatorCollisionResponse* colRes = (CFlaceAnimatorCollisionResponse*)
			node->findAnimator((irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_COLLISION_RESPONSE);

		if (colRes)
		{
			colRes->setUseFixedSlidingSpeed();
			colRes->setSlidingSpeed(var->getValueAsFloat());
		}
	}
	else
	if (attributeName == "soundvolume")
	{
		// system sound volume
		if (SoundEngine)
		{
			float f = var->getValueAsFloat() / 100.0f;
			f = irr::core::clamp(f, 0.0f, 1.0f);
			SoundEngine->setSoundVolume(f);
		}
	}
	else
	// Robbo added
	if (attributeName == "rotationspeed" && node)
	{
		irr::scene::CFlaceAnimatorCameraFPS* an3 = 
			(irr::scene::CFlaceAnimatorCameraFPS*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_FPS);

		if (an3)
			an3->setRotateSpeed(var->getValueAsInt());
	}
	else
	// Robbo added
	if (attributeName == "jumpspeed" && node)
	{
		irr::scene::CFlaceAnimatorCameraFPS* an3 = 
			(irr::scene::CFlaceAnimatorCameraFPS*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_FPS);

		if (an3)
			an3->setJumpSpeed(var->getValueAsFloat());
	}
	else
	// Robbo added
	if (attributeName == "canfly" && node)
	{
		irr::scene::CFlaceAnimatorCameraFPS* an3 = 
			(irr::scene::CFlaceAnimatorCameraFPS*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_FPS);

		if (an3)
			an3->setVerticalMovement(var->getValueAsString());
	}
}

ICCVariable* CPlayer::createTemporaryVariableIfPossible(irr::core::stringc& varname)
{
	irr::scene::ISceneNode* node = 0;
	irr::core::stringc attributeName;

	if (!getSceneNodeAndAttributeNameFromTemporaryVariableName(varname, &node, attributeName))
		return 0;

	CFlaceVariable* var = new CFlaceVariable();
	var->setName(varname.c_str());
	var->setValueAsInt(0); // default

	if (attributeName == "health")
	{
		irr::scene::CFlaceAnimatorGameAI* an = (irr::scene::CFlaceAnimatorGameAI*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_GAME_AI);
		if (an)
			var->setValueAsInt(an->getHealth());
	}
	else
	if (attributeName == "movementspeed" && node)
	{
		irr::scene::CFlaceAnimatorGameAI* an = 
			(irr::scene::CFlaceAnimatorGameAI*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_GAME_AI);
		irr::scene::CFlaceAnimatorKeyboardControlled* an2 = 
			(irr::scene::CFlaceAnimatorKeyboardControlled*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_KEYBOARD_CONTROLLED);
		irr::scene::CFlaceAnimatorCameraFPS* an3 = 
			(irr::scene::CFlaceAnimatorCameraFPS*)node->findAnimator((ESCENE_NODE_ANIMATOR_TYPE)EFAT_CAMERA_FPS);

		if (an3)
			var->setValueAsFloat(an3->getMoveSpeed());
		else
		if (an2)
			var->setValueAsFloat(an2->getMoveSpeed());
		else
		if (an)
			var->setValueAsFloat(an->getMovementSpeed());
	}
	else
	if (attributeName == "damage" && node)
	{
		const irr::core::list<irr::scene::ISceneNodeAnimator*>& animators = node->getAnimators();
		for (irr::core::list<irr::scene::ISceneNodeAnimator*>::ConstIterator it = animators.begin();
			it != animators.end(); ++it)
		{
			irr::scene::ISceneNodeAnimator* anim = *it;
			CCCActionShoot* shoot = (CCCActionShoot*)anim->findAction((int)ICCAT_SHOOT);
			if (shoot)
				var->setValueAsInt(shoot->getDamage());
		}
	}
	else
	if (attributeName == "colsmalldistance" && node)
	{
		CFlaceAnimatorCollisionResponse* colRes = (CFlaceAnimatorCollisionResponse*)
			node->findAnimator((irr::scene::ESCENE_NODE_ANIMATOR_TYPE)EFAT_COLLISION_RESPONSE);

		if (colRes)
			var->setValueAsFloat(colRes->getSlidingSpeed());
	}
	else
	if (attributeName == "soundvolume")
	{
		// system sound volume
		if (SoundEngine)
		{
			var->setValueAsFloat(irr::core::clamp(SoundEngine->getSoundVolume() * 100.0f, 0.0f, 100.0f));
		}
	}

	return var;
}


//! finds a variable by name
irr::IScriptEngine* CPlayer::getScriptEngine()
{
	return Scripting;
}

void filterNonAsciiSymbols(irr::core::stringw& inText)
{
	inText.replace(' ', '_');
	inText.replace('/', '_');
	inText.replace('\\','_');
	inText.replace(':', '_');
	inText.replace('*', '_');
	inText.replace('?', '_');
	inText.replace('"', '_');
	inText.replace('<', '_');
	inText.replace('>', '_');
	inText.replace('|', '_');

	for (int i=0; i<(int)inText.size(); ++i)
	{
		WebTextChar c = inText[i];

		if (!(c >= 'a' && c <= 'z') &&
			!(c >= 'A' && c <= 'Z') &&
			!(c >= '0' && c <= '9') &&
			!(c == '.'))
		{
			inText[i] = '_';
		}
	}
}


//! serializes (saves or loads) a variable
void CPlayer::serializeVariable(ICCVariable* var, bool load)
{
	if (!var)
		return;

	irr::core::stringw strTitle = "CopperCubeApp";	
	if (Document)
	{
		strTitle = Document->GetPublishSettings().ApplicationTitle;
		filterNonAsciiSymbols(strTitle);
	}

	irr::core::stringc pathraw = "Software\\Ambiera\\";
	pathraw += strTitle;
	const char* registryPath = pathraw.c_str();

	if (!openRegistry(registryPath, true))
		return;

	if (load)
	{
		const int strSz = 4084;
		_TCHAR stringBuf[strSz];
		if (readRegistryValue(registryPath, var->getName(), stringBuf, strSz-1))
			var->setValueAsString(stringBuf);
	}
	else
	{		
		writeRegistryValue(registryPath, var->getName(), var->getValueAsString());
	}

	closeRegistry(registryPath, true);
}

//! sets the currently running extension script behavior
void CPlayer::setCurrentlyRunningExtensionScriptAnimator(irr::scene::ISceneNodeAnimator* anim)  // implements ICCControlInterface
{
	if (CurrentlyRunningExtensionScript)
		CurrentlyRunningExtensionScript->drop();

	CFlaceAnimatorExtensionScript* newAnim = 0;

	if (anim && anim->getType() == (ESCENE_NODE_ANIMATOR_TYPE)EFAT_EXTENSION_SCRIPT)
		newAnim = (CFlaceAnimatorExtensionScript*)anim;

	CurrentlyRunningExtensionScript	= newAnim;
	if (CurrentlyRunningExtensionScript)
		CurrentlyRunningExtensionScript->grab();
}

irr::io::IReadFile* CPlayer::createReadFileForDocumentLoading()
{
    #ifndef WIN32
    // for testing on macos, enable this and replace the path below with your own example file:
    // irr::io::IReadFile* readFile = irr::io::createReadFile("/Users/niko/Development/transfer/test.ccb");
    return readFile;
    #endif
    
	if (Win32PlayerInfo.MagicId == Win32PlayerInfoMagicID)
	{
		// load from .exe
		ArchiveFile->seek(Win32PlayerInfo.OffsetFileStart);
		irr::io::CLimitReadFile* readFile = new irr::io::CLimitReadFile(ArchiveFile, Win32PlayerInfo.FlaceDocumentSize, "archive");
		return readFile;
	}
	else
	{
		#ifdef _FLACE_PLAYER_FORCE_LOAD_FROM_FILE
		// load from file (only for debugging purposes)
		irr::io::IReadFile* readFile = irr::io::createReadFile("test.ccb"); // Robbo
		return readFile;
		#endif
	}

	return 0;
}


//! closes the current file and loads and starts another one
void CPlayer::switchToCCBFile(const irr::c8* name) // implements ICCControlInterface
{
	if (!Document || !name)
		return;

	irr::io::IReadFile* readFile = irr::io::createReadFile(name);
	if (readFile)
	{
		// delete init data of old, previously loaded scene

		for (int i=0; i<(int)InitializedScenes.size(); ++i)
		{
			InitializedScenes[i].selector->drop();
			InitializedScenes.erase(i);
		}

		InitializedScenes.clear();

		// delete old document

		Document->drop();
		Document = 0;

		// delete old scene manager

		if (CurrentSceneManager)
			CurrentSceneManager->drop();

		CurrentSceneManager = 0;

		endAllVideoStreams(true);
			
		if (Scripting)
			Scripting->setSceneManager(0);

		// delete all unused textures

		irr::video::IVideoDriver* driver = Device->getVideoDriver();
		for (int i=0; i<(int)driver->getTextureCount(); ++i)
		{
			irr::video::ITexture* tex = driver->getTextureByIndex(i);
			if (tex->getReferenceCount() == 1)
			{
				driver->removeTexture(tex);
				--i;
			}
		}

		// load new document

		Document = new CFlaceDocument(Device, SoundEngine);

		CFlaceWarningAndErrorReceiver* warningReceiver = new CFlaceWarningAndErrorReceiver();
		Document->setWarningAndErrorReceiver(warningReceiver);

		Document->load(readFile);
		readFile->drop();

		Document->setWarningAndErrorReceiver(0);
		
		Services->setLastDocument(Document);

		// show first scene
		switchToScene(Document->getCurrentScene());		

		registerExtensionScripts(warningReceiver);

		warningReceiver->drop();
	}
}


//! reloads a scene from disk
void CPlayer::reloadScene(const irr::c8* name) // implements ICCControlInterface
{
	if (!Document || !name)
		return;

	CFlaceScene* scene = 0;

	int sceneIdx = -1;
	for (int i=0; i<Document->getSceneCount(); ++i)
	{
		CFlaceScene* sn = Document->getScene(i);
		if (sn && sn->getName() == name)
		{
			scene = sn;
			sceneIdx = i;
			break;
		}
	}

	if (sceneIdx == -1)
	{
		irr::core::stringw str = L"The scene does not exist: ";
		str += name;
		debugPrintLine(str.c_str());
		return;
	}

	// now finally reload that scene

	irr::io::IReadFile* readFile = createReadFileForDocumentLoading();
	if (readFile)
	{
		bool bIsCurrentlyActiveScene = CurrentSceneManager == scene->getSceneManager();

		if ( Document->reloadSceneFromFile(readFile, scene) == EFFLE_NO_ERROR)
		{
			// delete init data of old, previously loaded scene

			for (int i=0; i<(int)InitializedScenes.size(); ++i)
			{
				if (InitializedScenes[i].scene == scene)
				{
					InitializedScenes[i].selector->drop();
					InitializedScenes.erase(i);
					break;
				}
			}

			// set editor to be reloaded

			scene->getSceneManager()->getParameters()->setAttribute(
				irr::scene::IRR_SCENE_MANAGER_IS_RELOADED_INSTANCE, true);

			// switch to new scene if it was active before

			if (bIsCurrentlyActiveScene)
				switchToScene(scene);
		}

		readFile->drop();
	}
}

//! registers the action handler of an extension script and returns a unique id for it, so it can be referenced from the scripts
irr::s32 CPlayer::registerExtensionScriptActionHandler(ICCActionHandler* actionhandler)
{
	irr::s32 id = StoredExtensionScriptActionHandlers.linear_search(actionhandler);
	if (id != -1)
		return id;

	StoredExtensionScriptActionHandlers.push_back(actionhandler);

	if (actionhandler)
		actionhandler->grab();

	return (irr::s32)StoredExtensionScriptActionHandlers.size()-1;
}


void CPlayer::changeResolutionSettingsToScreenResolution()
{
	irr::IrrlichtDevice* nullDevice = irr::createDevice(video::EDT_NULL);

	if (!nullDevice)
		return;

	irr::core::dimension2d<irr::s32> sz = nullDevice->getVideoModeList()->getDesktopResolution();

	if (sz.Width > 0 && sz.Height > 0)
	{
		Win32PlayerInfo.ScreenSizeX = sz.Width;
		Win32PlayerInfo.ScreenSizeY = sz.Height;
	}

	nullDevice->drop();
}

void CPlayer::printFPSCountToDebugLog()
{
	if (Device && Document)
	{
		static irr::s32 lastFPSPrinted = -1;
		irr::s32 fps = Device->getVideoDriver()->getFPS();

		if (lastFPSPrinted != fps)
		{
			irr::core::stringc strText = "FPS:";
			strText += fps;
			strText += "\n";
			#if defined(WIN32)
			OutputDebugStringA(strText.c_str());
			#endif
		}
		
	}
}

//! returns currently used phyiscs engine
irr::physics::IPhysicsSimulation* CPlayer::getPhysicsEngine()  // implements ICCControlInterface
{
	return CurrentPhysics;
}

// implements ISceneNodeDeletionQueueClearCallback
void CPlayer::onNodeAndSelectorRemoved(irr::scene::ITriangleSelector* ts, irr::scene::ISceneNode* node)
{
	if (CurrentPhysics)
	{
		irr::scene::IMetaTriangleSelector* physicsWorld = (irr::scene::IMetaTriangleSelector*)CurrentPhysics->getWorld();

		if (physicsWorld)
			physicsWorld->removeTriangleSelector(ts);

		// unlink scene graph and physics. 
		
		CurrentPhysics->onNodeAndTriangleSelectorRemoved(ts, node);
	}
}

//! returns video stream control
irr::video::IVideoStreamControl* CPlayer::getActiveVideoStreamControlForFile(const irr::c8* filename, 
																			 bool createIfNotFound,
																			 ICCActionHandler* actionOnVideoEnded)
{
	endAllVideoStreams(true);
	
	for (int i=0; i<(int)ActiveVideoStreamControls.size(); ++i)
	{
		if (!strcmp(filename, ActiveVideoStreamControls[i].VideoStream->getStreamName()))
		{
			return ActiveVideoStreamControls[i].VideoStream;
		}
	}

	if (createIfNotFound)
	{
		irr::video::IVideoStreamControl* videoStream = Device->getVideoDriver()->createVideoStreamControl(filename);
		if (videoStream)
		{
			SVideoStreamData data;
			data.ActionOnEnd = actionOnVideoEnded;
			data.ActiveScene = CurrentSceneManager;
			data.VideoStream = videoStream;

			if (data.ActionOnEnd)
				data.ActionOnEnd->grab();

			ActiveVideoStreamControls.push_back(data);
		}

		return videoStream;
	}

	return 0;
}

void CPlayer::endAllVideoStreams(bool bOnlyEndedVideos)
{
	for (int i=0; i<(int)ActiveVideoStreamControls.size();)
	{
		SVideoStreamData& data = ActiveVideoStreamControls[i]; 
		bool bEraseStream = false;

		if (bOnlyEndedVideos)
		{
			if (data.VideoStream && data.VideoStream->hasPlayBackEnded())
				bEraseStream = true;
			else
			if (!data.VideoStream)
				bEraseStream = true;
		}
		else
			bEraseStream = true;

		if (bEraseStream)
		{
			if (data.VideoStream)
			{
				data.VideoStream->stop();
				data.VideoStream->drop();
			}

			if (data.ActionOnEnd)
				data.ActionOnEnd->drop();

			ActiveVideoStreamControls.erase(i);
		}
		else
			++i;
	}
}

void CPlayer::updateAllVideoStreams()
{
	for (int i=0; i<(int)ActiveVideoStreamControls.size(); ++i)
	{
		SVideoStreamData& data = ActiveVideoStreamControls[i];

		if (data.VideoStream)
		{
			// update

			data.VideoStream->update();

			// execute action on end if ended

			if (data.ActionOnEnd && 
				CurrentSceneManager == data.ActiveScene &&
				data.VideoStream->hasPlayBackEnded())
			{
				// grab and copy ActionOnEnd and set it to zero before executing, since execute() might start a new video this and modify the video steam array itself

				ICCActionHandler* onEnd = data.ActionOnEnd;
				onEnd->grab();
				data.ActionOnEnd->drop();
				data.ActionOnEnd = 0;

				onEnd->execute(CurrentSceneManager->getRootSceneNode(), CurrentSceneManager);

				onEnd->drop();				
			}
		}		
	}
}

bool CPlayer::isVideoPlaying()
{
	for (int i=0; i<(int)ActiveVideoStreamControls.size(); ++i)
	{
		if (ActiveVideoStreamControls[i].VideoStream &&
			!ActiveVideoStreamControls[i].VideoStream->hasPlayBackEnded())
			return true;
	}

	return false;
}


// implements INetworkRequestCallback
void CPlayer::OnRequestFinished(irr::s32 requestId, const irr::c8* pDataReceived, irr::s32 nDataSize)
{
	if (pDataReceived)
		Scripting->executeFunctionWithIntAndStringParam("ccbDoHTTPRequestFinishImpl", requestId, pDataReceived, nDataSize);
	else
		Scripting->executeFunctionWithIntAndStringParam("ccbDoHTTPRequestFinishImpl", requestId, "", 0);
}



//! returns network support
irr::net::INetworkSupport* CPlayer::getNetworkSupport()
{
	return NetworkSupport;
}

//! valve's steam
SteamSupport* CPlayer::getSteamSupport()
{
	return TheSteamSupport;
}

//! sets 'current' node executing for scripting
void CPlayer::setCurrentNode(irr::scene::ISceneNode* node)
{
	CurrentNodeForScripting = node;
}

//! gets 'current' node executing for scripting
irr::scene::ISceneNode* CPlayer::getCurrentNode()
{
	return CurrentNodeForScripting;
}


void CPlayer::setAppIcon()
{
	
	#ifdef WIN32
	{
		HINSTANCE hInstance = GetModuleHandle(0);
		HICON hIcon = LoadIcon(hInstance, "MAINICON"); //MAKEINTRESOURCE(1));

		if (hIcon != NULL)
		{
			HWND hwnd = 0;
			irr::video::IVideoDriver* driver = Device->getVideoDriver();

			switch(driver->getDriverType())
			{
            case EDT_OPENGL:
				hwnd = reinterpret_cast<HWND>(driver->getExposedVideoData().OpenGLWin32.HWnd);
                break;
            case EDT_DIRECT3D8:
                hwnd  = reinterpret_cast<HWND>(driver->getExposedVideoData().D3D8.HWnd);
                break;
            case EDT_DIRECT3D9:
                hwnd  = reinterpret_cast<HWND>(driver->getExposedVideoData().D3D9.HWnd);
                break;
			}

			SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
			SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        }
	}
	#endif
}


extern bool isNecessaryD3DXSDKInstalled();

void CPlayer::doDXSetupIfNeeded()
{
#ifdef WIN32

	// check if DX is installed

	bool bHasDXRuntimeInstalled = isNecessaryD3DXSDKInstalled();

	#ifdef _DEBUG
	//	bHasDXRuntimeInstalled = false; // just to show message
	#endif

	if (bHasDXRuntimeInstalled)
		return;


	// find installer path

	irr::core::stringc path = "dxredist";
	irr::core::stringw abspathw = getPathOfExe();
	abspathw.replace('/', '\\');
	if (abspathw.size() > 0 && abspathw[abspathw.size()-1] != '\\')
		abspathw += "\\";

	irr::core::stringw strInstallerFile = abspathw;
	strInstallerFile += "dxredist\\DXSETUP.exe";

	bool bExistsSetup = false;
	FILE* f = _wfopen(strInstallerFile.c_str(), L"rb");
	if (f) 
	{ 
		bExistsSetup = true;
		fclose(f);
	}
	
	if (!bExistsSetup)
		return; // nothing we can do


	// text file with message

	irr::core::stringw strInstallerMsgFile = abspathw;
	strInstallerMsgFile += "dxredist\\message.txt";
	irr::core::stringc strContent;
	readTextFileSimple(strInstallerMsgFile, strContent);

	if (!strContent.size())
		strContent = "This app needs DirectX to be installed. Install now?";

	if (MessageBoxA(0, strContent.c_str(), "", MB_OKCANCEL) == IDOK)
	{	
		// run installer as admin

		SHELLEXECUTEINFOW ShExecInfo = {0};
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
		ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		ShExecInfo.hwnd = NULL;
		ShExecInfo.lpVerb = L"runas";
		ShExecInfo.lpFile = strInstallerFile.c_str();        
		ShExecInfo.lpParameters = L"/silent";   
		ShExecInfo.lpDirectory = NULL;
		ShExecInfo.nShow = SW_SHOW;
		ShExecInfo.hInstApp = NULL; 
		ShellExecuteExW(&ShExecInfo);
		WaitForSingleObject(ShExecInfo.hProcess,INFINITE);
	}
#endif // WIN32	
}


void CPlayer::updateDebugConsoleSize()
{
	if (!DebugConsoleList || !Device)
		return;

	irr::core::dimension2di sz = Device->getVideoDriver()->getScreenSize();

	int w = irr::core::min_(800, (int)(sz.Width * 0.65f));
		
	int h = irr::core::min_(150, sz.Height);
	int deltax = 0;
	int deltay = 0;
	if (IsUsingOcculusRift)
	{
		deltax -= (int)(sz.Width * 0.5f);
		deltay = (int)(sz.Height * 0.25f);
	}

	irr::core::rect<irr::s32> rct(
		sz.Width - w + deltax,
		deltay,
		sz.Width + deltax,
		h + deltay);

	DebugConsoleList->setRelativePosition(rct);
}
