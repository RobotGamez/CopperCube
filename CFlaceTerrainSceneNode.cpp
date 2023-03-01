// Copyright (C) 2002-2008 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CFlaceTerrainSceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "ICameraSceneNode.h"
#include "os.h"
#include "CFlaceSerializer.h"
#include "CFlaceDeserializer.h"
#include "CFlaceAttributeSerializationHelper.h"
#include "irrHelpers.h"
#include "CFlaceMeshSceneNode.h"
#include "CFlaceAnimatedMeshSceneNode.h"

using namespace irr;
using namespace scene;

//! constructor
CFlaceTerrainSceneNode::CFlaceTerrainSceneNode(IUndoManager* undo, ISceneNode* parent, ISceneManager* mgr, irr::video::IVideoDriver* driver, s32 id)
: ISceneNode(parent, mgr, id, irr::core::vector3df(0,0,0), 
			 irr::core::vector3df(0,0,0), irr::core::vector3df(1,1,1), undo),
			 Driver(driver), GrassUsesWind(true)
{
	#ifdef _DEBUG
	setDebugName("CFlaceTerrainSceneNode");
	
	#endif

	LightingType = ETLT_DYNAMIC;
	CellCountX = 0;
	CellCountY = 0;
	TileCountX = 0;
	TileCountY = 0;
	CellSize = 0;
	SideLength = 0;
	CellsPerTileSide = 0;
	TextureScale = 0.02f;
	tTexHeightLow = 0.2f;
	tTexHeightMed = 0.9f;
	MaxHeight = 0;
	texBlend = 255;
	TileSize = 0;
	Displacement.set(0,0,0);

	recalculateBoundingBox();
}



//! destructor
CFlaceTerrainSceneNode::~CFlaceTerrainSceneNode()
{
	clearCurrentTerrainMeshes();
	clearTerrainTextures();
}


//! pre render event
void CFlaceTerrainSceneNode::OnRegisterSceneNode()
{
	setPosition(irr::core::vector3df(0,0,0));
	setRotation(irr::core::vector3df(0,0,0));
	setScale(irr::core::vector3df(1,1,1));

	if (IsVisible && DebugDataVisible)
		SceneManager->registerNodeForRendering(this);

	ISceneNode::OnRegisterSceneNode();
}


//! render
void CFlaceTerrainSceneNode::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();
	ICameraSceneNode* camera = SceneManager->getActiveCamera();

	if (!camera || !driver)
		return;

	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
}



//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32>& CFlaceTerrainSceneNode::getBoundingBox() const
{
	return BBox;
}


//! Creates a clone of this scene node and its children.
ISceneNode* CFlaceTerrainSceneNode::clone(ISceneNode* newParent, ISceneManager* newManager, IUndoManager* undo)
{
	if (!newParent)
		newParent = Parent;
	if (!newManager)
		newManager = SceneManager;

	CFlaceTerrainSceneNode* nb = new CFlaceTerrainSceneNode(undo, newParent, newManager, Driver, ID);

	nb->cloneMembers(this, newManager);

	nb->TerrainData = TerrainData;
	for (int i=0; i<(int)Textures.size(); ++i)
	{
		nb->Textures.push_back(Textures[i]);
		nb->Textures[i]->grab();
	}

	nb->BBox = BBox;

	nb->drop();
	return nb;
}



//! serialize
void CFlaceTerrainSceneNode::serialize(CFlaceSerializer* serializer)
{
	serializer->WriteBox(BBox);

	irr::s32 nFlags = 0;
	if (GrassUsesWind)
		nFlags |= 0x1;

	serializer->WriteS32(nFlags); // flags for future use	

	serializer->WriteS32(SideLength);
	serializer->WriteS32(CellSize);
	serializer->WriteS32(TileSize);
	serializer->WriteS32(TileCountX);
	serializer->WriteS32(TileCountY);
	serializer->WriteS32(CellCountX);
	serializer->WriteS32(CellCountY);
	serializer->WriteS32(CellsPerTileSide);
	serializer->WriteS32((irr::s32)LightingType);
	serializer->Write3DVectF(Displacement);
	serializer->WriteF32(TextureScale);

	serializer->WriteS32((irr::s32)Textures.size());
	for (int i=0; i<(int)Textures.size(); ++i)
		serializer->WriteTextureRef(Textures[i]);

	serializer->WriteS32((irr::s32)TerrainData.size());
	for (int i=0; i<(int)TerrainData.size(); ++i)
	{
		STerrainData& t = TerrainData[i];

		serializer->WriteF32(t.Height);
		serializer->WriteS32(t.UserSetTextureIndex);		
	}

	serializer->WriteS32((irr::s32)GrassInstances.size());
	for (int i=0; i<(int)GrassInstances.size(); ++i)
	{
		SGrassInstance& g = GrassInstances[i];

		serializer->WriteF32(g.Height);
		serializer->WriteF32(g.Width);		
		serializer->WriteF32(g.PosX);		
		serializer->WriteF32(g.PosZ);		
		serializer->WriteF32(g.Rotation);		
		serializer->WriteS32(g.TextureIndex);		
	}

	serializer->WriteS32((irr::s32)TerrainTiles.size());
	for (int i=0; i<(int)TerrainTiles.size(); ++i)
	{
		irr::s32 id = -1;
		CFlaceMeshSceneNode* tile = TerrainTiles[i];
		if (tile)
			id = tile->getID();
		serializer->WriteS32(id);
	}	
}


//! serialize
void CFlaceTerrainSceneNode::deserialize(CFlaceDeserializer* deserializer)
{
	BBox = deserializer->ReadBox();
	irr::s32 nFlags = deserializer->ReadS32(); // flags for future use

	GrassUsesWind = (nFlags & 0x1) != 0; 

	SideLength =  deserializer->ReadS32();
	CellSize = deserializer->ReadS32();
	TileSize = deserializer->ReadS32();
	TileCountX = deserializer->ReadS32();
	TileCountY = deserializer->ReadS32();
	CellCountX = deserializer->ReadS32();
	CellCountY = deserializer->ReadS32();
	CellsPerTileSide = deserializer->ReadS32();
	LightingType = (E_TERRAIN_LIGHTING_TYPE)deserializer->ReadS32();
	Displacement = deserializer->Read3DVectF();
	TextureScale = deserializer->ReadF32();

	irr::s32 texturesSize = deserializer->ReadS32();
	for (int i=0; i<texturesSize; ++i)
	{
		irr::video::ITexture* tex = deserializer->ReadTextureRef();
		if (tex)
			tex->grab();
		Textures.push_back(tex);
	}

	irr::s32 terrainDataSize = deserializer->ReadS32();
	for (int i=0; i<terrainDataSize; ++i)
	{
		STerrainData t;

		t.Height = deserializer->ReadF32();
		t.UserSetTextureIndex = deserializer->ReadS32();	

		TerrainData.push_back(t);
	}

	irr::s32 grassDataSize = deserializer->ReadS32();
	for (int i=0; i<grassDataSize; ++i)
	{
		SGrassInstance g;

		g.Height = deserializer->ReadF32();
		g.Width = deserializer->ReadF32();		
		g.PosX = deserializer->ReadF32();		
		g.PosZ = deserializer->ReadF32();		
		g.Rotation = deserializer->ReadF32();		
		g.TextureIndex = deserializer->ReadS32();	

		 GrassInstances.push_back(g);
	}

	irr::s32 terrainTileCount = deserializer->ReadS32();
	TemporaryTerrainTilesIds.clear();
	for (int i=0; i<terrainTileCount; ++i)
	{
		irr::s32 id = deserializer->ReadS32();	
		TemporaryTerrainTilesIds.push_back(id);
	}	

	// update

	calculateBlendingFactors();
}


//! Writes attributes of the scene node.
void CFlaceTerrainSceneNode::serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const
{
	if (options && (options->Flags & irr::io::EARWF_IRRLICHT_1_6_COMPATIBILTY))
	{
		return;
	}

	CFlaceAttributeSerializationHelper::serializeBaseAttributes(this, out, options);

	//out->addBool("IsClosedCircle", IsClosedCircle);
	//out->addFloat("Tightness", Tightness);

	out->addFloat("TextureScale", TextureScale);
	out->addBool("GrassUsesWind", GrassUsesWind);
}


//! Reads attributes of the scene node.
void CFlaceTerrainSceneNode::deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options)
{
	if (options && (options->Flags & irr::io::EARWF_IRRLICHT_1_6_COMPATIBILTY))
	{
		return;
	}

	CFlaceAttributeSerializationHelper::deserializeBaseAttributes(this, in, options);

	//IsClosedCircle = in->getAttributeAsBool("IsClosedCircle");
	//Tightness = in->getAttributeAsFloat("Tightness");

	bool bNeedsToRegenerateMesh = false;

	irr::f32 oldTextureScale = TextureScale;
	TextureScale = in->getAttributeAsFloat("TextureScale");

	if (!irr::core::equals(oldTextureScale, TextureScale))
		bNeedsToRegenerateMesh = true;

	bool bNewGrassUseswind = in->getAttributeAsBool("GrassUsesWind");
	if (bNewGrassUseswind != GrassUsesWind)
	{
		bNeedsToRegenerateMesh = true;
		GrassUsesWind = bNewGrassUseswind;
	}

	if (bNeedsToRegenerateMesh)
		updateMeshesFromTerrainData();
}


void CFlaceTerrainSceneNode::recalculateBoundingBox()
{
	BBox.reset(0,0,0);

	BBox.MaxEdge.set((irr::f32)SideLength, (irr::f32)MaxHeight, (irr::f32)SideLength);
	BBox.MaxEdge += Displacement;
	BBox.MinEdge += Displacement;
}


void CFlaceTerrainSceneNode::clearCurrentTerrainMeshes()
{
	for (int i=0; i<(int)TerrainTiles.size(); ++i)
	{
		if (TerrainTiles[i])
		{
			TerrainTiles[i]->remove();
			TerrainTiles[i]->drop();
		}
	}

	TerrainTiles.clear();
}

