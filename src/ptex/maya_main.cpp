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

#include "util.h"
#include "visnode.h"

#include <maya/MFnPlugin.h>

#include <mayabaselib/base.h>
#include <Ptexture.h>

PtexCache* gCache = 0;

//! Initialize the plugin in maya
EXPORT MStatus initializePlugin(MObject obj)
{
	gCache = PtexCache::create();
	
	MFnPlugin plugin(obj, "Sebastian Thiel", "0.1");
	MStatus stat;
	
	stat = plugin.registerNode(PtexVisNode::typeName, PtexVisNode::typeId, 
								PtexVisNode::creator, PtexVisNode::initialize,
								MPxNode::kLocatorNode);
	if (stat.error()){
		stat.perror("register visualization node");
		return stat;
	}

	return stat;
}

//! Deinitialize the plugin from maya
EXPORT MStatus uninitializePlugin(MObject obj)
{
	MFnPlugin plugin(obj);
	MStatus stat;

	stat = plugin.deregisterNode(PtexVisNode::typeId);
	if (stat.error()){
		stat.perror("deregister PtexVisNode");
		return stat;
	}
	
	// Finally clear ptexture cache
	gCache->release();
	gCache = NULL;
	return stat;
}

