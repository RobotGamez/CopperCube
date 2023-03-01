// Copyright (C) 2002-2007 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CFlaceLightSceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "ICameraSceneNode.h"
#include "CFlaceSerializer.h"
#include "CFlaceDeserializer.h"
#include "CFlaceAttributeSerializationHelper.h"
#include "CCCAttributeStrings.h"

using namespace irr;
using namespace scene;

//! constructor
CFlaceLightSceneNode::CFlaceLightSceneNode(IUndoManager* undo, ISceneNode* parent, ISceneManager* mgr, s32 id,	
	const core::vector3df& position, video::SColorf color,f32 radius)
: ILightSceneNode(parent, mgr, id, position, undo), EditorTexture(0), TriedToLoadTexture(false)
{
	#ifdef _DEBUG
	setDebugName("CFlaceLightSceneNode");
	#endif

	LightData.Direction.set(-0.256358f, -0.809352f, 0.528423f);
	LightData.Direction.normalize();

	LightData.DiffuseColor = color;
	// set some useful specular color
	LightData.SpecularColor = color.getInterpolated(video::SColor(255,255,255,255),0.7f);

	core::dimension2df size(10,10);
	f32 avg = (size.Width + size.Height)/6;
	BBox.MinEdge.set(-avg,-avg,-avg);
	BBox.MaxEdge.set(avg,avg,avg);
	
	setRadius(radius);
	doLightRecalc();
}


CFlaceLightSceneNode::~CFlaceLightSceneNode()
{
	if (EditorTexture)
		EditorTexture->drop();
}


//! pre render event
void CFlaceLightSceneNode::OnRegisterSceneNode()
{
	doLightRecalc();

	if (IsVisible)
	{
		SceneManager->registerNodeForRendering(this, ESNRP_LIGHT);

		if (DebugDataVisible)
			SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);

		if (!TriedToLoadTexture && SceneManager && SceneManager->getParameters()->getAttributeAsBool(IRR_SCENE_MANAGER_IS_EDITOR))
		{
			irr::core::stringc strTexture = SceneManager->getParameters()->getAttributeAsString(IRR_SCENE_MANAGER_EDITOR_DEFAULT_TEXTURES_DIR);

			if (getLightType() == irr::video::ELT_DIRECTIONAL)
				strTexture += "~default_lightdirectional.png";
			else
				strTexture += "~default_light.png";		
			
			TriedToLoadTexture = true;
			EditorTexture = SceneManager->getVideoDriver()->getTexture(strTexture.c_str());
			
			if (EditorTexture)
				EditorTexture->grab();
		}

		ISceneNode::OnRegisterSceneNode();
	}
}