void CFlaceTerrainSceneNode::clearTerrainTextures()
{
	for (int i=0; i<(int)Textures.size(); ++i)
	{
		if (Textures[i])
			Textures[i]->drop();
	}

	Textures.clear();
}

//! terrain to generate
void CFlaceTerrainSceneNode::generateTerrain(irr::s32 sideLen, irr::s32 cellSize, irr::s32 maxHeight, int topology,
	irr::video::ITexture* pTextureGrass, 
	irr::video::ITexture* pTextureRock,
	irr::video::ITexture* pTextureSand,
	STreeDistribution* pTreeDistribution, irr::s32 nTreeDistributionCount,
	SGrassDistribution* pGrassDistribution, irr::s32 nGrassDistributionCount)
{
	clearCurrentTerrainMeshes();
	clearTerrainTextures();

	// save textures

	if (pTextureGrass)
	{
		Textures.push_back(pTextureGrass);
		pTextureGrass->grab();
	}

	if (pTextureRock)
	{
		Textures.push_back(pTextureRock);
		pTextureRock->grab();
	}

	if (pTextureSand)
	{
		Textures.push_back(pTextureSand);
		pTextureSand->grab();
	}

	// calculate sizes

	SideLength = sideLen;
	CellSize = cellSize;
	CellsPerTileSide = 35;
	MaxHeight = (int)maxHeight;
	TileSize = CellSize * CellsPerTileSide;

	Displacement.X = -(SideLength / 2.0f);
	Displacement.Y = 0.0f;
	Displacement.Z = -(SideLength / 2.0f);
	

	TileCountX = (int)(sideLen / (irr::f32)(TileSize));
	TileCountY = (int)(sideLen / (irr::f32)(TileSize));

	if (TileCountX <= 0) TileCountX = 1;
	if (TileCountY <= 0) TileCountY = 1;

	SideLength = TileCountX * TileSize;

	CellCountX = TileCountX * CellsPerTileSide;
	CellCountY = TileCountY * CellsPerTileSide;

	// set initial terrain data

	int nTotalCellCount = CellCountX * CellCountY;
	TerrainData.set_used_construct(nTotalCellCount);

	irr::f32 oneMeter = 10.0f;

	switch(topology)
	{
	case 0: // = hills
		{
			for (int cx=0; cx<CellCountX; ++cx)
				for (int cy=0; cy<CellCountY; ++cy)
				{			
					STerrainData& rTerrain = TerrainData[getTerrainCellIndex(cx,cy)];
					float x = (cx * (irr::f32)CellSize / oneMeter);
					float y = (cy * (irr::f32)CellSize / oneMeter);

					rTerrain.Height = 0.0f;
					rTerrain.UserSetTextureIndex = 0;
					rTerrain.MainTextureIndex = 0;
					rTerrain.BlendingToTextureIndex = 0;

					// tiny rippling effect:
					const irr::f32 fact = 2.0f;
					irr::f32 hx = sin(x / (irr::f32)(1000.0f / 925.0f));
					irr::f32 hy = cos(y / (irr::f32)(1000.0f / 925.0f));
					irr::f32 rippling = ((hx + hy) / 2.0f) * oneMeter * 1.1f;
					// bigger hills	
					irr::f32 hx2 = (sin(x / (irr::f32)(1000.0f / 150.0f)) * (irr::f32)(MaxHeight* 1.0f));
					irr::f32 hy2 = (cos(y / (irr::f32)(1000.0f / 150.0f)) * (irr::f32)(MaxHeight* 1.0f)); 
					irr::f32 hill = (hx2 + hy2) / 2.0f;
					rTerrain.Height = irr::core::clamp((hill + rippling), 0.0f, (irr::f32)MaxHeight);

					if (rTerrain.Height < MaxHeight / 100.0f)
					{
						const irr::f32 fact = 2.0f;
						irr::f32 hx = sin(x / (irr::f32)(1000.0f / 925.0f));
						irr::f32 hy = cos(y / (irr::f32)(1000.0f / 925.0f));
						irr::f32 smallripplingOnGround = ((hx + hy) / 2.0f) * oneMeter * 0.2f;

						rTerrain.Height += smallripplingOnGround  + ((irr::os::Randomizer::rand() % 1000) / oneMeter * 0.03f);
					}
				}
		}
		break;
	case 1: // = desert
		{
			for (int cx=0; cx<CellCountX; ++cx)
				for (int cy=0; cy<CellCountY; ++cy)
				{
					STerrainData& rTerrain = TerrainData[getTerrainCellIndex(cx,cy)];
					float x = (cx * (irr::f32)CellSize / oneMeter);
					float y = (cy * (irr::f32)CellSize / oneMeter);

					rTerrain.Height = 0.0f;
					rTerrain.UserSetTextureIndex = 0;
					rTerrain.MainTextureIndex = 0;
					rTerrain.BlendingToTextureIndex = 0;

					// tiny rippling effect:
					const irr::f32 fact = 2.0f;
					irr::f32 hx = sin(x / (irr::f32)(1000.0f / 925.0f));
					irr::f32 hy = cos(y / (irr::f32)(1000.0f / 925.0f));
					irr::f32 rippling = ((hx + hy) / 2.0f) * oneMeter * 1.1f;
					// bigger hills	
					irr::f32 hx2 = sin(x / (irr::f32)(1000.0f / 150.0f)) * (irr::f32)MaxHeight;
					irr::f32 hy2 = cos(y / (irr::f32)(1000.0f / 150.0f)) * (irr::f32)MaxHeight;
					irr::f32 hill = hx2 + hy2 / 2.0f;
					rTerrain.Height = irr::core::clamp((hill + rippling), 0.0f, (irr::f32)MaxHeight);

					if (rTerrain.Height < MaxHeight / 100.0f)
					{
						const irr::f32 fact = 2.0f;
						irr::f32 hx = sin(x / (irr::f32)(1000.0f / 925.0f));
						irr::f32 hy = cos(y / (irr::f32)(1000.0f / 925.0f));
						irr::f32 smallripplingOnGround = ((hx + hy) / 2.0f) * oneMeter * 0.2f;

						rTerrain.Height += smallripplingOnGround + ((irr::os::Randomizer::rand() % 1000) / oneMeter * 0.03f);
					}
				}
		}
		break;
	case 2: // = flat
		{
			for (int x=0; x<CellCountX; ++x)
				for (int y=0; y<CellCountY; ++y)
				{
					STerrainData& rTerrain = TerrainData[getTerrainCellIndex(x,y)];

					rTerrain.Height = 0.0f;
					rTerrain.UserSetTextureIndex = 0;
					rTerrain.MainTextureIndex = 0;
					rTerrain.BlendingToTextureIndex = 0;
				}
		}
		break;
	}


	// paint some textures onto them

	setThreeTexturesBasedOnHeight();


	// create grass patch positions

	GrassInstances.clear();

	if (pGrassDistribution)
		generateGrass(pGrassDistribution, nGrassDistributionCount);

	
	// create terrain meshes

	createTerrainSceneNodes();
	updateMeshesFromTerrainData();

	recalculateBoundingBox();
}


void CFlaceTerrainSceneNode::generateGrass(SGrassDistribution* pGrassDistribution, irr::s32 nGrassDistributionCount)
{
	if (pGrassDistribution)
	{
		for (int i=0; i<nGrassDistributionCount; ++i)
		{
			SGrassDistribution& rDist = pGrassDistribution[i];

			if (irr::core::iszero(rDist.percentOfTerrainCoveredWithThis) ||
				irr::core::iszero(rDist.height) ||
				irr::core::iszero(rDist.width) ||
				!rDist.Texture)
			{
				continue;
			}

			// find texture or add it 

			irr::s32 nTexIndex = findTextureIndexOrAddNewOne(rDist.Texture);
			
			// calculate grass distribution
			
			int nGrassPatchesWanted = (int)(((SideLength * SideLength) / (rDist.width * rDist.width)) * (rDist.percentOfTerrainCoveredWithThis * 2.0f));

			for (int nPatch=0; nPatch<nGrassPatchesWanted; ++nPatch)
			{
				const int fact = 100;
				irr::f32 posx = ((irr::os::Randomizer::rand()) % ((SideLength - CellSize) * fact)) / (irr::f32)fact;
				irr::f32 posy = ((irr::os::Randomizer::rand()) % ((SideLength - CellSize) * fact)) / (irr::f32)fact;

				int cellX = (int)(posx / CellSize);
				int cellY = (int)(posy / CellSize);

				irr::f32 height = getTerrain3DPositionClamped(cellX, cellY).Y;
				if (height > rDist.maxPosHeight)
					continue;

				// add grass patch

				SGrassInstance instance;
				instance.PosX = posx;
				instance.PosZ = posy;
				instance.Height = rDist.height;
				instance.Width = rDist.width;
				instance.Rotation = (irr::os::Randomizer::rand() % 1000) / 500.0f;
				instance.TextureIndex = nTexIndex;

				GrassInstances.push_back(instance);
			}
		}
	}
}

// added by Robbo
void CFlaceTerrainSceneNode::setTextureHeights(irr::f32 tTexLow, irr::f32 tTexMed)
{
	tTexHeightLow = tTexLow;
	tTexHeightMed = tTexMed;
	
	// recalculate MaxHeight as may not be in memory
	createTerrainDataSnapshot();
	
	// redo 3 textures and blending
	setThreeTexturesBasedOnHeight();
	updateMeshesFromTerrainData();
	
	// check vertex normals
	// irr::u16 nIndexStart = buf->Vertices.size();
	
	if (tTexHeightLow == 0)
	{
		for (int vert=0; vert<Vertices.size(); ++vert)
		{
			irr::core::vector3df vert = buf->Vertices.normal;
			float vLen = vert.getLength();
			float angle = acos(vert.z/vLen);
		}
	}

}

// added by Robbo
void CFlaceTerrainSceneNode::setTextureBlend(irr::s32 TexBlend)
{
	texBlend = TexBlend;
	calculateBlendingAll(0,0,CellCountX,CellCountY);
}


