#pragma once
#if defined(EE_VULKAN)

#include "Base/Types/Arrays.h"
#include "Base/Types/List.h"
#include "Base/Types/HashMap.h"
#include "Base/RHI/Resource/RHITexture.h"
#include "Base/RHI/Resource/RHIPipelineState.h"
#include "Base/RHI/RHICommandBuffer.h"

#include <vulkan/vulkan_core.h>

namespace EE::RHI
{
    class RHIDevice;
    class RHIRenderPass;
    class RHIFramebuffer;
    class RHIPipelineState;

    class RHITexture;
}

namespace EE::Render
{
	namespace Backend
	{
        // Vulkan Pipeline Barrier Utility Types
        //-------------------------------------------------------------------------

        struct VkAccessInfo
        {
            /// Describes which stage in the pipeline this resource is used.
            VkPipelineStageFlags			m_stageMask;
            /// Describes which access mode in the pipeline this resource is used.
            VkAccessFlags					m_accessMask;
            /// Describes the image memory layout which image will be used if this resource is a image resource.
            VkImageLayout					m_imageLayout;
        };

        struct VkMemoryBarrierTransition
        {
            VkPipelineStageFlags				m_srcStage;
            VkPipelineStageFlags				m_dstStage;
            VkMemoryBarrier						m_barrier;
        };

        struct VkBufferBarrierTransition
        {
            VkPipelineStageFlags				m_srcStage;
            VkPipelineStageFlags				m_dstStage;
            VkBufferMemoryBarrier				m_barrier;
        };

        struct VkTextureBarrierTransition
        {
            VkPipelineStageFlags				m_srcStage;
            VkPipelineStageFlags				m_dstStage;
            VkImageMemoryBarrier				m_barrier;
        };

        struct VulkanDescriptorSetHash
        {
            uint32_t                                        m_set;
            TSpan<RHI::RHIPipelineBinding const> const&     m_bindings;

            size_t GetHash() const;

            friend bool operator==( VulkanDescriptorSetHash const& lhs, VulkanDescriptorSetHash const& rhs )
            {
                return eastl::tie( lhs.m_set, lhs.m_bindings )
                    == eastl::tie( rhs.m_set, rhs.m_bindings );
            }
        };

        //-------------------------------------------------------------------------

        class VulkanCommandBufferPool;
        struct VulkanCommonPipelineInfo;

		class VulkanCommandBuffer : public RHI::RHICommandBuffer
		{
            EE_RHI_OBJECT( Vulkan, RHICommandBuffer )

            friend class VulkanDevice;
            friend class VulkanCommandBufferPool;
            friend class VulkanCommandQueue;

        public:

            VulkanCommandBuffer()
                : RHICommandBuffer( RHI::ERHIType::Vulkan )
            {
                m_updatedDescriptorSets.clear();
            }
            virtual ~VulkanCommandBuffer() = default;

        public:

            // Synchronization
            //-------------------------------------------------------------------------

            //virtual RHI::RenderCommandSyncPoint SetSyncPoint( Render::PipelineStage stage ) override;
            //virtual void WaitSyncPoint( RHI::RenderCommandSyncPoint syncPoint, Render::PipelineStage stage ) override;

            // Render Commands
            //-------------------------------------------------------------------------

            virtual void Draw( uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, uint32_t firstInstance = 0 ) override;
            virtual void DrawIndexed( uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0 ) override;

            // Compute Commands
            //-------------------------------------------------------------------------

            virtual void Dispatch( uint32_t groupX, uint32_t groupY, uint32_t groupZ ) override;

            // Pipeline Barrier
            //-------------------------------------------------------------------------

            virtual bool BeginRenderPass( RHI::RHIRenderPassRef& pRhiRenderPass, RHI::RHIFramebufferRef& pFramebuffer, RHI::RenderArea const& renderArea, TSpan<RHI::RHITextureView const> textureViews ) override;
            virtual bool BeginRenderPassWithClearValue(
                RHI::RHIRenderPassRef& pRhiRenderPass, RHI::RHIFramebufferRef& pFramebuffer, RHI::RenderArea const& renderArea,
                TSpan<RHI::RHITextureView const> textureViews,
                RHI::RenderPassClearValue const& clearValue
            ) override;
            virtual void EndRenderPass() override;

            virtual void PipelineBarrier( 
                RHI::GlobalBarrier const* pGlobalBarriers,
                uint32_t bufferBarrierCount, RHI::BufferBarrier const* pBufferBarriers,
                uint32_t textureBarrierCount, RHI::TextureBarrier const* pTextureBarriers
            );

            // Resource Binding
            //-------------------------------------------------------------------------

            virtual void BindPipelineState( RHI::RHIPipelineRef& pRhiPipelineState ) override;
            virtual void BindDescriptorSetInPlace( uint32_t set, RHI::RHIPipelineRef const& pPipelineState, TSpan<RHI::RHIPipelineBinding const> const& bindings) override;

