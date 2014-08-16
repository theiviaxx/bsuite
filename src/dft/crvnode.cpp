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

#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPlug.h>
#include <maya/MFnMesh.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFloatPointArray.h>
#include <maya/MRampAttribute.h>


#include "mayabaselib/base.h"
#include "util.h"
#include "crvnode.h"

#include "assert.h"

#ifdef _OPENMP
	#include <omp.h>
#endif


//********************************************************************
//**	Class Implementation
//********************************************************************
/////////////////////////////////////////////////////////////////////

const MTypeId MeshCurvatureNode::typeId(0x00108bf9);
const MString MeshCurvatureNode::typeName("MeshCurvatureNode");


// Attributes
MObject MeshCurvatureNode::aCurveMap;
MObject MeshCurvatureNode::aInMesh;
MObject MeshCurvatureNode::aMapName;

MObject MeshCurvatureNode::aOutMesh;


MeshCurvatureNode::MeshCurvatureNode()
{}

MeshCurvatureNode::~MeshCurvatureNode()
{}

void MeshCurvatureNode::postConstructor()
{
	setMPSafe(true);
}

// DESCRIPTION:
// creates an instance of the node
void* MeshCurvatureNode::creator()
{
	return new MeshCurvatureNode();
}

// DESCRIPTION:
//
MStatus MeshCurvatureNode::initialize()
{
	MStatus status;

	MFnTypedAttribute fnType;
	
	aInMesh = fnType.create("inMesh", "im", MFnData::kMesh, &status);
	CHECK_MSTATUS(status);
	aOutMesh = fnType.create("outMesh", "om", MFnData::kMesh, &status);
	CHECK_MSTATUS(status);

	MObject vtxMapDefault(MFnStringData().create("curvatureMap"));
	aMapName = fnType.create("vertexColorMapName", "vcmn", MFnData::kString, vtxMapDefault, &status);
	CHECK_MSTATUS(status);

	aCurveMap = MRampAttribute::createColorRamp("curvatureMap", "cm", &status);
	CHECK_MSTATUS(status);

	// Add attributes
	/////////////////
	CHECK_MSTATUS(addAttribute(aInMesh));
	CHECK_MSTATUS(addAttribute(aOutMesh));
	CHECK_MSTATUS(addAttribute(aMapName));
	CHECK_MSTATUS(addAttribute(aCurveMap));

	// All input affect the output color
	CHECK_MSTATUS(attributeAffects(aInMesh, aOutMesh));
	CHECK_MSTATUS(attributeAffects(aMapName, aOutMesh));
	CHECK_MSTATUS(attributeAffects(aCurveMap, aOutMesh));

	return MS::kSuccess;
}

MStatus attachCurvatureToMesh(MFnMesh& mesh, const MString& ctxMapName)
{
	MStatus stat;


	//

	return stat;
}


MStatus MeshCurvatureNode::compute(const MPlug& plug, MDataBlock& data)
{
	data.setClean(plug);
	MStatus stat;
	if (plug == aOutMesh) {
		// Copy data, so that we can work on the out mesh
		stat = data.outputValue(aOutMesh).copy(data.inputValue(aInMesh));
		CHECK_MSTATUS(stat);

		MFnMesh mesh(data.inputValue(aOutMesh).asMesh(), &stat);
		CHECK_MSTATUS(stat);

		attachCurvatureToMesh(mesh);

	} else {
		return MS::kUnknownParameter;
	}

}