void CFlaceTerrainSceneNode::setThreeTexturesBasedOnHeight()
{
	for (int x=0; x<CellCountX; ++x)
		for (int y=0; y<CellCountY; ++y)
		{
			STerrainData& rTerrain = TerrainData[getTerrainCellIndex(x,y)];

			if (rTerrain.Height < MaxHeight * tTexHeightLow) // RC
				rTerrain.UserSetTextureIndex = 0;
			else
			if (rTerrain.Height < MaxHeight * tTexHeightMed) // RC
				rTerrain.UserSetTextureIndex = 1;
			else
				rTerrain.UserSetTextureIndex = 2;

			rTerrain.BlendFactorPerVertex[0] = 0;
			rTerrain.BlendFactorPerVertex[1] = 0;
			rTerrain.BlendFactorPerVertex[2] = 0;
			rTerrain.BlendFactorPerVertex[3] = 0;
		}

	calculateBlendingFactors();
}


//! terrain to generate
//! generates new terrain from heightmap
void CFlaceTerrainSceneNode::loadHeightMap(irr::s32 sideLenX, irr::s32 sideLenY, irr::s32 cellSize, irr::f32* pData, 
		irr::video::ITexture* pTextureGrass, irr::video::ITexture* pTextureRock,
		irr::video::ITexture* pTextureSand, irr::video::ITexture* pGrassSpriteTexture)
{
	clearCurrentTerrainMeshes();
	clearTerrainTextures();

	// save textures

	if (pTextureGrass)
	{
		Textures.push_back(pTextureGrass);
		pTextureGrass->grab();
	}

	if (pTextureRock)
	{
		Textures.push_back(pTextureRock);
		pTextureRock->grab();
	}

	if (pTextureSand)
	{
		Textures.push_back(pTextureSand);
		pTextureSand->grab();
	}

	TextureScale = 0.006f;

	// calculate sizes

	CellSize = cellSize;
	CellsPerTileSide = 35;
	SideLength = irr::core::max_(sideLenX, sideLenY) * CellSize;	
	MaxHeight = 0; 
	TileSize = CellSize * CellsPerTileSide;

	Displacement.X = -(SideLength / 2.0f);
	Displacement.Y = 0.0f;
	Displacement.Z = -(SideLength / 2.0f);
	
	TileCountX = (int)(SideLength / (irr::f32)(TileSize));
	TileCountY = (int)(SideLength / (irr::f32)(TileSize));

	if (TileCountX <= 0) TileCountX = 1;
	if (TileCountY <= 0) TileCountY = 1;

	SideLength = TileCountX * TileSize;

	CellCountX = TileCountX * CellsPerTileSide;
	CellCountY = TileCountY * CellsPerTileSide;

	// set initial terrain data

	int nTotalCellCount = CellCountX * CellCountY;
	TerrainData.set_used_construct(nTotalCellCount);

	for (int cx=0; cx<CellCountX; ++cx)
		for (int cy=0; cy<CellCountY; ++cy)
		{			
			STerrainData& rTerrain = TerrainData[getTerrainCellIndex(cx,cy)];

			int x = irr::core::clamp(cx, 0, sideLenX);
			int y = irr::core::clamp(cy, 0, sideLenY);

			rTerrain.Height = pData[(y * sideLenX) + x];
			rTerrain.UserSetTextureIndex = 0;
			rTerrain.MainTextureIndex = 0;
			rTerrain.BlendingToTextureIndex = 0;

			MaxHeight = (int)irr::core::max_((irr::f32)MaxHeight, rTerrain.Height);
		}	

	if (pTextureGrass && pTextureRock && pTextureSand)
		setThreeTexturesBasedOnHeight();
	else
		calculateBlendingFactors();


	// create grass 

	GrassInstances.clear();

	if (pGrassSpriteTexture)
	{
		SGrassDistribution GrassDistribution;
		GrassDistribution.percentOfTerrainCoveredWithThis = 0.8f;
		GrassDistribution.Texture = pGrassSpriteTexture;
		GrassDistribution.height = 17.0f;
		GrassDistribution.width = 20.0f;
		GrassDistribution.maxPosHeight = MaxHeight * tTexHeightLow * 0.9f; // RC

		generateGrass(&GrassDistribution, 1);
	}
	
	// create terrain meshes

	createTerrainSceneNodes();
	updateMeshesFromTerrainData();

	recalculateBoundingBox();
}


irr::s32 CFlaceTerrainSceneNode::getTerrainMeshIndex(irr::s32 tileX, irr::s32 tileY)
{
	return (tileY*TileCountX)+tileX;
}

irr::s32 CFlaceTerrainSceneNode::getTerrainCellIndex(irr::s32 cellX, irr::s32 cellY)
{
	return (cellY*CellCountX)+cellX;
}

irr::scene::SMesh* CFlaceTerrainSceneNode::getTerrainTileMesh(irr::s32 tileX, irr::s32 tileY)
{
	irr::s32 idx = getTerrainMeshIndex(tileX, tileY);
	if (idx < 0 || idx >= (irr::s32)TerrainTiles.size())
		return 0;

	CFlaceMeshSceneNode* node = TerrainTiles[idx];
	if (!node)
		return 0;

	return node->getOwnedMesh();
}

CFlaceMeshSceneNode* CFlaceTerrainSceneNode::getTerrainTileMeshSceneNodeFromGlobalPixelPosClamped(irr::f32 pixelX, irr::f32 pixelZ)
{
	int tilex = (irr::core::clamp((int)((pixelX - Displacement.X) / (irr::f32)TileSize), 0, TileCountX-1));
	int tiley = (irr::core::clamp((int)((pixelZ - Displacement.Z) / (irr::f32)TileSize), 0, TileCountY-1));

	return getTerrainTileMeshSceneNode(tilex, tiley);
}


CFlaceMeshSceneNode* CFlaceTerrainSceneNode::getTerrainTileMeshSceneNode(irr::s32 tileX, irr::s32 tileY)
{
	irr::s32 idx = getTerrainMeshIndex(tileX, tileY);
	if (idx < 0 || idx >= (irr::s32)TerrainTiles.size())
		return 0;

	return TerrainTiles[idx];
}


CFlaceTerrainSceneNode::STerrainData* CFlaceTerrainSceneNode::getTerrainData(irr::s32 globalCellX, irr::s32 globalCellY)
{
	if (globalCellX < 0 || globalCellY < 0 || globalCellX > CellCountX-1 || globalCellY > CellCountX-1)
		return 0;

	irr::s32 idx = getTerrainCellIndex(globalCellX, globalCellY);
	if (idx < 0 || idx >= (int)TerrainData.size())
		return 0;

	return &TerrainData[idx];
}


CFlaceTerrainSceneNode::STerrainData* CFlaceTerrainSceneNode::getTerrainDataClamped(irr::s32 globalCellX, irr::s32 globalCellY)
{
	return getTerrainData(irr::core::clamp(globalCellX, 0, CellCountX-1), 
						  irr::core::clamp(globalCellY, 0, CellCountY-1));
}


irr::f32 CFlaceTerrainSceneNode::getTerrainDataHeightClamped(irr::s32 globalCellX, irr::s32 globalCellY)
{	
	STerrainData* t = getTerrainDataClamped(globalCellX, globalCellY);
	if (t)
		return t->Height;

	return 0.0f;
}

//! returns the exact height of a position in the terrain, even if the position is not exactly at the vertex of a polygon. 
//! It is very fast for querying the height, as opposed to using Irrlicht's collision methods (which would have to iterate all triangles).
//! Also, if the position given is outside of the terrain, a height at the nearest border is returned.
irr::f32 CFlaceTerrainSceneNode::getExactTerrainHeightClampedAtPosition(irr::f32 globalPixelX, irr::f32 globalPixelY, irr::core::vector3df* outNormal)
{
	if (!CellSize)
		return 0.0f;

	int vertexCellx = (int)(globalPixelX / CellSize);
	int vertexCelly = (int)(globalPixelY / CellSize);	

	irr::core::vector3df posx1 = getTerrain3DPositionClamped(vertexCellx, vertexCelly) - Displacement;
	irr::core::vector3df posx2 = getTerrain3DPositionClamped(vertexCellx+1, vertexCelly) - Displacement;
	irr::core::vector3df posy2 = getTerrain3DPositionClamped(vertexCellx, vertexCelly+1) - Displacement;

	irr::core::vector3df linepoint(globalPixelX, 0, globalPixelY);
	irr::core::vector3df collision;

	irr::core::triangle3df tri(posx1, posx2, posy2);
	if (tri.getIntersectionOfPlaneWithLine(linepoint, irr::core::vector3df(0,1,0), collision))
	{
		irr::core::vector3df c = tri.closestPointOnTriangle(collision);

		if (!irr::core::equals(c.X, globalPixelX) || !irr::core::equals(c.Z, globalPixelY))
		{
			// the resulting collision point is not on the triangle anymore, so use the other triangle for collision

			irr::core::vector3df posy3 = getTerrain3DPositionClamped(vertexCellx+1, vertexCelly+1) - Displacement;
			tri.set(posx1, posx2, posy3);

			if (tri.getIntersectionOfPlaneWithLine(linepoint, irr::core::vector3df(0,1,0), collision))
			{
				if (outNormal)
					*outNormal = tri.getPlane().Normal;

				irr::core::vector3df c2 = tri.closestPointOnTriangle(collision);

				return c2.Y;
			}
		}

		if (outNormal)
			*outNormal = tri.getPlane().Normal;

		return c.Y;
	}

	return 0.0f;
}


irr::core::vector3df CFlaceTerrainSceneNode::getTerrain3DPositionClamped(irr::s32 globalCellX, irr::s32 globalCellY)
{
	irr::core::vector3df v(0,0,0);

	irr::s32 cx = irr::core::clamp(globalCellX, 0, CellCountX-1);
	irr::s32 cy = irr::core::clamp(globalCellY, 0, CellCountY-1);

	v.Y = TerrainData[getTerrainCellIndex(cx, cy)].Height;

	v.X = (irr::f32)(cx) * CellSize;
	v.Z = (irr::f32)(cy) * CellSize;		

	v += Displacement;

	return v;
}


