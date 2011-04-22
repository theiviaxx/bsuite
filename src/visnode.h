#ifndef PTEX_VISUALIZATION_NODE
#define PTEX_VISUALIZATION_NODE

#include <Ptexture.h>

#include <maya/MPxLocatorNode.h>
#include <maya/MGLdefinitions.h>
#include <maya/MFloatPoint.h>
#include <vector>


//! Structure with 3 floats and most general vector methods
struct Float3
{
	float x;
	float y;
	float z;
	
	Float3(float x=0.f, float y=0.f, float z=0.f)
	    : x(x)
	    , y(y)
	    , z(z)
	{}
	
	Float3(const MFloatPoint& r)
	    : x(r.x)
	    , y(r.y)
	    , z(r.z)
	{}
	
	inline Float3 operator - (const Float3& r) const {
		return Float3(x - r.x, y - r.y, z - r.z);
	}
	inline Float3 operator + (const Float3& r) const {
		return Float3(x + r.x, y + r.y, z + r.z);
	}
	inline Float3& operator += (const Float3& r) {
		x += r.x; 
		y += r.y; 
		z += r.z;
		return *this;
	}
	inline Float3 operator * (float v) const {
		return Float3(x * v, y * v, z * v);
	}
	inline Float3 operator / (float v) const {
		return Float3(x / v, y / v, z / v);
	}
};

//! Simple 4 float structure
struct Float4 : public Float3
{
	float w;
	
	Float4(float x=0.f, float y=0.f, float z=0.f, float w=0.f)
	    : Float3(x,y,z)
	    , w(w)
	{}
};

typedef PtexPtr<PtexFilter> PtexFilterPtr;
typedef PtexPtr<PtexTexture> PtexTexturePtr;
typedef std::vector<Float3>	Float3Vector;


//! Node helping to obtain information about ptextures. It can visualize them in the viewport as well
class PtexVisNode : public MPxLocatorNode
{
	public:
	
	enum DisplayMode
	{
		Texel = 0,					//!< Direct display
		FaceRelative = 1,			//!< display in face space, along uvs
		FaceAbsolute = 2,			//!< display in face space, along longest edge
	};
	
	public:
		PtexVisNode();
		virtual ~PtexVisNode();

		virtual MStatus compute(const MPlug&, MDataBlock&);
		virtual bool	setInternalValueInContext(const MPlug &plug, const MDataHandle &dataHandle, MDGContext &ctx);
		virtual void    postConstructor();
		virtual void	draw(M3dView &view, const MDagPath &path, M3dView::DisplayStyle style, M3dView::DisplayStatus);

		static  void*   creator();
		static  MStatus initialize();

		static const MTypeId typeId;				//!< binary file type id
		static const MString typeName;				//!< node type name

	protected:
		//! \return filter type enumeration based on an int (in a safe manner as we do not cast)
		static PtexFilter::FilterType to_filter_type(int type);
		
		//! Assure a filter is present.
		//! \return false if this is not the case, usually because the filename is invalid
		bool assure_texture(MDataBlock& data);
		
		//! release the current texture (if there is one). This releases the filter as well !
		void release_texture_and_filter();
		
		//! reset output information (e.g. once we have no valid texture)
		void reset_output_info(MDataBlock& data);
		
		//! release data taken up by our sample cache
		void release_cache();
		
		//! update our sample cache with changes. As a result, we will fill our 
		//! sample cache with local space positions and colors, ready to be 
		//! pushed through opengl.
		//! \return true on success
		//! \note changes error code on failure
		bool update_sample_buffer(MDataBlock& data);
		
	protected:
		// Input attributes
		static MObject aPtexFileName;			//!< path to ptex file to use
		static MObject aPtexFilterType;			//!< type of filter for evaluation
		static MObject aPtexFilterSize;			//!< desired uv filter size
		static MObject aInMesh;					//!< mesh to display the ptex information for
		static MObject aGlPointSize;			//!< size of a point when drawing
		static MObject aDisplayMode;			//!< defines the way we display samples
		static MObject aSampleMultiplier;		//!< Multiply amount of samples taken
		
		// output attributes
		static MObject aOutNumChannels;			//!< provide the number of channels in the file
		static MObject aOutNumFaces;			//!< amount of faces in the file
		static MObject aOutAlphaChannel;		//!< index of the channel providing alpha information
		static MObject aOutHasMipMaps;			//!< true if mipmaps are stored
		static MObject aOutHasEdits;			//!< true if edits are available
		static MObject aOutMetaDataKeys;		//!< string array of meta data keys
		static MObject aOutDataType;			//!< data type used in the channels
		static MObject aOutMeshType;			//!< mesh type used in file
		static MObject aOutUBorderMode;			//!< u border mode
		static MObject aOutVBorderMode;			//!< u border mode
		static MObject aOutNumSamples;			//!< number of samples we have taken
		static MObject aNeedsCompute;			//!< dummy output (for now) to check if we need to compute
		

	protected:
		uint32_t		m_ptex_num_channels;	//!< Amount of channels currently stored in the file
		PtexFilterPtr	m_ptex_filter;			//!< filter ptr to be used for ptex evaluation
		PtexTexturePtr	m_ptex_texture;			//!< texture pointer
		MString			m_error;				//!< error string
		
		Float3Vector	m_sample_pos;			//!< sample positions
		Float3Vector	m_sample_col;			//!< sample colors
		MGLfloat		m_gl_point_size;		//!< size of a point when drawing (cache)	
};

#endif
