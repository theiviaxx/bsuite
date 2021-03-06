/* Copyright (c) 2012, Sebastian Thiel
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *   Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, 
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <maya/MIOStream.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPlug.h>
#include <maya/MFileObject.h>
#include <maya/MStringArray.h>
#include <maya/MFnMesh.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatPointArray.h>
#include <maya/MPlugArray.h>


#include "mayabaselib/ogl_headers.h"

#include "mayabaselib/base.h"
#include "util.h"
#include "visnode.h"

#include "assert.h"

#include "baselib/math_util.h"

#ifdef _OPENMP
	#include <omp.h>
#endif


//********************************************************************
//**	Class Implementation
//********************************************************************
/////////////////////////////////////////////////////////////////////

const MTypeId PtexVisNode::typeId(0x00108bdd);
const MString PtexVisNode::typeName("ptexVisNode");


// Attributes
MObject PtexVisNode::aPtexFileName;
MObject PtexVisNode::aPtexFilterType;
MObject PtexVisNode::aPtexFilterSize;
MObject PtexVisNode::aDisplayMode;
MObject PtexVisNode::aDisplayCacheMode;
MObject PtexVisNode::aSampleMultiplier;
MObject PtexVisNode::aInMesh;

MObject PtexVisNode::aOutNumChannels;
MObject PtexVisNode::aOutNumFaces;
MObject PtexVisNode::aOutHasEdits;
MObject PtexVisNode::aOutHasMipMaps;
MObject PtexVisNode::aOutAlphaChannel;
MObject PtexVisNode::aNeedsCompute;
MObject PtexVisNode::aOutMetaDataKeys;
MObject PtexVisNode::aOutMeshType;
MObject PtexVisNode::aOutDataType;
MObject PtexVisNode::aOutUBorderMode;
MObject PtexVisNode::aOutVBorderMode;
MObject PtexVisNode::aOutNumSamples;
MObject PtexVisNode::aGlPointSize;


PtexVisNode::PtexVisNode()
	: m_ptex_num_channels(0)
	, m_needs_cache_update(false)
    , m_gl_point_size(1.0)
{}

PtexVisNode::~PtexVisNode()
{}

void PtexVisNode::postConstructor()
{
	setMPSafe(false);
}

// DESCRIPTION:
// creates an instance of the node
void* PtexVisNode::creator()
{
	return new PtexVisNode();
}

void add_border_mode_fields(MFnEnumAttribute& mfnEnum)
{
	mfnEnum.addField("clamp", 0);
	mfnEnum.addField("black", 1);
	mfnEnum.addField("periodic", 2);
}

// DESCRIPTION:
//
MStatus PtexVisNode::initialize()
{
	MStatus status;
	MFnNumericAttribute numFn;
	MFnTypedAttribute typFn;
	MFnStringArrayData stringArrayFn;

	// Input attributes
	////////////////////
	aPtexFileName = typFn.create("ptexFilePath", "ptfp", MFnData::kString, &status);
	CHECK_MSTATUS(status);
	typFn.setInternal(true);
	
	aInMesh = typFn.create("inMesh", "i", MFnData::kMesh, &status);
	typFn.setDefault(MObject::kNullObj);
	CHECK_MSTATUS(status);

	MFnEnumAttribute mfnEnum;
	aPtexFilterType = mfnEnum.create("ptexFilterType", "ptft", 0);
	mfnEnum.addField("Point",      0);
	mfnEnum.addField("Bilinear",   1);
	mfnEnum.addField("Box",        2);
	mfnEnum.addField("Gaussian",   3);
	mfnEnum.addField("Bicubic",    4);
	mfnEnum.addField("BSpline",    5);
	mfnEnum.addField("CatmullRom", 6);
	mfnEnum.addField("Mitchell",   7);
	mfnEnum.setKeyable(true);

	aDisplayMode = mfnEnum.create("displayMode", "dm");
	mfnEnum.addField("texelTile", (short)TexelTile);
	mfnEnum.addField("faceRelative", (short)FaceRelative);
	mfnEnum.addField("faceAbsolute", (short)FaceAbsolute);
	mfnEnum.setDefault((short)FaceAbsolute);
	mfnEnum.setKeyable(true);
	
	aDisplayCacheMode = mfnEnum.create("displayCacheMode", "dcm");
	mfnEnum.addField("SystemCache", (short)DCSystem);
	mfnEnum.addField("GPUCache", (short)DCGPU);
	mfnEnum.setDefault((short)DCSystem);
	
	aPtexFilterSize = numFn.create("ptexFilterSize", "ptfs", MFnNumericData::kFloat, 0.001);
	numFn.setMin(0.0);
	numFn.setMax(1.0);
	numFn.setKeyable(true);
	
	aGlPointSize = numFn.create("glPointSize", "glps", MFnNumericData::kInt, 1.0);
	numFn.setMin(1.0);
	numFn.setKeyable(true);
	numFn.setInternal(true);
	
	aSampleMultiplier = numFn.create("sampleMultiplier", "smlt", MFnNumericData::kFloat, 1.0);
	numFn.setMin(0.0001f);
	numFn.setKeyable(true);
	
	// Output attributes
	/////////////////////
	aNeedsCompute = numFn.create("needsComputation", "nc", MFnNumericData::kInt);
	setup_as_output(numFn);
	
	aOutMetaDataKeys = typFn.create("outMetaDataKeys", "omdk", MFnData::kStringArray, &status);
	CHECK_MSTATUS(status);
	typFn.setDefault(stringArrayFn.create());
	setup_as_output(typFn);
	
	aOutNumChannels = numFn.create("outNumChannels", "onc", MFnNumericData::kInt);
	setup_as_output(numFn);
	
	aOutNumFaces = numFn.create("outNumFaces", "onf", MFnNumericData::kInt);
	setup_as_output(numFn);
	
	aOutAlphaChannel = numFn.create("outAlphaChannel", "oac", MFnNumericData::kInt);
	setup_as_output(numFn);
	
	aOutHasEdits = numFn.create("outHasEdits", "ohe", MFnNumericData::kBoolean);
	setup_as_output(numFn);
	
	aOutHasMipMaps = numFn.create("outHasMipMaps", "ohm", MFnNumericData::kBoolean);
	setup_as_output(numFn);
	
	aOutNumSamples = numFn.create("outNumSamples", "ons", MFnNumericData::kInt);
	setup_as_output(numFn);
	
	aOutMeshType = mfnEnum.create("outMeshType", "omt");
	setup_as_output(mfnEnum);
	mfnEnum.addField("triangle", 0);
	mfnEnum.addField("quad", 1);
	
	aOutDataType = mfnEnum.create("outDataType", "odt");
	setup_as_output(mfnEnum);
	mfnEnum.addField("int8", 0);
	mfnEnum.addField("int16", 1);
	mfnEnum.addField("half", 2);
	mfnEnum.addField("float", 3);
	
	aOutUBorderMode = mfnEnum.create("outUBorderMode", "oubm");
	setup_as_output(mfnEnum);
	add_border_mode_fields(mfnEnum);
	aOutVBorderMode = mfnEnum.create("outVBorderMode", "ovbm");
	setup_as_output(mfnEnum);
	add_border_mode_fields(mfnEnum);
	

	// Add attributes
	/////////////////
	CHECK_MSTATUS(addAttribute(aPtexFileName));
	CHECK_MSTATUS(addAttribute(aPtexFilterType));
	CHECK_MSTATUS(addAttribute(aPtexFilterSize));
	CHECK_MSTATUS(addAttribute(aGlPointSize));
	CHECK_MSTATUS(addAttribute(aDisplayMode));
	CHECK_MSTATUS(addAttribute(aDisplayCacheMode));
	CHECK_MSTATUS(addAttribute(aSampleMultiplier));
	CHECK_MSTATUS(addAttribute(aInMesh));
	
	CHECK_MSTATUS(addAttribute(aOutMetaDataKeys));
	CHECK_MSTATUS(addAttribute(aNeedsCompute));
	CHECK_MSTATUS(addAttribute(aOutNumChannels));
	CHECK_MSTATUS(addAttribute(aOutNumFaces));
	CHECK_MSTATUS(addAttribute(aOutAlphaChannel));
	CHECK_MSTATUS(addAttribute(aOutHasEdits));
	CHECK_MSTATUS(addAttribute(aOutHasMipMaps));
	CHECK_MSTATUS(addAttribute(aOutMeshType));
	CHECK_MSTATUS(addAttribute(aOutDataType));
	CHECK_MSTATUS(addAttribute(aOutUBorderMode));
	CHECK_MSTATUS(addAttribute(aOutVBorderMode));
	CHECK_MSTATUS(addAttribute(aOutNumSamples));
	
	

	// All input affect the output color
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutMetaDataKeys));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutNumChannels));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutNumFaces));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutHasEdits));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutHasMipMaps));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutAlphaChannel));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutMeshType));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutDataType));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutUBorderMode));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,	aOutVBorderMode));
	CHECK_MSTATUS(attributeAffects(aInMesh,			aNeedsCompute));
	CHECK_MSTATUS(attributeAffects(aDisplayMode,	aNeedsCompute));
	CHECK_MSTATUS(attributeAffects(aDisplayCacheMode,	aNeedsCompute));
	CHECK_MSTATUS(attributeAffects(aSampleMultiplier,	aNeedsCompute));
	CHECK_MSTATUS(attributeAffects(aPtexFileName,   aNeedsCompute));
	CHECK_MSTATUS(attributeAffects(aPtexFilterSize, aNeedsCompute));
	CHECK_MSTATUS(attributeAffects(aPtexFilterType, aNeedsCompute));

	return MS::kSuccess;
}

PtexFilter::FilterType PtexVisNode::to_filter_type(int type) 
{
	switch (type) {
	case 0: return PtexFilter::f_point;
	case 1: return PtexFilter::f_bilinear;
	case 2: return PtexFilter::f_box;
	case 3: return PtexFilter::f_gaussian;
	case 4: return PtexFilter::f_bicubic;
	case 5: return PtexFilter::f_bspline;
	case 6: return PtexFilter::f_catmullrom;
	case 7: return PtexFilter::f_mitchell;
	default: {
		// usually we would throw, but as long as we are intergated into the node tightly
		// we just handle it.
		cerr << "Invalid filter type: " << type << " - defaulting to point filtering" << endl;
		return PtexFilter::f_point;
	}
	}
}

bool PtexVisNode::assure_texture(MDataBlock& data)
{
	if (m_ptex_texture.get()) {
		return true;
	}
	
	const MString& filePath = data.inputValue(aPtexFileName).asString();
	if (filePath.length() == 0) {
		reset_output_info(data);
		return false;
	}
	
	// ptex crashes ungracefully if the file doesn't exist ... great
	MFileObject file;
	file.setRawFullName(filePath);
	if (!file.exists()) {
		reset_output_info(data);
		return false;
	}
	
	Ptex::String error;
	PtexTexturePtr ptex(gCache->get(file.resolvedFullName().asChar(), error));	// This pointer interface is ridiculous
	if (!ptex.get()) {
		m_error = error.c_str();
		reset_output_info(data);
		return false;
	}
	m_ptex_texture.swap(ptex);
	return true;
}

void PtexVisNode::reset_output_info(MDataBlock &data)
{
	// reset only the most important attributes
	data.outputValue(aOutAlphaChannel).setInt(0);
	data.outputValue(aOutHasEdits).setBool(false);
	data.outputValue(aOutHasMipMaps).setBool(false);
	data.outputValue(aOutNumChannels).setInt(0);
	data.outputValue(aOutNumFaces).setInt(0);
	
	MFnStringArrayData saFn;
	MObject empty = saFn.create();
	data.outputValue(aOutMetaDataKeys).setMObject(empty);
}

void PtexVisNode::release_texture_and_filter()
{
	PtexTexturePtr ptex;
	PtexFilterPtr pfilter;
	m_ptex_texture.swap(ptex);
	m_ptex_filter.swap(pfilter);
}

void PtexVisNode::release_cache()
{
	m_sysbuf.resize(0);
	m_gpubuf.resize(0);
	
	MPlug(thisMObject(), aOutNumSamples).setInt(0);
}

bool PtexVisNode::setInternalValueInContext(const MPlug &plug, const MDataHandle &dataHandle, MDGContext &ctx)
{
	if (plug == aPtexFileName) {
		release_texture_and_filter();
		release_cache();
	} else if (plug == aGlPointSize) {
		m_gl_point_size = static_cast<float>(dataHandle.asInt());
	}
	
	return false;
}

//! \return uv values for the given point on a triangle
template <typename T>
void uv_from_pos(const T& a, const T& b, const T& c, const T& p, float& out_u, float& out_v)
{
	T v0 = b - a;
	T v1 = c - a;
	T v2 = p - a;
	
	const float dot00 = dot(v0, v0);
	const float dot01 = dot(v0, v1);
	const float dot02 = dot(v0, v2);
	
	const float dot11 = dot(v1, v1);
	const float dot12 = dot(v1, v2);
	
	const float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
	
	out_u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	out_v = (dot00 * dot12 - dot01 * dot02) * invDenom;
}

template <typename Buffer>
bool PtexVisNode::update_sample_buffer(Buffer &buf)
{
	PtexTexture* tex = m_ptex_texture.get();
	PtexFilter* filter = m_ptex_filter.get();
	
	if (!tex | !filter) {
		m_error = "Cannot sample ptex without a valid texture and filter";
		return false;
	}
	
	if (tex->numChannels() > 4) {
		m_error = "Can only handle up to 4 channels currently";
		return false;
	}
	
	MObject thisNode(thisMObject());
	
	// OBTAIN SAMPLES
	/////////////////
	// lets just read the samples for now
	const int numFaces = tex->numFaces();
	if (numFaces == 0) {
		m_error = "Ptextured had zero faces - its empty or corrupt";
		return false;
	}
	const int numChannels = tex->numChannels();
	const DisplayMode displayMode = (DisplayMode)MPlug(thisNode, aDisplayMode).asShort();
	float mult = MPlug(thisNode, aSampleMultiplier).asFloat();
	
	// only absolute sampling supports higher values. The others are along UV!
	if (displayMode == TexelTile && mult > 1.0) {
		mult = 1.0;
	}
	
#ifdef _OPENMP
	// just use vector to assure we don't leak. We use direct memory access later, which is somewhat evil !
	typedef std::vector<size_t> SizeVector;
	SizeVector lut_destruct;
	lut_destruct.reserve(numFaces);	// prevents a call to the constructor ! We write the data later
	size_t* flut = &lut_destruct[0];
	
	// Ptex crashes if any filter is used - point seems to work well in multi-threaded mode !
	const int thread_count = to_filter_type(MPlug(thisNode, aPtexFilterType).asShort()) == PtexFilter::f_point 
									? omp_get_max_threads()
									: 1;
#endif
	
	// count memory we require for preallocation
	size_t numTexels = 0;
	for (int i = 0; i < numFaces; ++i) {
#ifdef _OPENMP
		// build a lookup table for each face, to allow accesing the texel directly
		flut[i] = numTexels;
#endif
		
		const Ptex::Res& r = tex->getFaceInfo(i).res;
		// emulate the way we use the multiplier later
		numTexels += (size_t)((int)(r.u() * mult) * (int)(r.v() * mult));
	}// for each face
	
	buf.resize(numTexels);
	
	if (!buf.begin_access()) {
		return false;
	}
	
	VtxPrimitive* opos = reinterpret_cast<VtxPrimitive*>(buf.begin(VertexArray));
	ColPrimitive* ocol = reinterpret_cast<ColPrimitive*>(buf.begin(ColorArray));
	
	assert(opos);
	assert(ocol);
	
	bool rval = false;
	Float4 pix;							// one pixel
	switch(displayMode)
	{
	case TexelTile:
	{
		const float inv_mult = 1.0f / mult;
		const float step = 0.01f * inv_mult;
		Float3		pos;			// position
		
		// NOTE: can't properly paralellize this without another LUT for the posX
		// As this is not really useful (unless you are debugging), we spare the work here
		for (int i = 0; i < numFaces; ++i) {
			const Ptex::FaceInfo& fi = tex->getFaceInfo(i);
			const int ures = (int)(fi.res.u() * mult);
			const int vres = (int)(fi.res.v() * mult);
					
			for (int u = 0; u < ures; ++u) {
				for (int v = 0; v < vres; ++v) {
					tex->getPixel(i, static_cast<int>(u * inv_mult), static_cast<int>(v * inv_mult), &pix.x, 0, numChannels);
					*opos++ = pos;
					*ocol++ = pix;
					pos.y += step;
				}// for each v texel
				pos.x += step;
				pos.y = 0.0f;
			}// for each u texel
		}// for each face
		rval = true;
		break;
	}// case texel
	case FaceRelative:	// fall through
	case FaceAbsolute:
	{
		if (tex->meshType() != Ptex::mt_triangle)  {
			m_error = "Cannot visualize non-triangle meshes for now";
			break;
		}
		
		MStatus stat;
		// init from conneted node - don't have datablock here :(
		MPlug inMeshPlug(thisNode, aInMesh);
		MPlugArray cons;
		inMeshPlug.connectedTo(cons, true, false);
		
		if (cons.length() != 1) {
			m_error = "no mesh connected to our inMesh attribute";
			break;
		}
		MFnMesh meshFn(cons[0].node(), &stat);
		
		
		// For now, lets support one-on-one mappings without sub-face support
		if (meshFn.numPolygons() != tex->numFaces()) {
			m_error = "Face count of texture does not match polygon count of connected mesh. Currently these must match one on one: ";
			m_error += meshFn.numPolygons();
			m_error += " != ";
			m_error += tex->numFaces();
			m_error += "(mesh.tricount != tex.tricount)";
			break;
		}
		
		if (tex->numFaces() == 0) {
			m_error = "Empty texture encountered";
			break;
		}
		
#if MAYA_API_VERSION > 200810
		#define TFLOAT3 Float3
		const Float3* vtx = reinterpret_cast<const Float3*>(meshFn.getRawPoints(&stat));
#else
		MFloatPointArray vtx;
		meshFn.getPoints(vtx);
		#define TFLOAT3 MFloatPoint
#endif
		MIntArray tcounts;
		MIntArray tverts;
		meshFn.getTriangles(tcounts, tverts);
		tcounts.clear();
		
		const float fsize = MPlug(thisNode, aPtexFilterSize).asFloat();
		
#ifdef _OPENMP
#pragma omp parallel for private(pix) schedule(dynamic) num_threads(thread_count)
#endif
		for (int i = 0; i < numFaces; ++i) {
			const Ptex::FaceInfo& fi = tex->getFaceInfo(i);
			const int ures = (int)(fi.res.u() * mult);
			const int vres = (int)(fi.res.v() * mult);
			const float ufres = (float)ures;
			const float vfres = (float)vres;
			
			const uint32_t triofs = i * 3;
			const TFLOAT3& a = vtx[tverts[triofs + 0]];
			const TFLOAT3& b = vtx[tverts[triofs + 1]];
			const TFLOAT3& c = vtx[tverts[triofs + 2]];
			
#ifdef _OPENMP
			VtxPrimitive* popos = opos + flut[i];
			ColPrimitive* pocol = ocol + flut[i];
#endif
			
			switch(displayMode)
			{
			case FaceRelative:
			{
				// Walk along the uvs and produce a point accordingly
				for (int u = 0; u < ures; ++u) {
					const float uf = u / ufres;
					const TFLOAT3 uvec = (b-a) * uf;
					for (int v = 0; v < vres; ++v) {
						const float vf = (1.0f - uf) * (v / vfres);
						filter->eval(&pix.x, 0, numChannels, i, uf, vf, fsize, fsize, fsize, fsize);
#ifdef _OPENMP
						*popos++ = a + uvec + (c-a)*vf;
						*pocol++ = pix;
#else
						*opos++ = a + uvec + (c-a)*vf;
						*ocol++ = pix;
#endif
					}// for each vsample
				}// for each usample
				break;
			}
			case FaceAbsolute:
			{
				// Walk along the first edge to generate the sampling raster
				// that was used to create the texture. Our samples hit the sample center
				// if no sampling multiplier is used
				const TFLOAT3 suv = (b - a) / ufres;	// vector spanning one sample in u
				//const TFLOAT3 suvhalf = suv / 2.0f;
				const TFLOAT3 svv = (c - a) / vfres;	// vector spanning one sample in v
				//const TFLOAT3 svvhalf = svv / 2.0f;
				
				int ur = ures;				// editable vresolution
				float uf, vf;				// uvs matching our sample positions
				
				// even samples
				TFLOAT3 p;
				for (int v = 0; v < vres; ++v, --ur) {
					TFLOAT3 ta = a + (svv * static_cast<float>(v));		// texel vertex a
					for (int u = 0; u < ur; ++u, ta += suv) {
						const TFLOAT3 tb = ta + suv;
						const TFLOAT3 tc = ta + svv;
						for (short is_odd = 0; is_odd < 2; is_odd += 1 + (u+1==ur)) {
							if (is_odd) {
								p = (tb + tc + (ta + (suv + svv))) / 3.0f;
							} else {
								p = (ta + tb + tc) / 3.0f;
							}
							uv_from_pos(a, b, c, p, uf, vf);
							filter->eval(&pix.x, 0, numChannels, i, uf, vf, fsize, fsize, fsize, fsize);
#ifdef _OPENMP
							*popos++ = p;
							*pocol++ = pix;
#else							
							*opos++ = p;
							*ocol++ = pix;
#endif
						}// for each even/odd texel
					}// for each vsample
				}// for each usample
				break;
			}
			default: break;
			}// end sampling mode switch
		}// for each face
		
		rval = true;
#undef TFLOAT3
		break;
	}
	}// switch displayMode
	

	// in omp mode, we access the data directly, hence the points don't't move
#ifndef _OPENMP
	assert(opos == buf.end(VertexArray));
	assert(ocol == buf.end(ColorArray));
#else
	// be very sure that the finalization code is run by the main thread
	#pragma omp master
#endif
	
	
	
	buf.end_access();
	
	// reset previous error - at this point we have samples
	if (m_error.length() && numTexels && rval) {
		m_error = MString();
	}
	
	// Update sample count, just for user information
	MPlug(thisNode, aOutNumSamples).setInt(static_cast<int>(numTexels));
	
	return rval;
}

MStatus PtexVisNode::compute(const MPlug& plug, MDataBlock& data)
{
	// in all cases, just set us clean - if we are not setup, we would be called
	// over and over again, lets limit this to just when something changes
	data.setClean(plug);
	if (plug == aNeedsCompute) {
		MDataHandle ncHandle = data.outputValue(aNeedsCompute);
		// we are successfull, even if there is no valid texture
		// We only use error codes if the sampling fails
		ncHandle.setInt(1);
		if (!assure_texture(data)) {
			return MS::kSuccess;
		}
		
		// just update our filter (it needs to be recreated as options are not adjustable
		// through the interface)
		const float fSize = data.inputValue(aPtexFilterSize).asFloat();
		const PtexFilter::FilterType fType = to_filter_type(data.inputValue(aPtexFilterType).asShort());
		
		PtexFilter::Options opts(fType, 0, fSize);
		PtexFilterPtr pfilter(PtexFilter::getFilter(m_ptex_texture.get(), opts));
		m_ptex_filter.swap(pfilter);
		
		// We cache the samples for faster drawing, and won't support live-drawing for now
		m_needs_cache_update = true;
		return MS::kSuccess;
	} else if (plug == aOutMetaDataKeys || plug == aOutNumChannels || plug == aOutNumFaces ||
	           plug == aOutAlphaChannel || plug == aOutHasEdits || plug == aOutHasMipMaps  ||
	           plug == aOutMeshType || plug == aOutDataType || 
	           plug == aOutUBorderMode || plug == aOutVBorderMode) {
		if (!assure_texture(data)) {
			return MS::kSuccess;
		}
		
		// Update all file information
		MFnStringArrayData arrayData(data.outputValue(aOutMetaDataKeys).data());
		MStringArray keys = arrayData.array();
		keys.clear();
		
		PtexTexture* tex = m_ptex_texture.get();
		assert(tex);
		
		PtexMetaData* mdata = tex->getMetaData();
		for (int i = 0; i < mdata->numKeys(); ++i) {
			const char* keyName = 0;
			Ptex::MetaDataType dt;
			mdata->getKey(i, keyName, dt	);
			keys.append(MString(keyName ? keyName : "unknown"));
		}
		
		data.outputValue(aOutNumChannels).asInt() = tex->numChannels();
		data.outputValue(aOutNumFaces).asInt() = tex->numFaces();
		data.outputValue(aOutHasEdits).asBool() = tex->hasEdits();
		data.outputValue(aOutHasMipMaps).asBool() = tex->hasMipMaps();
		data.outputValue(aOutAlphaChannel).asInt() = tex->alphaChannel();
		data.outputValue(aOutMeshType).asShort() = (short)tex->meshType();
		data.outputValue(aOutDataType).asShort() = (short)tex->dataType();
		data.outputValue(aOutUBorderMode).asShort() = (short)tex->uBorderMode();
		data.outputValue(aOutVBorderMode).asShort() = (short)tex->vBorderMode();
		
		// set all clean
		MObject* attrs[] = {&aOutMetaDataKeys, &aOutNumChannels, &aOutNumFaces, 
		                    &aOutAlphaChannel, &aOutHasEdits, &aOutHasMipMaps,
		                   &aOutMeshType, &aOutDataType, &aOutUBorderMode, &aOutVBorderMode};
		MObject** end = attrs + (sizeof(attrs) / sizeof(attrs[0]));
		for (MObject** i = attrs; i < end; ++i) {
			data.setClean(**i);
		}
		
		return MS::kSuccess;
	} else {
		return MS::kUnknownParameter;
	}
}

void PtexVisNode::draw(M3dView &view, const MDagPath &path, M3dView::DisplayStyle style, M3dView::DisplayStatus)
{
	// make sure we are uptodate
	MPlug(thisMObject(), aNeedsCompute).asInt();
	bool res = false;
	MGLFunctionTable* glf = 0;
	
	view.beginGL();
	if (m_error.length()) {
		view.drawText(MString("Error: ") + m_error, MPoint());
	}
		
	// start drawing
	MHardwareRenderer* renderer = MHardwareRenderer::theRenderer();
	if (!renderer) {
		m_error = "No hardware renderer";
		goto finish_drawing;
	}
	
	glf = renderer->glFunctionTable();
	if (!glf) {
		m_error = "No function table";
		goto finish_drawing;
	}
	
	// UPDATE SAMPLE BUFFERS
	////////////////////////
	if (m_needs_cache_update) {
		m_needs_cache_update = false;
		const DisplayCacheMode cache_mode = (DisplayCacheMode)MPlug(thisMObject(), aDisplayCacheMode).asShort();
		if (cache_mode == DCSystem) {
			m_gpubuf.resize(0);
			update_sample_buffer(m_sysbuf);
		} else {
			// always keep it uptodate
			m_gpubuf.set_glf(glf);
			m_sysbuf.resize(0);
			update_sample_buffer(m_gpubuf);
		}
	}
	
	
	glf->glPointSize(m_gl_point_size);
	if (m_gpubuf.is_valid()) {
		res = m_gpubuf.draw(glf);
	} else if (m_sysbuf.is_valid()) {
		res = m_sysbuf.draw(glf);
	} else if (m_error.length() == 0) {
		m_error = "Nothing to display";
	}
	
	if (res == false && m_error.length() == 0) {
		m_error = "Display of samples failed, GL error code: ";
		m_error += (int)glf->glGetError();
	} else if (res) {
		m_error.clear();
	}
	
finish_drawing:
	view.endGL();
}