void CFlaceTerrainSceneNode::createTerrainSceneNodes()
{
	irr::s32 nTileCount = TileCountX * TileCountY;

	if (TerrainTiles.size() != nTileCount)
	{
		clearCurrentTerrainMeshes();

		for (int i=0; i<nTileCount; ++i)
		{
			CFlaceMeshSceneNode* meshNode = new CFlaceMeshSceneNode(0, 0, this, SceneManager, -1);
			meshNode->setAutomaticCulling(irr::scene::EAC_FRUSTUM_BOX);
			meshNode->setUseOwnedMesh(true);
			meshNode->setReceivesStaticShadows(false);
			setUniqueIdForSceneNode(meshNode, SceneManager);
			TerrainTiles.push_back(meshNode);
		}
	}
}

void CFlaceTerrainSceneNode::clearCachedCollisionTrianglesFromTerrainMeshes()
{
	for (int i=0; i<(int)TerrainTiles.size(); ++i)
	{
		if (TerrainTiles[i])
			TerrainTiles[i]->setTriangleSelector(0);
	}
}

void CFlaceTerrainSceneNode::createCollisionTrianglesForTerrainMeshes()
{
	for (int i=0; i<(int)TerrainTiles.size(); ++i)
	{
		CFlaceMeshSceneNode* mesh = TerrainTiles[i];
		if (mesh && !mesh->getTriangleSelector())
		{
			irr::scene::ITriangleSelector* selector = SceneManager->createTriangleSelector(mesh->getMesh(), mesh, true);
			if (selector)
			{
				mesh->setTriangleSelector(selector);
				selector->drop();
			}
		}
	}
}

irr::scene::SMeshBuffer* CFlaceTerrainSceneNode::getOrCreateMeshBuffer(irr::scene::SMesh* mesh, 
																	   irr::s32 mainTextureIndex, 
																	   irr::s32 blendingToTextureIndex,
																	   irr::s32 nWithFreeVertices, 
																	   irr::s32 nWithFreeIndices,
																	   bool forGrass)
{
	if (!mesh)
		return 0;

	irr::video::ITexture* tex1 = 0;
	irr::video::ITexture* tex2 = 0;

	if (mainTextureIndex >= 0 && mainTextureIndex < (int)Textures.size())
		tex1 = Textures[mainTextureIndex];
	if (blendingToTextureIndex >= 0 && blendingToTextureIndex < (int)Textures.size())
		tex2 = Textures[blendingToTextureIndex];

	if (tex1 == tex2)
		tex2 = 0;

	irr::video::E_MATERIAL_TYPE grassMat =  GrassUsesWind ? 
												irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF_MOVING_GRASS : 
												irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;

	for (int i=0; i<(int)mesh->MeshBuffers.size(); ++i)
	{
		if (mesh->MeshBuffers[i]->getVertexType() != irr::video::EVT_STANDARD)
			continue; // probably created by the light mapper, we ignore those now

		irr::scene::SMeshBuffer* buf = (irr::scene::SMeshBuffer*)mesh->MeshBuffers[i];

		if (buf->Material.getTexture(0) == tex1 &&
			buf->Material.getTexture(1) == tex2 &&
			( (  forGrass && buf->Material.MaterialType == grassMat ) ||
			  ( !forGrass && buf->Material.MaterialType != grassMat ) )
		   )
		{
			if (buf->getVertexCount() + nWithFreeVertices < 65536 &&
				buf->getIndexCount()  + nWithFreeIndices  < 65536 )
			{
				return buf;
			}
		}
	}

	// not found, create new one

	irr::scene::SMeshBuffer* buffer = new irr::scene::SMeshBuffer();

	if (forGrass)
	{
		buffer->Material.MaterialType = GrassUsesWind ?
			irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF_MOVING_GRASS :
			irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		buffer->Material.MaterialTypeParam = 0.5f;
		buffer->Material.BackfaceCulling = false; // grass is double sided
	}
	else
		if (tex2 == 0)
			buffer->Material.MaterialType = irr::video::EMT_SOLID;
		else
			buffer->Material.MaterialType = irr::video::EMT_SOLID_VERTEX_ALPHA_TWO_TEXTURE_BLEND;

	buffer->Material.setTexture(0, tex1);
	buffer->Material.setTexture(1, tex2);
	
	if (LightingType == ETLT_DYNAMIC)
		buffer->Material.Lighting = true;
	else
		buffer->Material.Lighting = false;
	
	mesh->addMeshBuffer(buffer);

	buffer->drop();

	return buffer;
}

void CFlaceTerrainSceneNode::updateMeshesFromTerrainData()
{
	updateMeshesFromTerrainData(0, 0, CellCountX, CellCountY);	
}


void CFlaceTerrainSceneNode::updateMeshesFromTerrainData(int startCellX, int startCellY, int endCellX, int endCellY)
{
	createTerrainSceneNodes();

	irr::core::rect<irr::s32> rectAffected(startCellX, startCellY, endCellX, endCellY);

	irr::video::SColor clr = video::DefaultWhiteColor;

	// update tiles

	for (int tileX=0; tileX<TileCountX; ++tileX)
	{
		for (int tileY=0; tileY<TileCountY; ++tileY)
		{
			// check if this tile is actually affected. If not, we can skip it

			irr::core::rect<irr::s32> rectContainedByTile(tileX * CellsPerTileSide, tileY * CellsPerTileSide, (tileX+1) * CellsPerTileSide, (tileY+1) * CellsPerTileSide);
			if (!rectContainedByTile.isRectCollided(rectAffected))
				continue;

			// clear cached collision geometry

			CFlaceMeshSceneNode* node = getTerrainTileMeshSceneNode(tileX, tileY);
			if (node)
				node->setTriangleSelector(0); 

			// create new geometry

			irr::scene::SMesh* mesh = getTerrainTileMesh(tileX, tileY);
			if (mesh)
			{
				// clear mesh buffers

				for (u32 im=0; im<mesh->MeshBuffers.size(); ++im)
					if (mesh->MeshBuffers[im])
						mesh->MeshBuffers[im]->drop();
				mesh->MeshBuffers.clear();

				// now go through all cells of this tile and create vertices for them

				for (int x=0; x<CellsPerTileSide; ++x)
				{
					for (int y=0; y<CellsPerTileSide; ++y)
					{
						irr::s32 globalCellX = (tileX*CellsPerTileSide) + x;
						irr::s32 globalCellY = (tileY*CellsPerTileSide) + y;

						STerrainData* terrainData = getTerrainData(globalCellX, globalCellY);

						irr::scene::SMeshBuffer* buf = getOrCreateMeshBuffer(mesh, terrainData->MainTextureIndex, terrainData->BlendingToTextureIndex, 4, 6, false);
						if (buf)
						{
							irr::video::S3DVertex vtx;

							// the height for each cell is not in the center of the tile. Otherwise we would get steps in the terrain.
							// so the height is in the upper left corner = the first vertex. The height of the other vertices needs to be 
							// taken from the neighbouring terrain datas.
							//
							// vertices are this:
							//
							// 0 ------ 1
							// | \      |
							// |   \    |
							// |     \  |
							// 2 ------ 3

							irr::u16 nIndexStart = buf->Vertices.size();

							irr::s32 displaceX[4] = {0,1,0,1};
							irr::s32 displaceY[4] = {0,0,1,1};

							for (int vertex=0; vertex<4; ++vertex)
							{
								int vertexCellx = globalCellX+displaceX[vertex];
								int vertexCelly = globalCellY+displaceY[vertex];

								vtx.Pos = getTerrain3DPositionClamped(vertexCellx, vertexCelly);

								vtx.TCoords.X = vtx.Pos.X * TextureScale;
								vtx.TCoords.Y = vtx.Pos.Z * TextureScale;

								vtx.Color.set(terrainData->BlendFactorPerVertex[vertex], clr.getRed(), clr.getGreen(), clr.getBlue());

								// calculate normal

								irr::core::vector3df posTop = getTerrain3DPositionClamped(vertexCellx, vertexCelly-1);
								irr::core::vector3df posBottom = getTerrain3DPositionClamped(vertexCellx, vertexCelly+1);
								irr::core::vector3df posLeft = getTerrain3DPositionClamped(vertexCellx-1, vertexCelly);
								irr::core::vector3df posRight = getTerrain3DPositionClamped(vertexCellx+1, vertexCelly);

								core::vector3df normal;
								normal += core::plane3d<f32>(posTop, vtx.Pos, posLeft).Normal;
								normal += core::plane3d<f32>(posTop, posRight, vtx.Pos).Normal;
								normal += core::plane3d<f32>(vtx.Pos, posRight, posBottom).Normal;
								normal += core::plane3d<f32>(vtx.Pos, posBottom, posLeft).Normal;

								normal.normalize();
								vtx.Normal = normal * -1.0f;

								// add vertex

								buf->Vertices.push_back(vtx);
							}
							
							buf->Indices.push_back(nIndexStart);
							buf->Indices.push_back(nIndexStart + 3);
							buf->Indices.push_back(nIndexStart + 1);

							buf->Indices.push_back(nIndexStart);
							buf->Indices.push_back(nIndexStart + 2);
							buf->Indices.push_back(nIndexStart + 3);
						}

					} // end for y cells
				}	// end for x cells


				// TODO: recalculating bounding box can be done in O(1) by using the cell sizes

				for (u32 i=0; i<mesh->MeshBuffers.size(); ++i)
					mesh->MeshBuffers[i]->recalculateBoundingBox();

				mesh->recalculateBoundingBox();
				
			} // end if mesh

		} // end for y tiles

	} // end for x tiles


	// update grass patches

	irr::video::SColor clrGrass = video::DefaultWhiteColor;

	for (int i=0; i<(int)GrassInstances.size(); ++i)
	{
		SGrassInstance& g = GrassInstances[i];

		int tileX = (int)(g.PosX / TileSize);
		int tileY = (int)(g.PosZ / TileSize);

		// check if this tile is actually affected. If not, we can skip it

		irr::core::rect<irr::s32> rectContainedByTile(tileX * CellsPerTileSide, tileY * CellsPerTileSide, (tileX+1) * CellsPerTileSide, (tileY+1) * CellsPerTileSide);
		if (!rectContainedByTile.isRectCollided(rectAffected))
			continue;

		irr::scene::SMesh* mesh = getTerrainTileMesh(tileX, tileY);
		if (mesh)
		{
			int vertexCellx = (int)(g.PosX / CellSize);
			int vertexCelly = (int)(g.PosZ / CellSize);

			irr::core::vector3df normal;
			irr::f32 height = getExactTerrainHeightClampedAtPosition(g.PosX, g.PosZ, &normal);

			if (normal.getLength() > 0)
			{
				normal.normalize();
				normal *= -1.0f;
			}
			else
				normal.set(0,1,0);

			irr::scene::SMeshBuffer* buf = getOrCreateMeshBuffer(mesh, g.TextureIndex, g.TextureIndex, 16, 24, true);
			if (buf)
			{		
				irr::core::vector3df pos(g.PosX + Displacement.X, height, g.PosZ + Displacement.Z);

				for (int axis=0; axis<2; ++axis)
				{
					for (int backface=0; backface<1; ++backface)
					{
						irr::u16 nIndexStart = buf->Vertices.size();

						irr::f32 r = g.Rotation;
						if (axis == 1) r += 1.34f;

						irr::video::S3DVertex vtx1;
						vtx1.Color = clrGrass;
						vtx1.TCoords.X = 0.0f;
						vtx1.TCoords.Y = 1.0f;
						vtx1.Pos.X = pos.X + sin(r) * g.Width * 0.5f;
						vtx1.Pos.Y = height;
						vtx1.Pos.Z = pos.Z + cos(r) * g.Width * 0.5f;
						
						irr::video::S3DVertex vtx2;
						vtx2.Color = clrGrass;
						vtx2.TCoords.X = 1.0f;
						vtx2.TCoords.Y = 1.0f;
						vtx2.Pos.X = pos.X - sin(r) * g.Width * 0.5f;
						vtx2.Pos.Y = height;
						vtx2.Pos.Z = pos.Z - cos(r) * g.Width * 0.5f;
						
						irr::video::S3DVertex vtx3;
						vtx3.Color = clrGrass;
						vtx3.TCoords.X = 0.0f;
						vtx3.TCoords.Y = 0.0f;
						vtx3.Pos.X = pos.X + sin(r) * g.Width * 0.5f;
						vtx3.Pos.Y = height + g.Height;
						vtx3.Pos.Z = pos.Z + cos(r) * g.Width * 0.5f;
						
						irr::video::S3DVertex vtx4;
						vtx4.Color = clrGrass;
						vtx4.TCoords.X = 1.0f;
						vtx4.TCoords.Y = 0.0f;
						vtx4.Pos.X = pos.X - sin(r) * g.Width * 0.5f;
						vtx4.Pos.Y = height + g.Height;
						vtx4.Pos.Z = pos.Z - cos(r) * g.Width * 0.5f;				

						vtx1.Normal = normal;
						vtx2.Normal = normal;
						vtx3.Normal = normal;
						vtx4.Normal = normal;

						buf->Vertices.push_back(vtx1);
						buf->Vertices.push_back(vtx2);
						buf->Vertices.push_back(vtx3);
						buf->Vertices.push_back(vtx4);
						
						const int indicesFront[] = {2,1,0, 2,3,1};
						const int indicesBack[]  = {2,0,1, 2,1,3};
						int* indices = (int*)indicesFront;
						if (backface > 0)
							indices = (int*)indicesBack;

						for (int ind=0; ind<6; ++ind)
							buf->Indices.push_back(nIndexStart + indices[ind]);
					}
				}
			}
		}
	}
}

