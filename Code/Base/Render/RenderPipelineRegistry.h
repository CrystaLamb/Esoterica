#pragma once

#include "Base/Systems.h"
#include "Base/Resource/ResourcePtr.h"
#include "Base/Render/RenderShader.h"
#include "Base/Render/RenderPipeline.h"
#include "Base/Render/RenderPipelineState.h"
#include "Base/Types/IDVector.h"
#include "Base/RHI/Resource/RHIResourceCreationCommons.h"

#include <numeric>

//-------------------------------------------------------------------------

namespace EE
{
	class TaskSystem;
    namespace Resource { class ResourceSystem; class ResourceServer; }
}

namespace EE::RHI
{
    class RHIDevice;
    class RHIPipelineState;
}

//-------------------------------------------------------------------------

namespace EE::Render
{
	class PipelineHandle
	{
		friend class PipelineRegistry;

	public:

        PipelineHandle() = default;

		inline bool IsValid() const
		{
			return m_ID != 0 && m_ID != std::numeric_limits<uint32_t>::max();
		}

		inline uint32_t RawValue() const
		{
			return m_ID;
		}

        inline PipelineType GetPipelineType() const
        {
            return m_type;
        }

	public:

		inline friend bool operator<( PipelineHandle const& lhs, PipelineHandle const& rhs )
		{
			return lhs.m_ID < rhs.m_ID;
		}

		inline friend bool operator==( PipelineHandle const& lhs, PipelineHandle const& rhs )
		{
			return lhs.m_ID == rhs.m_ID;
		}

	private:

		PipelineHandle( PipelineType type, uint32_t id );

	private:

		// this id will be used as ResourceRequestID, so it can not be 0.
		uint32_t				m_ID = std::numeric_limits<uint32_t>::max();
		PipelineType			m_type = PipelineType::Raster;
	};
}

//-------------------------------------------------------------------------

namespace eastl
{
	template <>
	struct hash<EE::Render::PipelineHandle>
	{
		EE_FORCE_INLINE eastl_size_t operator()( EE::Render::PipelineHandle const& handle ) const
		{
			return static_cast<eastl_size_t>( handle.RawValue() );
		}
	};
}

//-------------------------------------------------------------------------

namespace EE::Render
{
	class EE_BASE_API RasterPipelineEntry
	{
	public:

		inline PipelineHandle GetID() const { return m_handle; }
		inline bool IsReadyToCreatePipelineLayout() const { return m_vertexShader.IsLoaded() && m_pixelShader.IsLoaded(); }
        // A pipeline entry is visible means that it is ready to be used outside.
        // If a pipeline is NOT fully ready, it is invisible to outside, as if it doesn't exist.
        // This function should only be called with PipelineRegistry.
        inline bool IsVisible() const { return m_pPipelineState != nullptr; }

	public:

		// TODO: support more shader type
		TResourcePtr<VertexShader>			    m_vertexShader;
		TResourcePtr<PixelShader>			    m_pixelShader;

        RHI::RHIRasterPipelineStateCreateDesc   m_desc;

        RHI::RHIPipelineState*                  m_pPipelineState = nullptr;
        PipelineHandle                          m_handle;
	};

	class EE_BASE_API ComputePipelineEntry
	{
    public:

        inline PipelineHandle GetID() const { return m_handle; }
        inline bool IsReadyToCreatePipelineLayout() const { return m_computeShader.IsLoaded(); }
        // A pipeline entry is visible means that it is ready to be used outside.
        // If a pipeline is NOT fully ready, it is invisible to outside, as if it doesn't exist.
        // This function should only be called with PipelineRegistry.
        inline bool IsVisible() const { return m_pPipelineState != nullptr; }

    public:

        TResourcePtr<ComputeShader>			    m_computeShader;

        RHI::RHIComputePipelineStateCreateDesc  m_desc;

        RHI::RHIPipelineState*                  m_pPipelineState = nullptr;
		PipelineHandle					        m_handle;
	};

	//-------------------------------------------------------------------------

	class EE_BASE_API PipelineRegistry
	{

	public:

		PipelineRegistry() = default;
        ~PipelineRegistry();