//! render
void CFlaceLightSceneNode::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();
	if (!driver)
		return;

	if (SceneManager->getSceneNodeRenderPass() == scene::ESNRP_TRANSPARENT)
	{
		if ( DebugDataVisible )
		{
			bool bShadowMapEnabled = driver->isShadowMapEnabled();
			if (bShadowMapEnabled)
				driver->enableShadowMap(false);

			// draw billboard

			driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
	      
			// draw light as billboard

			scene::ICameraSceneNode* camera = SceneManager->getActiveCamera();
			if (camera)
			{
				video::S3DVertex vertices[4];
				u16 indices[6];

				video::SColor col = LightData.DiffuseColor.toSColor();

				core::dimension2df size(10,10);

				core::vector3df pos = getAbsolutePosition();

				core::vector3df campos = camera->getAbsolutePosition();
				core::vector3df target = camera->getTarget();
				core::vector3df up = camera->getUpVector();
				core::vector3df view = target - campos;
				view.normalize();

				core::vector3df horizontal = up.crossProduct(view);
				if ( horizontal.getLength() == 0 )
				{
					horizontal.set(up.Y,up.X,up.Z);
				}
				horizontal.normalize();
				horizontal *= 0.5f * size.Width;

				core::vector3df vertical = horizontal.crossProduct(view);
				vertical.normalize();
				vertical *= 0.5f * size.Height;

				view *= -1.0f;

				for (s32 i=0; i<4; ++i)
					vertices[i].Normal = view;

				vertices[0].Pos = pos + horizontal + vertical;
				vertices[1].Pos = pos + horizontal - vertical;
				vertices[2].Pos = pos - horizontal - vertical;
				vertices[3].Pos = pos - horizontal + vertical;


				indices[0] = 0;
				indices[1] = 2;
				indices[2] = 1;
				indices[3] = 0;
				indices[4] = 3;
				indices[5] = 2;

				vertices[0].TCoords.set(0.0f, 1.0f);
				vertices[0].Color = col;

				vertices[1].TCoords.set(0.0f, 0.0f);
				vertices[1].Color = col;

				vertices[2].TCoords.set(1.0f, 0.0f);
				vertices[2].Color = col;

				vertices[3].TCoords.set(1.0f, 1.0f);
				vertices[3].Color = col;

				// draw billoard

				if ( DebugDataVisible & scene::EDS_BBOX )
				{
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
					video::SMaterial m;
					m.Lighting = false;
					driver->setMaterial(m);
					driver->draw3DBox(BBox, video::SColor(0,208,195,152));
				}

				driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

				video::SMaterial material;
				material.Lighting = false;
				material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
				//material.MaterialTypeParam = 255;
				material.TextureLayer[0].Texture = EditorTexture;

				driver->setMaterial(material);

				driver->drawIndexedTriangleList(vertices, 4, indices, 2);

				// draw radius

				if ( ( DebugDataVisible & scene::EDS_MESH_WIRE_OVERLAY) &&
					 LightData.Type == video::ELT_POINT )
				{
					video::SMaterial material2;
					material2.Lighting = false;
					driver->setMaterial(material2);

					core::vector3df pt1 = LightData.Position;
					core::vector3df pt2;

					const s32 count = 16;
					for (s32 j=0; j<count; ++j)
					{
						pt2 = pt1;

						f32 p = j / (f32)count * (core::PI*2);
						pt1 = LightData.Position + core::vector3df(sin(p)*LightData.Radius, 0, cos(p)*LightData.Radius);

						driver->draw3DLine(pt1, pt2, LightData.DiffuseColor.toSColor());
					}
				}

				// draw direction of directional light

				if (( LightData.Type == video::ELT_DIRECTIONAL || LightData.Type == video::ELT_SPOT ) &&
					 DebugDataVisible )
				{
					video::SMaterial m;
					m.Lighting = false;
					driver->setMaterial(m);

					const f32 lineLength = 50.0f;
					const f32 headHeight = 5.0f;

					irr::core::vector3df arrowBegin = irr::core::vector3df(0,0,0); //getAbsolutePosition();
					irr::core::vector3df arrowEnd = irr::core::vector3df(0,0,lineLength); //arrowBegin + (LightData.Direction * lineLength);
					irr::core::vector3df arrowHeadEnd1 = arrowEnd - irr::core::vector3df(0, headHeight, headHeight);
					irr::core::vector3df arrowHeadEnd2 = arrowEnd - irr::core::vector3df(0, -headHeight, headHeight);
					
					irr::video::SColor clr = LightData.DiffuseColor.toSColor();
					video::SColor linecolor = defaultEditorLineColor;

					irr::core::matrix4 matarrow;
					matarrow.buildCameraLookAtMatrixLH(getAbsolutePosition(), getAbsolutePosition() + (LightData.Direction * lineLength), irr::core::vector3df(0,1,0));
					matarrow.makeInverse();
					driver->setTransform(video::ETS_WORLD,matarrow);

					irr::core::vector3df translate[] = {
						irr::core::vector3df(0,0,0),
						irr::core::vector3df(10.0f, 0.0f, 0.0f),
						irr::core::vector3df(-10.0f, 0.0f, 0.0f)};

					for (int i=0; i<sizeof(translate) / sizeof(irr::core::vector3df); ++i)
					{
						irr::core::vector3df t = translate[i];
						driver->draw3DLine(arrowBegin + t, arrowEnd      + t , linecolor);
						driver->draw3DLine(arrowEnd   + t, arrowHeadEnd1 + t, linecolor);
						driver->draw3DLine(arrowEnd   + t, arrowHeadEnd2 + t, linecolor);
					}

					
					// box at arrow end for moving

					driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

					arrowEnd = getAbsolutePosition() + (LightData.Direction * lineLength);;
					irr::core::aabbox3df boxArrowEnd;
					irr::core::vector3df boxsz(1.5f, 1.5f, 1.5f);
					boxArrowEnd.MinEdge = arrowEnd - boxsz;
					boxArrowEnd.MaxEdge = arrowEnd + boxsz;
					driver->draw3DBox(boxArrowEnd, linecolor);
				}
			}		

			

			// enable again

			if (bShadowMapEnabled)
				driver->enableShadowMap(true);
		}
	}
	else
	{
		/*if ( DebugDataVisible & scene::EDS_BBOX )
		{
			driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
			video::SMaterial m;
			m.Lighting = false;
			driver->setMaterial(m);
	
			switch ( LightData.Type )
			{
				case video::ELT_POINT:
				case video::ELT_SPOT:
					driver->draw3DBox(BBox, LightData.DiffuseColor.toSColor());
					break;
	
				case video::ELT_DIRECTIONAL:
					{
						
					}
					break;
			}
		}*/

		driver->addDynamicLight(LightData);
	}
}


//! returns the light data
void CFlaceLightSceneNode::setLightData(const video::SLight& light)
{
	LightData = light;
	ISceneNode::setPosition(light.Position);
	ISceneNode::updateAbsolutePosition();
}