void CFlaceTerrainSceneNode::calculateBlendingFactors()
{
	calculateBlendingFactors(0, 0, CellCountX, CellCountY);	
}

void CFlaceTerrainSceneNode::calculateBlendingFactors(int startCellX, int startCellY, int endCellX, int endCellY)
{
	if (startCellX < 0)
		startCellX = 0;

	if (startCellY < 0)
		startCellY = 0;

	if (endCellX > CellCountX)
		endCellX = CellCountX;

	if (endCellY > CellCountY)
		endCellY = CellCountY;

	// for all terrain data points,
	// calculate best target texture to blend to from the main texture, 
	// and also at what vertex and with what blending factor
	// 0 ------ 1
	// | \      |
	// |   \    |
	// |     \  |
	// 2 ------ 3

	// it works like this:
	// we always look at 4 tiles at the same time, and blend between what the user set there, to make it
	// look as close as possible to what the user wanted to have.

	// clear to defaults, if the terrain has no factor of 4 size

	for (int x=startCellX; x<endCellX; ++x)
		for (int y=startCellY; y<endCellY; ++y)
		{
			STerrainData* current = &TerrainData[getTerrainCellIndex(x,y)]; 
			current->MainTextureIndex = current->UserSetTextureIndex;
			current->BlendingToTextureIndex = current->UserSetTextureIndex;
			current->BlendFactorPerVertex[0] = 0;
			current->BlendFactorPerVertex[1] = 0;
			current->BlendFactorPerVertex[2] = 0;
			current->BlendFactorPerVertex[3] = 0;
		}

	for (int x=startCellX; x<endCellX; ++x)
	{
		for (int y=startCellY; y<endCellY; ++y)
		{
			STerrainData& current = TerrainData[getTerrainCellIndex(x,y)];

			// check if this is a border element
			
			int otherTexture = current.UserSetTextureIndex;
			bool isBorderElement = false;
			irr::core::position2di posFound(0,0);

			for (int nx=x-1; nx<x+2; ++nx)
				for (int ny=y-1; ny<y+2; ++ny)
				{
					STerrainData* pOther = getTerrainDataClamped(nx,ny);
					if (pOther->UserSetTextureIndex > current.UserSetTextureIndex)
					{
						otherTexture = pOther->UserSetTextureIndex;
						isBorderElement = true;
						break;
					}
				}

			// based on where the other texture is, set blending factors
			// vertices: 
			// 0 ------ 1
			// | \      |
			// |   \    |
			// |     \  |
			// 2 ------ 3

			// indices to set to blending if a border element is set,
			// based on location of other target. If any entry is 1,
			// blending needs to be set for this vertex.

				
			irr::s32 indicesToSetToBlending[9][4] = 
				{{1,0,0,0}, {1,1,0,0}, {0,1,0,0},
				 {1,0,1,0}, {0,0,0,0}, {0,1,0,1},
				 {0,0,1,0}, {0,0,1,1}, {0,0,0,1}};

			if (isBorderElement)
			{
				irr::s32 idx = -1;
				for (int ny=y-1; ny<y+2; ++ny)
					for (int nx=x-1; nx<x+2; ++nx)					
					{
						++idx;

						STerrainData* pOther = getTerrainDataClamped(nx,ny);

						if (pOther->UserSetTextureIndex != current.UserSetTextureIndex)
						{
							irr::s32* indicesToSet = indicesToSetToBlending[idx];

							for (int vtxidx=0; vtxidx<4; ++vtxidx)
							{
								if (indicesToSet[vtxidx] != 0)
									current.BlendFactorPerVertex[vtxidx] = 255;
							}
						}
					}

				current.MainTextureIndex = current.UserSetTextureIndex;
				current.BlendingToTextureIndex = otherTexture;
			}
			else
			{
				// no blending necessary
				current.BlendFactorPerVertex[0] = 0; 
				current.BlendFactorPerVertex[1] = 0; 
				current.BlendFactorPerVertex[2] = 0; 
				current.BlendFactorPerVertex[3] = 0; 
				current.BlendingToTextureIndex = current.MainTextureIndex;
			}
		}
	}
}



void CFlaceTerrainSceneNode::calculateBlendingAll(int startCellX, int startCellY, int endCellX, int endCellY)
{
	
	for (int x=startCellX; x<endCellX; ++x)
	{
		for (int y=startCellY; y<endCellY; ++y)
		{
			STerrainData& current = TerrainData[getTerrainCellIndex(x,y)];

			int otherTexture = current.UserSetTextureIndex;
			irr::core::position2di posFound(0,0);

			for (int nx=x-1; nx<x+2; ++nx)
				for (int ny=y-1; ny<y+2; ++ny)
				{
					STerrainData* pOther = getTerrainDataClamped(nx,ny);
				}

			irr::s32 indicesToSetToBlending[9][4] = 
				{{1,0,0,0}, {1,1,0,0}, {0,1,0,0},
				 {1,0,1,0}, {0,0,0,0}, {0,1,0,1},
				 {0,0,1,0}, {0,0,1,1}, {0,0,0,1}};

				irr::s32 idx = -1;
				for (int ny=y-1; ny<y+2; ++ny)
					for (int nx=x-1; nx<x+2; ++nx)					
					{
						++idx;

						STerrainData* pOther = getTerrainDataClamped(nx,ny);

						if (pOther->UserSetTextureIndex != current.UserSetTextureIndex)
						{
							irr::s32* indicesToSet = indicesToSetToBlending[idx];

							for (int vtxidx=0; vtxidx<4; ++vtxidx)
							{
								if (indicesToSet[vtxidx] != 0)
									current.BlendFactorPerVertex[vtxidx] = texBlend;
							}
						}
					}
				current.MainTextureIndex = current.UserSetTextureIndex;
				current.BlendingToTextureIndex = otherTexture;
		}
	}
}




//! Returns the material based on the zero based index i.
irr::video::SMaterial& CFlaceTerrainSceneNode::getMaterial(u32 num)
{
	syncMaterials();

	if (num >= 0 && num < DummyMaterials.size())
		return DummyMaterials[num];

	return *((video::SMaterial*)0);
}

