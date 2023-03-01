// Copyright (C) 2002-2007 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_FLACE_LIGHT_SCENE_NODE_H_INCLUDED__
#define __C_FLACE_LIGHT_SCENE_NODE_H_INCLUDED__

#include "ILightSceneNode.h"
#include "EFlaceSceneNodeTypes.h"
#include "IFlaceSerializationSupport.h"


//! Scene node which is a dynamic light. You can switch the light on and off by 
//! making it visible or not, and let it be animated by ordinary scene node animators.
// This scene node is an own implementation for lights, to be able to display
// billboards where the light is.
class CFlaceLightSceneNode : public irr::scene::ILightSceneNode, public IFlaceSerializationSupport
{
public:

	//! constructor
	CFlaceLightSceneNode(IUndoManager* undo, irr::scene::ISceneNode* parent, irr::scene::ISceneManager* mgr, irr::s32 id,
		const irr::core::vector3df& position=irr::core::vector3df(0,0,0), 
		irr::video::SColorf color=irr::video::SColor(128,255,255,255), 
		irr::f32 range=50.0f);

	virtual ~CFlaceLightSceneNode();

	//! pre render event
	virtual void OnRegisterSceneNode();

	//! render
	virtual void render();

	//! set node light data from light info
	virtual void setLightData( const irr::video::SLight& light);

	//! \return Returns the light data.
	virtual irr::video::SLight& getLightData();

	//! \return Returns the light data.
	virtual const irr::video::SLight& getLightData() const;

	//! returns the axis aligned bounding box of this node
	virtual const irr::core::aabbox3d<irr::f32>& getBoundingBox() const;

	//! Returns type of the scene node
	virtual irr::scene::ESCENE_NODE_TYPE getType() const { return (irr::scene::ESCENE_NODE_TYPE)EFSNT_FLACE_LIGHT; }

	//! Writes attributes of the scene node.
	virtual void serializeAttributes(irr::io::IAttributes* out, irr::io::SAttributeReadWriteOptions* options=0) const;

	//! Reads attributes of the scene node.
	virtual void deserializeAttributes(irr::io::IAttributes* in, irr::io::SAttributeReadWriteOptions* options=0);

	//! Creates a clone of this scene node and its children.
	virtual ISceneNode* clone(irr::scene::ISceneNode* newParent=0, irr::scene::ISceneManager* newManager=0, IUndoManager* undo=0); 

	//! Sets the light's radius of influence.
	/** Outside this radius the light won't lighten geometry and cast no
	shadows. Setting the radius will also influence the attenuation, setting
	it to (0,1/radius,0). If you want to override this behavior, set the
	attenuation after the radius.
	\param radius The new radius. */
	virtual void setRadius(irr::f32 radius);

	//! Gets the light's radius of influence.
	/** \return The current radius. */
	virtual irr::f32 getRadius() const;
	
	//! Gets the light inner cone - RC
	virtual irr::f32 getInnerCone();
	
	//! Sets the light inner cone - RC
	virtual void setInnerCone(irr::f32 inner);

	//! Sets the light type.
	/** \param type The new type. */
	virtual void setLightType(irr::video::E_LIGHT_TYPE type);

	//! Gets the light type.
	/** \return The current light type. */
	virtual irr::video::E_LIGHT_TYPE getLightType() const;

	//! Sets whether this light casts shadows.
	/** Enabling this flag won't automatically cast shadows, the meshes
	will still need shadow scene nodes attached. But one can enable or
	disable distinct lights for shadow casting for performance reasons.
	\param shadow True if this light shall cast shadows. */
	virtual void enableCastShadow(bool shadow=true);

	//! Check whether this light casts shadows.
	/** \return True if light would cast shadows, else false. */
	virtual bool getCastShadow() const;

	//! serialize
	void serialize(CFlaceSerializer* serializer);

	//! deserialize
	void deserialize(CFlaceDeserializer* deserializer);

	//! sets the direction of a directional light
	virtual void setDirection(const irr::core::vector3df& dir, IUndoManager* undo);

private:

	irr::video::SLight LightData;
	irr::core::aabbox3d<irr::f32> BBox;
	void doLightRecalc();


	irr::video::ITexture* EditorTexture;
	bool TriedToLoadTexture;
};


#endif