//! \return Returns the light data.
video::SLight& CFlaceLightSceneNode::getLightData()
{
	return LightData;
}

//! \return Returns the light data.
const video::SLight& CFlaceLightSceneNode::getLightData() const
{
	return LightData;
}



//! Sets the light type.
/** \param type The new type. */
void CFlaceLightSceneNode::setLightType(video::E_LIGHT_TYPE type)
{
	LightData.Type = type;
}


//! Gets the light type.
/** \return The current light type. */
video::E_LIGHT_TYPE CFlaceLightSceneNode::getLightType() const
{
	return LightData.Type;
}


//! Sets the light's radius of influence.
/** Outside this radius the light won't lighten geometry and cast no
shadows. Setting the radius will also influence the attenuation, setting
it to (0,1/radius,0). If you want to override this behavior, set the
attenuation after the radius.
\param radius The new radius. */
void CFlaceLightSceneNode::setRadius(f32 radius)
{
	LightData.Radius=radius;
	LightData.Attenuation.set(0.f, 1.0f / radius, 0.f);
}


//! Gets the light's radius of influence.
/** \return The current radius. */
f32 CFlaceLightSceneNode::getRadius() const
{
	return LightData.Radius;
}

//! Gets the light's inner cone - Robbo
f32 CFlaceLightSceneNode::getInnerCone()
{
	return LightData.InnerCone;
}

//! Sets the light's radius inner cone
void CFlaceLightSceneNode::setInnerCone(f32 inner)
{
	LightData.InnerCone=inner;
}


//! Sets whether this light casts shadows.
/** Enabling this flag won't automatically cast shadows, the meshes
will still need shadow scene nodes attached. But one can enable or
disable distinct lights for shadow casting for performance reasons.
\param shadow True if this light shall cast shadows. */
void CFlaceLightSceneNode::enableCastShadow(bool shadow)
{
	LightData.CastShadows=shadow;
}


//! Check whether this light casts shadows.
/** \return True if light would cast shadows, else false. */
bool CFlaceLightSceneNode::getCastShadow() const
{
	return LightData.CastShadows;
}


//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32>& CFlaceLightSceneNode::getBoundingBox() const
{
	return BBox;
}


void CFlaceLightSceneNode::doLightRecalc()
{
	if (LightData.Type == video::ELT_SPOT || LightData.Type == video::ELT_POINT)
	{
		const f32 r = LightData.Radius * LightData.Radius * 0.5f; // Robbo
		BBox.MaxEdge.set( r, r, r ); // Robbo
		BBox.MinEdge.set( -r, -r, -r ); // Robbo
		setAutomaticCulling( scene::EAC_BOX );
		LightData.Position = getAbsolutePosition();
	}

	if (LightData.Type == video::ELT_DIRECTIONAL)
	{
		setAutomaticCulling( scene::EAC_OFF );
	}
}

//! Writes attributes of the scene node.
void CFlaceLightSceneNode::serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const
{
	if (options && (options->Flags & irr::io::EARWF_IRRLICHT_1_6_COMPATIBILTY))
	{
		ILightSceneNode::serializeAttributes(out, options);

		out->addColorf	("AmbientColor", LightData.AmbientColor);
		out->addColorf	("DiffuseColor", LightData.DiffuseColor);
		out->addColorf	("SpecularColor", LightData.SpecularColor);
		out->addVector3d("Attenuation", LightData.Attenuation);
		out->addFloat	("Radius", LightData.Radius);
		out->addFloat	("OuterCone", LightData.OuterCone);
		out->addFloat	("InnerCone", LightData.InnerCone);
		out->addFloat	("Falloff", LightData.Falloff);
		out->addBool	("CastShadows", LightData.CastShadows);
		out->addEnum	("LightType", LightData.Type, video::LightTypeNames);
		return;
	}

	CFlaceAttributeSerializationHelper::serializeBaseAttributes(this, out, options);

	out->addColorf(sCCAttributeString_LightColor, LightData.DiffuseColor);

	if (LightData.Type == video::ELT_POINT || LightData.Type == video::ELT_SPOT)
	{
		out->addFloat("Radius", LightData.Radius);
		out->addBool("CastShadows", LightData.CastShadows);
		out->addVector3d("Attenuation", LightData.Attenuation); // Robbo added
		out->addFloat("OuterCone", LightData.OuterCone); // Robbo added
		out->addFloat("InnerCone", LightData.InnerCone); // Robbo added
		out->addFloat("Falloff", LightData.Falloff); // Robbo added
	}

	if (LightData.Type == video::ELT_DIRECTIONAL || LightData.Type == video::ELT_SPOT)
		out->addVector3d("Direction", LightData.Direction);
}