//! Get amount of materials used by this scene node.
irr::u32 CFlaceTerrainSceneNode::getMaterialCount() const
{
	return (irr::u32)Textures.size();
}

//! ensures data shown in dummy material array is the same as in the textures
void CFlaceTerrainSceneNode::syncMaterials()
{
	while(Textures.size() > DummyMaterials.size())
		DummyMaterials.push_back(irr::video::SMaterial());

	while(Textures.size() < DummyMaterials.size())
		DummyMaterials.erase(DummyMaterials.size() - 1);

	for (int i=0; i<(int)Textures.size(); ++i)
	{
		irr::video::SMaterial& rMatDummy = DummyMaterials[i];
		if (rMatDummy.getTexture(0) != Textures[i])
			rMatDummy.setTexture(0, Textures[i]);
	}
}

void CFlaceTerrainSceneNode::replaceTexture(int idx, irr::video::ITexture* newtexture, IUndoManager* undo)
{
	if (idx < 0 || idx >= (int)Textures.size())
		return;	

	// record material change

	irr::video::ITexture* texBefore = Textures[idx];
	
	if (newtexture == texBefore)
		return;		

	if (undo)
	{
		undo->addUndoPartChangeTerrainTexture(this, texBefore, newtexture, idx);
	}

	if (Textures[idx])
		Textures[idx]->drop();

	Textures[idx] = newtexture;

	if (Textures[idx])
		Textures[idx]->grab();

	// set textures

	syncMaterials();

	// also change for all terrain meshes

	updateMeshesFromTerrainData();
}


CFlaceTerrainSceneNode::E_TERRAIN_LIGHTING_TYPE CFlaceTerrainSceneNode::getLightingType()
{
	return ETLT_DYNAMIC;
}


void CFlaceTerrainSceneNode::setLightingType(E_TERRAIN_LIGHTING_TYPE nType, IUndoManager* undo)
{
	if (nType != LightingType)
	{
		if (undo)
			undo->addUndoPartChangeTerrainLighting(this, LightingType, nType);

		LightingType = nType;

		updateMeshesFromTerrainData();
	}
}

void CFlaceTerrainSceneNode::distributeMeshes(irr::scene::IAnimatedMesh* tree, irr::f32 distribution, 
											  IUndoManager* undo, const irr::c8* basename)
{
	if (!tree || distribution < 0.001f)
		return;

	irr::scene::IMesh* mesh = tree->getMesh(0);
	if (!mesh)
		return;

	irr::core::aabbox3df box = mesh->getBoundingBox();

	float treeWidth = box.getExtent().X;
	float treeDepth = box.getExtent().Z;
	float treeHeight = box.getExtent().Y;

	if (treeWidth < 1) treeWidth = 1;
	if (treeDepth < 1) treeDepth = 1;

	int nTreesWanted = (int)(((SideLength * SideLength) / (treeWidth * treeDepth)) * distribution);
	if (nTreesWanted > 500)
		nTreesWanted = 500;

	for (int nTree=0; nTree<nTreesWanted; ++nTree)
	{
		irr::core::vector3df pos;
		irr::core::vector3df rot;

		rot.Y = irr::os::Randomizer::randFloat() * 360.0f;

		const int fact = 100;
		pos.X = ((irr::os::Randomizer::rand()) % ((SideLength - CellSize) * fact)) / (irr::f32)fact;
		pos.Z = ((irr::os::Randomizer::rand()) % ((SideLength - CellSize) * fact)) / (irr::f32)fact;

		pos.Y = getExactTerrainHeightClampedAtPosition(pos.X, pos.Z);

		pos += Displacement;
		pos.Y -= treeHeight * 0.025f; // so that the tree doesn't float in hills

		CFlaceMeshSceneNode* tileNode = getTerrainTileMeshSceneNodeFromGlobalPixelPosClamped(pos.X, pos.Z);
		if (!tileNode)
		{
			// something went wrong
			continue;
		}

		CFlaceAnimatedMeshSceneNode* node = new CFlaceAnimatedMeshSceneNode(undo, tree, tileNode, 
				getSceneManager(), -1, pos, rot);		

		setUniqueIdForSceneNode(node, getSceneManager());
		setUniqueNameForSceneNode(node, basename, getSceneManager());

		if (getLightingType() == ETLT_DYNAMIC)
		{
			for (int i=0; i<(int)node->getMaterialCount(); ++i)
				node->getMaterial(i).Lighting = true;
		}

		node->drop();
	}
}


bool CFlaceTerrainSceneNode::getSelectedTerrainTileFromScreenCoords(int x, int y, irr::core::vector2di& rOut)
{
	irr::core::line3df line = 
		SceneManager->getSceneCollisionManager()->getRayFromScreenCoordinates(irr::core::position2di(x,y), SceneManager->getActiveCamera());

	if (line.start.equals(irr::core::vector3df(0,0,0)))
		return false;

	createCollisionTrianglesForTerrainMeshes();	

	irr::core::vector3df closestCollisionPoint;
	irr::f32 nearestDistance = 9999999999.9f;
	bool collisionFound = false;

	for (int i=0; i<(int)TerrainTiles.size(); ++i)
	{
		CFlaceMeshSceneNode* mesh = TerrainTiles[i];
		if (mesh && mesh->getTriangleSelector() &&
			mesh->getBoundingBox().intersectsWithLine(line)) // <- we can safely check if the line collides with 
															 // the non transformed bounding box, because terrain
															 // always is at position (0,0,0) with no transformation
															 // applied
		{
			irr::core::vector3df collisionPoint;
			irr::core::triangle3df tri;

			if (SceneManager->getSceneCollisionManager()->getCollisionPoint(line, 
						mesh->getTriangleSelector(), collisionPoint, tri, true))
			{
				irr::f32 distance = collisionPoint.getDistanceFrom(line.start);

				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					closestCollisionPoint = collisionPoint;
					collisionFound = true;
				}				
			}
		}
	}

	if (!collisionFound)
		return false;

	// now get all tiles starting with this collision point

	int tileX = (int)((closestCollisionPoint.X - Displacement.X) / CellSize);
	int tileY = (int)((closestCollisionPoint.Z - Displacement.Z) / CellSize);

	rOut.set(tileX, tileY);
	return true;
}


void CFlaceTerrainSceneNode::drawEditBrushSelection(irr::core::vector2di tile,
													irr::video::SColor clr, irr::f32 brushSize)
{
	video::SMaterial m;
	m.Lighting = false;
	m.ZBuffer = false;
	Driver->setMaterial(m);

	irr::core::matrix4 mat;
	Driver->setTransform(video::ETS_WORLD, mat);


	for (int x=0; x<(int)brushSize; ++x)
	{
		for (int y=0; y<(int)brushSize; ++y)
		{
			int cTileX = tile.X + x - (int)(brushSize/2);
			int cTileY = tile.Y + y - (int)(brushSize/2);

			irr::core::vector2di displacements[4] = { 
				irr::core::vector2di(0,0), irr::core::vector2di(1,0), irr::core::vector2di(0,1), irr::core::vector2di(1,1) };
			irr::core::vector3df vects[4];

			for (int i=0; i<4; ++i)
			{
				STerrainData* pData = getTerrainDataClamped(cTileX + displacements[i].X, cTileY + displacements[i].Y);

				vects[i].X = ((cTileX + displacements[i].X) * CellSize) + Displacement.X;
				vects[i].Y = pData->Height + Displacement.Y;
				vects[i].Z = ((cTileY + displacements[i].Y) * CellSize) + Displacement.Z;
			}	

			Driver->draw3DLine(vects[0], vects[1], clr);
			Driver->draw3DLine(vects[1], vects[3], clr);

			if (y == (int)brushSize-1)
				Driver->draw3DLine(vects[2], vects[3], clr);

			if (x == 0)
				Driver->draw3DLine(vects[0], vects[2], clr);
		}
	}	
}


void CFlaceTerrainSceneNode::getMinMaxHeightOfTerrainDataInBrush(irr::core::vector2di tile, irr::s32 brushSize, 
											  irr::f32& rOutMinValue, irr::f32& rOutMaxValue)
{
	bool bFirstValue = true;

	for (int x=0; x<(int)brushSize; ++x)
	{
		for (int y=0; y<(int)brushSize; ++y)
		{
			int cTileX = tile.X + x - (int)(brushSize/2);
			int cTileY = tile.Y + y - (int)(brushSize/2);

			STerrainData* data = getTerrainData(cTileX, cTileY);
			if (data)
			{
				if (bFirstValue)
				{
					rOutMinValue = data->Height;
					rOutMaxValue = data->Height;
					bFirstValue = false;
				}
				else
				{
					rOutMinValue = irr::core::min_(rOutMinValue, data->Height);
					rOutMaxValue = irr::core::max_(rOutMaxValue, data->Height);
				}
			}
		}
	}
}

void CFlaceTerrainSceneNode::drawEditBrushSelectionMountainValleyTool(irr::core::vector2di tile, irr::video::SColor clr, irr::f32 brushSize, irr::f32 additionalHeight)
{
	drawEditBrushSelectionRaiseTool(tile, clr, brushSize, additionalHeight, CellSize * brushSize * 0.03f * additionalHeight);
}