            virtual void BindVertexBuffer( uint32_t firstBinding, TSpan<RHI::RHIBufferRef const&> pVertexBuffers, uint32_t offset = 0 ) override;
            virtual void BindIndexBuffer( RHI::RHIBufferRef const& pIndexBuffer, uint32_t offset = 0 ) override;

            virtual void UpdateDescriptorSetBinding( uint32_t set, uint32_t binding, RHI::RHIPipelineRef const& pPipelineState, RHI::RHIPipelineBinding const& rhiBinding ) override;

            // State Settings
            //-------------------------------------------------------------------------

            virtual void ClearColor( Color color ) override;
            virtual void ClearDepthStencil( RHI::RHITextureRef& pTexture, RHI::TextureSubresourceRange range, RHI::ETextureLayout currentLayout, float depthValue, uint32_t stencil ) override;

            virtual void SetViewport( uint32_t width, uint32_t height, int32_t xOffset = 0, int32_t yOffset = 0 ) override;
            virtual void SetScissor( uint32_t width, uint32_t height, int32_t xOffset = 0, int32_t yOffset = 0 ) override;

            // Resource Copying
            //-------------------------------------------------------------------------

            virtual void CopyBufferToBuffer( RHI::RHIBufferRef& pSrcBuffer, RHI::RHIBufferRef& pDstBuffer ) override;
            virtual void CopyBufferToTexture( RHI::RHITextureRef& pDstTexture, RHI::RenderResourceBarrierState dstBarrier, TSpan<RHI::TextureSubresourceRangeUploadRef> const uploadDataRef ) override;

            //-------------------------------------------------------------------------

			inline VkCommandBuffer Raw() const { return m_pHandle; }

        private:

            // Vulkan pipeline barrier utility functions
            //-------------------------------------------------------------------------
            
            VkMemoryBarrierTransition GetMemoryBarrierTransition( RHI::GlobalBarrier const& globalBarrier );
            VkBufferBarrierTransition GetBufferBarrierTransition( RHI::BufferBarrier const& bufferBarrier );
            VkTextureBarrierTransition GetTextureBarrierTransition( RHI::TextureBarrier const& textureBarrier );

            // Vulkan descriptor binding helper functions
            //-------------------------------------------------------------------------

            VkWriteDescriptorSet WriteDescriptorSet(
                VkDescriptorSet set, uint32_t binding, RHI::SetDescriptorLayout const& setDescriptorLayout, RHI::RHIPipelineBinding const& rhiBinding,
                TSInlineList<VkDescriptorBufferInfo, 8>& bufferInfos, TSInlineList<VkDescriptorImageInfo, 8>& textureInfos, TInlineVector<uint32_t, 4> dynOffsets
            );

            TVector<VkWriteDescriptorSet> WriteDescriptorSets(
                VkDescriptorSet set, RHI::SetDescriptorLayout const& setDescriptorLayout, TSpan<RHI::RHIPipelineBinding const> const& bindings,
                TSInlineList<VkDescriptorBufferInfo, 8>& bufferInfos, TSInlineList<VkDescriptorImageInfo, 8>& textureInfos, TInlineVector<uint32_t, 4> dynOffsets
            );

            inline void MarkAsUpdated( size_t setHashValue, VkDescriptorSet vkSet );
            inline bool IsInPlaceDescriptorSetUpdated( size_t setHashValue );
            
            VkDescriptorSet CreateOrFindInPlaceUpdatedDescriptorSet( VulkanDescriptorSetHash const& hash, VulkanCommonPipelineInfo const& vkPipelineInfo );

            //-------------------------------------------------------------------------

            // clean all old states and prepare for new command enqueue.
            // Usually called after its command pool is reset.
            void CleanUp();

		private:

            static TInlineVector<VkMemoryBarrier, 1>                    m_sGlobalBarriers;
            static TInlineVector<VkBufferMemoryBarrier, 32>             m_sBufferBarriers;
            static TInlineVector<VkImageMemoryBarrier, 32>              m_sTextureBarriers;
			
            RHI::RHIDeviceRef                                           m_pDevice;

            VkCommandBuffer					                            m_pHandle = nullptr;

            // Only safe to cache hash here
            THashMap<size_t, VkDescriptorSet>                           m_updatedDescriptorSets;

            TVector<TPair<VkEvent, VkPipelineStageFlags>>               m_syncPoints;
        };
    }
}

// Hash
//-------------------------------------------------------------------------

namespace eastl
{
    template <>
    struct hash<EE::Render::Backend::VulkanDescriptorSetHash>
    {
        EE_FORCE_INLINE eastl_size_t operator()( EE::Render::Backend::VulkanDescriptorSetHash const& descriptorSetHash ) const noexcept
        {
            return descriptorSetHash.GetHash();
        }
    };
}

#endif