//! Reads attributes of the scene node.
void CFlaceLightSceneNode::deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options)
{
	if (options && (options->Flags & irr::io::EARWF_IRRLICHT_1_6_COMPATIBILTY))
	{
		LightData.AmbientColor =	in->getAttributeAsColorf("AmbientColor");
		LightData.DiffuseColor =	in->getAttributeAsColorf("DiffuseColor");
		LightData.SpecularColor =	in->getAttributeAsColorf("SpecularColor");
		LightData.Radius = in->getAttributeAsFloat("Radius");
		if (in->existsAttribute("Attenuation")) // might not exist in older files
			LightData.Attenuation =	in->getAttributeAsVector3d("Attenuation");
		if (in->existsAttribute("OuterCone")) // might not exist in older files
			LightData.OuterCone =	in->getAttributeAsFloat("OuterCone");
		if (in->existsAttribute("InnerCone")) // might not exist in older files
			LightData.InnerCone =	in->getAttributeAsFloat("InnerCone");
		if (in->existsAttribute("Falloff")) // might not exist in older files
			LightData.Falloff =	in->getAttributeAsFloat("Falloff");
		LightData.CastShadows =	in->getAttributeAsBool("CastShadows");
		LightData.Type = (video::E_LIGHT_TYPE)in->getAttributeAsEnumeration("LightType", video::LightTypeNames);
		setRadius(LightData.Radius); // Robbo
		doLightRecalc();
		ILightSceneNode::deserializeAttributes(in, options);
		return;
	}

	CFlaceAttributeSerializationHelper::deserializeBaseAttributes(this, in, options);

	if (in->existsAttribute(sCCAttributeString_LightColor))
		LightData.DiffuseColor = in->getAttributeAsColorf(sCCAttributeString_LightColor);

	LightData.SpecularColor = LightData.DiffuseColor;

	if (in->existsAttribute("Radius")) // Robbo
		LightData.Radius = in->getAttributeAsFloat("Radius");
		setRadius(LightData.Radius);

	if (in->existsAttribute("CastShadows"))
		LightData.CastShadows = in->getAttributeAsBool("CastShadows");
	
	// Robbo added
	if (in->existsAttribute("OuterCone"))
		LightData.OuterCone = in->getAttributeAsFloat("OuterCone");
		LightData.InnerCone = in->getAttributeAsFloat("InnerCone");
		LightData.Falloff = in->getAttributeAsFloat("Falloff");
		LightData.Attenuation = in->getAttributeAsVector3d("Attenuation");
		
	// Robbo change
	if (in->existsAttribute("Direction"))
	{
		LightData.Direction = in->getAttributeAsVector3d("Direction");
		LightData.Direction.normalize();
	}
}


//! Creates a clone of this scene node and its children.
ISceneNode* CFlaceLightSceneNode::clone(ISceneNode* newParent, ISceneManager* newManager, IUndoManager* undo)
{
	if (!newParent) newParent = Parent;
	if (!newManager) newManager = SceneManager;

	CFlaceLightSceneNode* nb = new CFlaceLightSceneNode(undo, newParent, 
		newManager, ID, RelativeTranslation, LightData.DiffuseColor, LightData.Radius);

	nb->cloneMembers(this, newManager);
	nb->LightData = LightData;
	nb->BBox = BBox;

	nb->drop();
	return nb;
}



//! serialize
void CFlaceLightSceneNode::serialize(CFlaceSerializer* serializer)
{
	serializer->WriteBox(BBox);
	serializer->WriteS32(LightData.Type);
	serializer->WriteColorF(LightData.DiffuseColor);
	serializer->WriteColorF(LightData.SpecularColor);
	serializer->WriteBool(LightData.CastShadows);	
	serializer->Write3DVectF(LightData.Direction);
	serializer->WriteF32(LightData.Radius);
}


//! serialize
void CFlaceLightSceneNode::deserialize(CFlaceDeserializer* deserializer)
{
	irr::s32 nextPosAfterSceneTag = deserializer->getNextTagPos();

	BBox = deserializer->ReadBox();
	LightData.Type = (irr::video::E_LIGHT_TYPE)deserializer->ReadS32();
	LightData.DiffuseColor = deserializer->ReadColorF();
	LightData.SpecularColor = deserializer->ReadColorF();
	LightData.CastShadows = deserializer->ReadBool();
	LightData.Direction = deserializer->Read3DVectF();
	LightData.Radius = deserializer->ReadF32();
	setRadius(LightData.Radius);
}

//! sets the direction of a directional light
void CFlaceLightSceneNode::setDirection(const irr::core::vector3df& dir, IUndoManager* undo)
{
	if (undo)
		undo->addUndoPartChangeVector(this, &LightData.Direction, LightData.Direction, dir);

	LightData.Direction = dir;
}