void CFlaceTerrainSceneNode::drawEditBrushSelectionRaiseTool(irr::core::vector2di tile,
													irr::video::SColor clr, irr::f32 brushSize, 
													irr::f32 additionalHeight,
													irr::f32 sphereFactor)
{
	// go transforming

	video::SMaterial m;
	m.Lighting = false;
	m.ZBuffer = false;
	Driver->setMaterial(m);

	irr::core::matrix4 mat;
	Driver->setTransform(video::ETS_WORLD, mat);

	irr::f32 minValue = 0.0f;
	irr::f32 maxValue = 0.0f;
	getMinMaxHeightOfTerrainDataInBrush(tile, (irr::s32)brushSize, minValue, maxValue);
	bool enableSmoothRaising = !irr::core::equals(minValue, maxValue);

	for (int x=0; x<(int)brushSize; ++x)
	{
		for (int y=0; y<(int)brushSize; ++y)
		{
			int cTileX = tile.X + x - (int)(brushSize/2);
			int cTileY = tile.Y + y - (int)(brushSize/2);

			irr::core::vector2di displacements[4] = { 
				irr::core::vector2di(0,0), irr::core::vector2di(1,0), irr::core::vector2di(0,1), irr::core::vector2di(1,1) };
			irr::core::vector3df vects[4];

			for (int i=0; i<4; ++i)
			{
				STerrainData* pData = getTerrainDataClamped(cTileX + displacements[i].X, cTileY + displacements[i].Y);

				vects[i].X = ((cTileX + displacements[i].X) * CellSize) + Displacement.X;
				vects[i].Z = ((cTileY + displacements[i].Y) * CellSize) + Displacement.Z;

				irr::f32 h = pData->Height;

				if (!irr::core::iszero(sphereFactor))
				{
					float mountain = (sin((x+displacements[i].X) / (float)(brushSize * 1.00f) * irr::core::PI) 
						* sin((y+displacements[i].Y) / (float)(brushSize * 1.00f) * irr::core::PI))
						* sphereFactor * 0.01f;

					if (additionalHeight > 0.0f)
						h += additionalHeight * mountain;
					else
						h -= additionalHeight * mountain;
				}
				else
				if (enableSmoothRaising)
				{
					// scale height addition by distance from max value
					irr::f32 fact = (maxValue - pData->Height) / (maxValue - minValue);
					if (fact > 1.0f) fact = 1.0f;

					if (additionalHeight < 0.0f)
						fact = 1.0f - fact;

					h += additionalHeight * fact;
				}
				else
					h += additionalHeight;	
				

				vects[i].Y = h + Displacement.Y;
			}	

			Driver->draw3DLine(vects[0], vects[1], clr);
			Driver->draw3DLine(vects[1], vects[3], clr);

			if (y == (int)brushSize-1)
				Driver->draw3DLine(vects[2], vects[3], clr);

			if (x == 0)
				Driver->draw3DLine(vects[0], vects[2], clr);
		}
	}
}


irr::s32 CFlaceTerrainSceneNode::findTextureIndexOrAddNewOne(irr::video::ITexture* tex)
{
	if (!tex)
		return -1;

	irr::s32 nTexIndex = -1;
	for (int t=0; t<(int)Textures.size(); ++t)
		if (Textures[t] == tex)
		{
			nTexIndex = t;
			break;
		}

	if (nTexIndex == -1)
	{
		nTexIndex = (irr::s32)Textures.size();

		Textures.push_back(tex);
		tex->grab();				
	}

	return nTexIndex;
}


void CFlaceTerrainSceneNode::paintTexture(int screenCoord2DX, int screenCoord2DY, irr::f32 brushSize, irr::video::ITexture* texture, IUndoManager* undo)
{
	irr::core::vector2di tile;

	if (!texture || brushSize < 1 || !getSelectedTerrainTileFromScreenCoords(screenCoord2DX, screenCoord2DY, tile))
		return;

	bool changeDone = false;
	irr::f32* pSnaphshotOld = 0;

	irr::s32 texIndex = findTextureIndexOrAddNewOne(texture);

	// paint

	for (int x=0; x<(int)brushSize; ++x)
	{
		for (int y=0; y<(int)brushSize; ++y)
		{
			int cTileX = tile.X + x - (int)(brushSize/2);
			int cTileY = tile.Y + y - (int)(brushSize/2);

			STerrainData* data = getTerrainData(cTileX, cTileY);
			if (data && data->UserSetTextureIndex != texIndex)
			{
				data->UserSetTextureIndex = texIndex;
				changeDone = true;

				if (undo && !pSnaphshotOld)
					pSnaphshotOld = createTerrainDataSnapshot();
			}
		}
	}

	// rebuild terrain

	if (changeDone)
	{
		if (undo)
		{
			irr::f32* pSnapsotNew = createTerrainDataSnapshot();
			undo->addUndoPartChangeTerrainData(this, pSnaphshotOld, pSnapsotNew);
		}
		
		const int border = 2;
		int brushSizeHalf = (int)(brushSize/2.0f);
		calculateBlendingFactors(tile.X - border - brushSizeHalf, tile.Y - border - brushSizeHalf, 
								    tile.X + brushSizeHalf + border + 1, tile.Y + brushSizeHalf + border + 1);
		updateMeshesFromTerrainData(tile.X - border - brushSizeHalf, tile.Y - border - brushSizeHalf, 
								    tile.X + brushSizeHalf + border + 1, tile.Y + brushSizeHalf + border + 1);
	}
}


void CFlaceTerrainSceneNode::resetTerrainDataFromSnapshot(irr::f32* pTerrainData)
{
	if (!TerrainData.size())
		return;

	for (int i=0; i<(int)TerrainData.size(); ++i)
	{
		TerrainData[i].Height = pTerrainData[i*2 +0];
		TerrainData[i].UserSetTextureIndex = (int)pTerrainData[i*2 +1];
	}

	calculateBlendingFactors();
	updateMeshesFromTerrainData();
}


irr::f32* CFlaceTerrainSceneNode::createTerrainDataSnapshot()
{
	if (!TerrainData.size())
		return 0;

	irr::f32* data = new irr::f32[TerrainData.size() * 2];

	for (int i=0; i<(int)TerrainData.size(); ++i)
	{
		data[i*2 +0] = TerrainData[i].Height;
		data[i*2 +1] = (irr::f32)TerrainData[i].UserSetTextureIndex;
		MaxHeight = (int)irr::core::max_((irr::f32)MaxHeight, TerrainData[i].Height); // Robbo
	}
	return data;
}


void CFlaceTerrainSceneNode::resetTerrainGrassDataFromSnapshot(irr::f32* pTerrainData)
{
	GrassInstances.clear();

	int sz = *(irr::s32*)((void*)&pTerrainData[0]);
	const int headerSize = 1;

	for (int i=0; i<sz; ++i)
	{
		SGrassInstance instance;
		instance.Height =				pTerrainData[i*6 +0 + headerSize];
		instance.Width =				pTerrainData[i*6 +1 + headerSize];
		instance.PosX =					pTerrainData[i*6 +2 + headerSize];
		instance.PosZ =					pTerrainData[i*6 +3 + headerSize];
		instance.Rotation =				pTerrainData[i*6 +4 + headerSize];
		instance.TextureIndex = (int)	pTerrainData[i*6 +5 + headerSize];

		GrassInstances.push_back(instance);
	}

	updateMeshesFromTerrainData();
}


irr::f32* CFlaceTerrainSceneNode::createTerrainGrassDataSnapshot()
{
	if (!TerrainData.size())
		return 0;

	irr::f32* data = new irr::f32[(GrassInstances.size() * 6) + 1];

	const int headerSize = 1;
	int sz = GrassInstances.size();
	data[0] = *(irr::f32*)((void*)&sz);

	for (int i=0; i<(int)GrassInstances.size(); ++i)
	{
		data[i*6 +0 + headerSize] = GrassInstances[i].Height;
		data[i*6 +1 + headerSize] = GrassInstances[i].Width;
		data[i*6 +2 + headerSize] = GrassInstances[i].PosX;
		data[i*6 +3 + headerSize] = GrassInstances[i].PosZ;
		data[i*6 +4 + headerSize] = GrassInstances[i].Rotation;
		data[i*6 +5 + headerSize] = (irr::f32)GrassInstances[i].TextureIndex;
	}

	return data;
}


void CFlaceTerrainSceneNode::mountainValleyTerrain(irr::core::vector2di tile, irr::f32 brushSize, irr::f32 additionalHeight, IUndoManager* undo)
{
	raiseLowerTerrain(tile, brushSize, additionalHeight, undo, CellSize * brushSize * 0.03f * additionalHeight);
}

void CFlaceTerrainSceneNode::raiseLowerTerrain(irr::core::vector2di tile, irr::f32 brushSize, irr::f32 additionalHeight, IUndoManager* undo, irr::f32 sphereFactor)
{
	if (irr::core::equals(additionalHeight, 0.0f) || brushSize < 1)
		return;

	bool changeDone = false;
	irr::f32* pSnaphshotOld = 0;

	// calculate min and max values for raising

	irr::f32 minValue = 0.0f;
	irr::f32 maxValue = 0.0f;
	getMinMaxHeightOfTerrainDataInBrush(tile, (irr::s32)brushSize, minValue, maxValue);
	bool enableSmoothRaising = !irr::core::equals(minValue, maxValue);


	// precalculations for embedded mesh movement

	irr::core::array<SOldMeshPositionsInTerrain> embeddedMeshOldPositions;
	getEmbeddedMeshPositionsInTerrain(embeddedMeshOldPositions, tile, (irr::s32)brushSize);

	// raise

	for (int x=0; x<(int)brushSize; ++x)
	{
		for (int y=0; y<(int)brushSize; ++y)
		{
			int cTileX = tile.X + x - (int)(brushSize/2);
			int cTileY = tile.Y + y - (int)(brushSize/2);

			STerrainData* pData = getTerrainData(cTileX, cTileY);
			if (pData)
			{
				if (undo && !pSnaphshotOld)
					pSnaphshotOld = createTerrainDataSnapshot();

				if (!irr::core::iszero(sphereFactor))
				{
					float mountain = (sin((x) / (float)(brushSize * 1.00f) * irr::core::PI) 
						* sin((y) / (float)(brushSize * 1.00f) * irr::core::PI))
						* sphereFactor * 0.01f;
					
					if (additionalHeight > 0.0f)
						pData->Height += additionalHeight * mountain;
					else
						pData->Height -= additionalHeight * mountain;
				}
				else
				if (enableSmoothRaising)
				{
					irr::f32 fact = (maxValue - pData->Height) / (maxValue - minValue);
					if (fact > 1.0f) fact = 1.0f;

					if (additionalHeight < 0.0f)
						fact = 1.0f - fact;

					pData->Height += additionalHeight * fact;
				}
				else
					pData->Height += additionalHeight;

				changeDone = true;
			}
		}
	}

	// rebuild terrain

	if (changeDone)
	{
		if (undo)
		{
			irr::f32* pSnapsotNew = createTerrainDataSnapshot();
			undo->addUndoPartChangeTerrainData(this, pSnaphshotOld, pSnapsotNew);
		}

		// move embedded meshes

		adjustEmbeddedMeshHeights(embeddedMeshOldPositions, undo);

		// update terrain

		const int border = 2;
		int brushSizeHalf = (int)(brushSize/2.0f);
		updateMeshesFromTerrainData(tile.X - border - brushSizeHalf, tile.Y - border - brushSizeHalf, 
								    tile.X + brushSizeHalf + border + 1, tile.Y + brushSizeHalf + border + 1);
	}	
}


