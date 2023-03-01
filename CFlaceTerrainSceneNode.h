// Copyright (C) 2002-2014 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_FLACE_TERRAIN_SCENE_NODE_H_INCLUDED__
#define __C_FLACE_TERRAIN_SCENE_NODE_H_INCLUDED__

#include "ISceneNode.h"
#include "EFlaceSceneNodeTypes.h"
#include "S3DVertex.h"
#include "IFlaceSerializationSupport.h"
#include "CFlaceTerrainSceneNode.h"

class CFlaceMeshSceneNode;

//! Scene node which is a path. 
class CFlaceTerrainSceneNode : public irr::scene::ISceneNode, public IFlaceSerializationSupport
{
public:

	//! constructor
	CFlaceTerrainSceneNode(IUndoManager* undo, irr::scene::ISceneNode* parent, irr::scene::ISceneManager* mgr, 
		irr::video::IVideoDriver* driver, irr::s32 id);

	~CFlaceTerrainSceneNode();

	//! pre render event
	virtual void OnRegisterSceneNode();

	//! render
	virtual void render();

	//! returns the axis aligned bounding box of this node
	virtual const irr::core::aabbox3d<irr::f32>& getBoundingBox() const;
	
	//! Writes attributes of the scene node.
	virtual void serializeAttributes(irr::io::IAttributes* out, irr::io::SAttributeReadWriteOptions* options=0) const;

	//! Reads attributes of the scene node.
	virtual void deserializeAttributes(irr::io::IAttributes* in, irr::io::SAttributeReadWriteOptions* options=0);

	//! Returns type of the scene node
	virtual irr::scene::ESCENE_NODE_TYPE getType() const { return (irr::scene::ESCENE_NODE_TYPE)EFSNT_FLACE_TERRAIN; }

	//! Creates a clone of this scene node and its children.
	virtual irr::scene::ISceneNode* clone(irr::scene::ISceneNode* newParent=0, irr::scene::ISceneManager* newManager=0, IUndoManager* undo=0);

	//! serialize
	void serialize(CFlaceSerializer* serializer);

	//! deserialize
	void deserialize(CFlaceDeserializer* deserializer);

	// called when the node and its children were read, so the node is able to link with its children if necessary
	virtual void onDeserializedWithChildren();

	//! Returns the material based on the zero based index i.
	virtual irr::video::SMaterial& getMaterial(irr::u32 num);

	//! Get amount of materials used by this scene node.
	virtual irr::u32 getMaterialCount() const;

	//! returns the exact height of a position in the terrain, even if the position is not exactly at the vertex of a polygon. 
	//! It is very fast for querying the height, as opposed to using Irrlicht's collision methods (which would have to iterate all triangles).
	//! Also, if the position given is outside of the terrain, a height at the nearest border is returned.
	irr::f32 getExactTerrainHeightClampedAtPosition(irr::f32 globalPixelX, irr::f32 globalPixelY, irr::core::vector3df* outNormal=0);

	struct STerrainData
	{
		irr::f32 Height;
		irr::s32 UserSetTextureIndex;		// texture index set by the user

		// runtime values
		irr::s32 MainTextureIndex;			// when rendering, this is the first texture sent to the 3D driver
		irr::s32 BlendingToTextureIndex;	// when rendering, this is the second texture sent to the 3D driver
		irr::u8  BlendFactorPerVertex[4];
	};

	struct SGrassInstance
	{
		irr::f32 Height;
		irr::f32 Width;
		irr::f32 PosX;
		irr::f32 PosZ;
		irr::f32 Rotation;
		irr::s32 TextureIndex;
	};

	struct STreeDistribution
	{
		irr::f32 percentOfTerrainCoveredWithThis;
		irr::scene::IAnimatedMesh* Mesh;
	};

	struct SGrassDistribution
	{
		irr::f32 percentOfTerrainCoveredWithThis;
		irr::video::ITexture* Texture;
		irr::f32 height;
		irr::f32 width;
		irr::f32 maxPosHeight;
	};