		PipelineRegistry( PipelineRegistry const& ) = delete;
		PipelineRegistry& operator=( PipelineRegistry const& ) = delete;

		PipelineRegistry( PipelineRegistry&& ) = delete;
		PipelineRegistry& operator=( PipelineRegistry&& ) = delete;

	public:

        // Initialize pipeline registry with remote compilation mode.
        // If resource system doesn't exists, please provide a upper layer class which derive from this PipelineRegistry to get more functionality.
		bool Initialize( Resource::ResourceSystem* pResourceSystem );
		void Shutdown();

		[[nodiscard]] inline PipelineHandle RegisterRasterPipeline( RHI::RHIRasterPipelineStateCreateDesc const& rasterPipelineDesc );
		[[nodiscard]] inline PipelineHandle RegisterComputePipeline( RHI::RHIComputePipelineStateCreateDesc const& computePipelineDesc );

        bool IsPipelineReady( PipelineHandle const& pipelineHandle ) const;
        inline bool IsBusy() const
        {
            return !m_waitToLoadRasterPipelines.empty()
                || !m_waitToRegisteredRasterPipelines.empty()
                || !m_retryRasterPipelineCaches.empty()
                || !m_waitToLoadComputePipelines.empty()
                || !m_waitToRegisteredComputePipelines.empty()
                || !m_retryComputePipelineCaches.empty();
        }

        RHI::RHIPipelineState* TryGetRHIPipelineHandle( PipelineHandle const& pipelineHandle ) const;

        // Update pipeline registry.
        // This function will block until all pipeline loading is completed.
        bool UpdateBlock( RHI::RHIDevice* pDevice );
        
        void DestroyAllPipelineStates( RHI::RHIDevice* pDevice );

	private:

        // Update pipeline loading status
        void UpdateLoadedPipelineShaders();
        // Registered loaded pipelines to create actual RHI resources
        bool TryCreatePipelineForLoadedPipelineShaders( RHI::RHIDevice* pDevice );

        inline bool AreAllRequestedPipelineLoaded() const;

        // Create RHI raster pipeline state for certain RasterPipelineEntry.
        // This function can't have any side-effects, since it may be call for the same entry multiple time.
        bool TryCreateRHIRasterPipelineStateForEntry( TSharedPtr<RasterPipelineEntry>& rasterEntry, RHI::RHIDevice* pDevice );
        bool TryCreateRHIComputePipelineStateForEntry( TSharedPtr<ComputePipelineEntry>& computeEntry, RHI::RHIDevice* pDevice );

        // Unload all pipeline shaders inside all registries.
		void UnloadAllPipelineShaders();

	private:

		bool															        m_isInitialized = false;

		Resource::ResourceSystem*										        m_pResourceSystem = nullptr;

        // TODO: may be extract to single pipeline cache class
		mutable TIDVector<PipelineHandle, TSharedPtr<RasterPipelineEntry>>      m_rasterPipelineStatesCache;
		THashMap<RHI::RHIRasterPipelineStateCreateDesc, PipelineHandle>         m_rasterPipelineHandlesCache;

        mutable TIDVector<PipelineHandle, TSharedPtr<ComputePipelineEntry>>     m_computePipelineStatesCache;
        THashMap<RHI::RHIComputePipelineStateCreateDesc, PipelineHandle>        m_computePipelineHandlesCache;

        // TODO: state machine update pattern
        //TVector<TSharedPtr<RasterPipelineEntry>>						        m_waitToSubmitRasterPipelines;
        TVector<TSharedPtr<RasterPipelineEntry>>						        m_waitToLoadRasterPipelines;
        TVector<TSharedPtr<RasterPipelineEntry>>						        m_waitToRegisteredRasterPipelines;
        TVector<TSharedPtr<RasterPipelineEntry>>                                m_retryRasterPipelineCaches;

        //TVector<TSharedPtr<ComputePipelineEntry>>						        m_waitToSubmitComputePipelines;
        TVector<TSharedPtr<ComputePipelineEntry>>						        m_waitToLoadComputePipelines;
        TVector<TSharedPtr<ComputePipelineEntry>>						        m_waitToRegisteredComputePipelines;
        TVector<TSharedPtr<ComputePipelineEntry>>                               m_retryComputePipelineCaches;
	};
}