void CFlaceTerrainSceneNode::modifyTerrain(irr::core::vector2di tile, irr::f32 brushSize, bool smooth, bool noise, bool flatten, IUndoManager* undo)
{
	if (brushSize < 1)
		return;

	bool changeDone = false;
	irr::f32* pSnaphshotOld = 0;

	// calculate min and max values for smoothing

	irr::f32 minValue = 0.0f;
	irr::f32 maxValue = 0.0f;
	getMinMaxHeightOfTerrainDataInBrush(tile, (irr::s32)brushSize, minValue, maxValue);
	bool enableSmoothing = !irr::core::equals(minValue, maxValue);

	if (!enableSmoothing)
		return;

	// precalculations for embedded mesh movement

	irr::core::array<SOldMeshPositionsInTerrain> embeddedMeshOldPositions;
	getEmbeddedMeshPositionsInTerrain(embeddedMeshOldPositions, tile, (irr::s32)brushSize);

	// smooth

	irr::f32 average = minValue + ((maxValue - minValue) * 0.5f);

	for (int x=0; x<(int)brushSize; ++x)
	{
		for (int y=0; y<(int)brushSize; ++y)
		{
			int cTileX = tile.X + x - (int)(brushSize/2);
			int cTileY = tile.Y + y - (int)(brushSize/2);

			STerrainData* pData = getTerrainData(cTileX, cTileY);
			if (pData)
			{
				if (undo && !pSnaphshotOld)
					pSnaphshotOld = createTerrainDataSnapshot();

				if (smooth)
				{
					irr::f32 distanceFromAverage = pData->Height - average;
					pData->Height -= distanceFromAverage * 0.5f;
				}
				else
				if (flatten)
				{
					pData->Height = minValue;
				}
				else
				if (noise)
				{
					pData->Height += (irr::os::Randomizer::randFloat() - 0.5f) * (CellSize);
				}			
				
				changeDone = true;
			}
		}
	}

	// rebuild terrain

	if (changeDone)
	{
		if (undo)
		{
			irr::f32* pSnapsotNew = createTerrainDataSnapshot();
			undo->addUndoPartChangeTerrainData(this, pSnaphshotOld, pSnapsotNew);
		}

		// move embedded meshes

		adjustEmbeddedMeshHeights(embeddedMeshOldPositions, undo);

		// update terrain

		const int border = 2;
		int brushSizeHalf = (int)(brushSize/2.0f);
		updateMeshesFromTerrainData(tile.X - border - brushSizeHalf, tile.Y - border - brushSizeHalf, 
								    tile.X + brushSizeHalf + border + 1, tile.Y + brushSizeHalf + border + 1);
	}	
}


void CFlaceTerrainSceneNode::paintGrass(irr::core::vector2di tile, irr::f32 brushSize,
										bool removeGrass, irr::video::ITexture* tex, 
										irr::f32 width, irr::f32 height, IUndoManager* undo)
{
	const int border = 0;
	int brushSizeHalf = (int)(brushSize/2.0f);
	irr::core::rect<irr::s32> rectAffected(tile.X - border - brushSizeHalf, 
											tile.Y - border - brushSizeHalf, 
										   tile.X + brushSizeHalf + border + 1, tile.Y + brushSizeHalf + border + 1);	

	bool changeDone = false;
	irr::f32* pSnaphshotOld = 0;

	int nTexIndex = -1;
	if (!removeGrass)
	{
		nTexIndex = findTextureIndexOrAddNewOne(tex);

		if (nTexIndex == -1)
			return;
	}

	if (removeGrass)
	{
		// remove grass

		for (int i=0; i<(int)GrassInstances.size(); )
		{
			int cellX = (int)(GrassInstances[i].PosX / CellSize);
			int cellY = (int)(GrassInstances[i].PosZ/ CellSize);

			if (rectAffected.isPointInside(irr::core::position2di(cellX, cellY)))
			{
				if (undo && !pSnaphshotOld)
					pSnaphshotOld = createTerrainGrassDataSnapshot();

				changeDone = true;

				GrassInstances.erase(i);
			}
			else
				++i;
		}
	}
	else
	{
		// add grass

		int grassPatchesToAdd = (int)brushSize;

		for (int g=0; g<grassPatchesToAdd; ++g)
		{
			if (undo && !pSnaphshotOld)
				pSnaphshotOld = createTerrainGrassDataSnapshot();

			changeDone = true;

			const int fact = 100;
			irr::f32 posx = (rectAffected.UpperLeftCorner.X * CellSize) + ((irr::os::Randomizer::randFloat() * CellSize) * ((rectAffected.getWidth()-1)));
			irr::f32 posy = (rectAffected.UpperLeftCorner.Y * CellSize) + ((irr::os::Randomizer::randFloat() * CellSize) * ((rectAffected.getHeight()-1)));
			
			int cellX = (int)(posx / CellSize);
			int cellY = (int)(posy / CellSize);

			// add grass patch

			SGrassInstance instance;
			instance.PosX = posx;
			instance.PosZ = posy;
			instance.Height = height;
			instance.Width = width;
			instance.Rotation = (irr::os::Randomizer::rand() % 1000) / 500.0f;
			instance.TextureIndex = nTexIndex;

			GrassInstances.push_back(instance);
		}
	}

	if (changeDone)
	{
		if (undo)
		{
			irr::f32* pSnapsotNew = createTerrainGrassDataSnapshot();
			undo->addUndoPartChangeTerrainGrassData(this, pSnaphshotOld, pSnapsotNew);
		}

		updateMeshesFromTerrainData(rectAffected.UpperLeftCorner.X, rectAffected.UpperLeftCorner.Y,
									rectAffected.LowerRightCorner.X, rectAffected.LowerRightCorner.Y);
	}	
}

void CFlaceTerrainSceneNode::getEmbeddedMeshPositionsInTerrain(irr::core::array<SOldMeshPositionsInTerrain>& outArr, irr::core::vector2di tile, irr::s32 brushSize)
{
	const int border = 1;
	int brushSizeHalf = (int)(brushSize/2.0f);
	irr::core::rect<irr::f32> rectAffected((irr::f32)(tile.X - border - brushSizeHalf) * CellSize, 
										   (irr::f32)(tile.Y - border - brushSizeHalf) * CellSize, 
										   (irr::f32)(tile.X + brushSizeHalf + border + 1) * CellSize,
										   (irr::f32)(tile.Y + brushSizeHalf + border + 1) * CellSize);

	for (int i=0; i<(int)TerrainTiles.size(); ++i)
	{
		if (TerrainTiles[i])
		{
			const core::list<ISceneNode*>& children = TerrainTiles[i]->getChildren();

			for (core::list<ISceneNode*>::ConstIterator it = children.begin();
				 it != children.end(); ++it)
			{
				irr::scene::ISceneNode* node = *it;

				irr::core::position2df posInTerrain(node->getPosition().X - Displacement.X, node->getPosition().Z - Displacement.Z);

				if (rectAffected.isPointInside(posInTerrain))
				{
					SOldMeshPositionsInTerrain pos;
					pos.node = node;
					pos.oldHeight = getExactTerrainHeightClampedAtPosition(node->getPosition().X - Displacement.X, node->getPosition().Z - Displacement.Z);

					outArr.push_back(pos);
				}				
			}
		}
	}
	
}

void CFlaceTerrainSceneNode::adjustEmbeddedMeshHeights(irr::core::array<SOldMeshPositionsInTerrain>& positions, IUndoManager* undo)
{
	for (int i=0; i<(int)positions.size(); ++i)
	{
		irr::scene::ISceneNode* node = positions[i].node;

		irr::f32 newHeight = getExactTerrainHeightClampedAtPosition(node->getPosition().X - Displacement.X, node->getPosition().Z - Displacement.Z);
		irr::f32 delta = positions[i].oldHeight - newHeight;

		if (!irr::core::iszero(delta))
		{			
			irr::core::vector3df posnew = node->getPosition();
			posnew.Y -= delta;

			node->setPositionWithUndo(posnew, undo);
		}		
	}
}

void CFlaceTerrainSceneNode::resortChildIntoCorrectTerrainTileMesh(irr::scene::ISceneNode* node, IUndoManager* undo)
{
	if (!node)
		return;

	irr::core::vector3df pos = node->getPosition();

	CFlaceMeshSceneNode* tileNode = getTerrainTileMeshSceneNodeFromGlobalPixelPosClamped(pos.X, pos.Z);
	
	if (tileNode && node->getParent() != tileNode)
	{
		node->grab();

		node->getParent()->removeChild(node, undo);
		tileNode->addChild(node, undo);

		node->drop();
	}
}

// called when the node and its children were read, so the node is able to link with its children if necessary
void CFlaceTerrainSceneNode::onDeserializedWithChildren()
{
	clearCurrentTerrainMeshes();

	for (int i=0; i<(int)TemporaryTerrainTilesIds.size(); ++i)
	{
		irr::s32 id = TemporaryTerrainTilesIds[i];
		irr::scene::ISceneNode* node = getSceneManager()->getSceneNodeFromId(id, this);
		if (node)
			node->grab();

		CFlaceMeshSceneNode* fmesh = (CFlaceMeshSceneNode*)node;
		TerrainTiles.push_back(fmesh);
	}

	TemporaryTerrainTilesIds.clear();
}