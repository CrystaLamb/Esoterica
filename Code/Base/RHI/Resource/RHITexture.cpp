#include "RHITexture.h"
#include "../RHIDevice.h"
#include "Base/Logging/Log.h"

namespace EE::RHI
{
    RHITexture::~RHITexture()
    {
        if ( !m_viewCache.empty() )
        {
            EE_LOG_ERROR( "RHI", "RHITexture", "Did you forget to call ClearAllViews() before destroy the texture?" );
            EE_ASSERT( false );
        }
    }

    //-------------------------------------------------------------------------

    RHITextureView RHITexture::GetOrCreateView( RHIDeviceRef& pDevice, RHITextureViewCreateDesc const& desc )
    {
        auto iter = m_viewCache.find( desc );
        if ( iter != m_viewCache.end() )
        {
            return iter->second;
        }

        if ( !pDevice )
        {
            return {};
        }

        auto textureView = CreateView( pDevice, desc );
        if ( textureView.IsValid() )
        {
            m_viewCache.insert( { desc, textureView } );
            return textureView;
        }

        return {};
    }

    void RHITexture::ClearAllViews( RHIDeviceRef& pDevice )
    {
        for ( auto& view : m_viewCache )
        {
            DestroyView( pDevice, view.second );
        }
        m_viewCache.clear();
    }

    //-------------------------------------------------------------------------

	void RHITexture::Enqueue( DeferReleaseQueue& queue )
	{
        queue.m_deferReleaseTextures.enqueue( this );
	}

    void RHITexture::Release( RHIDeviceRef& pDevice )
    {
        pDevice->DestroyTexture( this );

    }
}