	//! terrain to generate
	//! topology: 1=hills 2=desert 3=flat
	void generateTerrain(irr::s32 sideLen, irr::s32 cellSize, irr::s32 maxHeight, int topology, 
		irr::video::ITexture* pTextureGrass, 
		irr::video::ITexture* pTextureRock,
		irr::video::ITexture* pTextureSand,
		STreeDistribution* pTreeDistribution, irr::s32 nTreeDistributionCount,
		SGrassDistribution* pGrassDistribution, irr::s32 nGrassDistributionCount);

	//! generates new terrain from heightmap
	void loadHeightMap(irr::s32 sideLenX, irr::s32 sideLenY, irr::s32 cellSize, irr::f32* pData, 
		irr::video::ITexture* pTextureGrass,
		irr::video::ITexture* pTextureRock = 0,
		irr::video::ITexture* pTextureSand = 0,
		irr::video::ITexture* pGrassSpriteTexture = 0);

	void distributeMeshes(irr::scene::IAnimatedMesh* tree, irr::f32 distribution, IUndoManager* undo=0, const irr::c8* basename="tree");

	void replaceTexture(int idx, irr::video::ITexture* newtexture, IUndoManager* undo);
	
	// Robbo added
	void setTextureHeights(irr::f32 tTexLow, irr::f32 tTexMed);
	void calculateBlendingAll(int startCellX, int startCellY, int endCellX, int endCellY);
	void setTextureBlend(int TexBlend);

	enum E_TERRAIN_LIGHTING_TYPE
	{
		ETLT_NONE = 0,
		ETLT_DYNAMIC,
		ETLT_LIGHTMAP_VERTEX_COLORS,

		ETLT_FORCE_32_BIT = 0x77ffffff
	};

	E_TERRAIN_LIGHTING_TYPE getLightingType();
	void setLightingType(E_TERRAIN_LIGHTING_TYPE nType, IUndoManager* undo);

	bool getSelectedTerrainTileFromScreenCoords(int x, int y, irr::core::vector2di& rOut);
	void drawEditBrushSelection(irr::core::vector2di tile, irr::video::SColor clr, irr::f32 brushSize);
	void drawEditBrushSelectionRaiseTool(irr::core::vector2di tile, irr::video::SColor clr, irr::f32 brushSize, irr::f32 additionalHeight, irr::f32 sphereFactor = 0.0f);
	void drawEditBrushSelectionMountainValleyTool(irr::core::vector2di tile, irr::video::SColor clr, irr::f32 brushSize, irr::f32 additionalHeight);
	void paintTexture(int screenCoord2DX, int screenCoord2DY, irr::f32 brushSize, irr::video::ITexture* texture, IUndoManager* undo);
	void raiseLowerTerrain(irr::core::vector2di tile, irr::f32 brushSize, irr::f32 height, IUndoManager* undo, irr::f32 sphereFactor = 0.0f);
	void mountainValleyTerrain(irr::core::vector2di tile, irr::f32 brushSize, irr::f32 height, IUndoManager* undo);
	void modifyTerrain(irr::core::vector2di tile, irr::f32 brushSize, bool smooth, bool noise, bool flatten, IUndoManager* undo);
	void paintGrass(irr::core::vector2di tile, irr::f32 brushSize, bool removeGrass, irr::video::ITexture* tex, irr::f32 width, irr::f32 height, IUndoManager* undo);
	
	void resetTerrainDataFromSnapshot(irr::f32* pTerrainData);
	irr::f32* createTerrainDataSnapshot();

	void resetTerrainGrassDataFromSnapshot(irr::f32* pTerrainData);
	irr::f32* createTerrainGrassDataSnapshot();

	void resortChildIntoCorrectTerrainTileMesh(irr::scene::ISceneNode* node, IUndoManager* undo);

	virtual irr::core::vector3df getDisplacement() { return Displacement; }
	

protected:

	void recalculateBoundingBox();
	void clearCurrentTerrainMeshes();
	void clearCachedCollisionTrianglesFromTerrainMeshes();
	void createCollisionTrianglesForTerrainMeshes();
	void clearTerrainTextures();
	void createTerrainSceneNodes();

	void setThreeTexturesBasedOnHeight(); 
	void generateGrass(SGrassDistribution* pGrassDistribution, irr::s32 nGrassDistributionCount);

	void updateMeshesFromTerrainData(int startCellX, int startCellY, int endCellX, int endCellY);
	void updateMeshesFromTerrainData();

	void calculateBlendingFactors(int startCellX, int startCellY, int endCellX, int endCellY);
	void calculateBlendingFactors();

	irr::scene::SMeshBuffer* getOrCreateMeshBuffer(irr::scene::SMesh* mesh, irr::s32 mainTextureIndex, 
		irr::s32 blendingToTextureIndex, irr::s32 nWithFreeVertices, irr::s32 nWithFreeIndices, bool forGrass);
		
	void syncMaterials();
	irr::s32 findTextureIndexOrAddNewOne(irr::video::ITexture* tex);
	irr::s32 getTerrainMeshIndex(irr::s32 tileX, irr::s32 tileY);
	irr::s32 getTerrainCellIndex(irr::s32 cellX, irr::s32 cellY);
	irr::scene::SMesh* getTerrainTileMesh(irr::s32 tileX, irr::s32 tileY);
	CFlaceMeshSceneNode* getTerrainTileMeshSceneNode(irr::s32 tileX, irr::s32 tileY);
	CFlaceMeshSceneNode* getTerrainTileMeshSceneNodeFromGlobalPixelPosClamped(irr::f32 pixelX, irr::f32 pixelZ);
	STerrainData* getTerrainData(irr::s32 globalCellX, irr::s32 globalCellY);
	STerrainData* getTerrainDataClamped(irr::s32 globalCellX, irr::s32 globalCellY);
	irr::f32 getTerrainDataHeightClamped(irr::s32 globalCellX, irr::s32 globalCellY);
	irr::core::vector3df getTerrain3DPositionClamped(irr::s32 globalCellX, irr::s32 globalCellY);
	
	void getMinMaxHeightOfTerrainDataInBrush(irr::core::vector2di tile, irr::s32 brushSize, irr::f32& rOutMinValue, irr::f32& rOutMaxValue);

	struct SOldMeshPositionsInTerrain
	{
		irr::f32 oldHeight;
		irr::scene::ISceneNode* node;
	};

	void getEmbeddedMeshPositionsInTerrain(irr::core::array<SOldMeshPositionsInTerrain>& outArr, irr::core::vector2di tile, irr::s32 brushSize);
	void adjustEmbeddedMeshHeights(irr::core::array<SOldMeshPositionsInTerrain>& positions, IUndoManager* undo);
	
	int SideLength;
	int CellSize;
	int TileSize;
	int TileCountX;
	int TileCountY;
	int CellCountX;
	int CellCountY;
	int CellsPerTileSide;
	int MaxHeight;
	int texBlend;
	float tTexHeightLow;
	float tTexHeightMed;
	E_TERRAIN_LIGHTING_TYPE LightingType;

	irr::core::vector3df Displacement;

	// user adjustable sizes
	irr::f32 TextureScale;

	// data for recreating the terrain (won't be saved for published apps to save space)

	irr::core::array<irr::video::ITexture*> Textures;
	irr::core::array<STerrainData> TerrainData;
	irr::core::array<SGrassInstance> GrassInstances;
	bool GrassUsesWind;

	irr::core::aabbox3d<irr::f32> BBox;
	irr::core::array<CFlaceMeshSceneNode*> TerrainTiles;
	
	// runtime
	// material dummies
	irr::core::array<irr::video::SMaterial> DummyMaterials;
	irr::video::IVideoDriver* Driver;

	// temporary and runtime
	irr::core::array<irr::s32> TemporaryTerrainTilesIds; // only needed shortly after deserializing

};


#endif

