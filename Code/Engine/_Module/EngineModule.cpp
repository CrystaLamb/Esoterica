#include "EngineModule.h"
#include "Engine/Entity/EntityLog.h"
#include "Engine/Navmesh/NavPower.h"
#include "Engine/Physics/Physics.h"
#include "Base/Resource/ResourceProviders/NetworkResourceProvider.h"
#include "Base/Resource/ResourceProviders/PackagedResourceProvider.h"
#include "Base/Network/NetworkSystem.h"

//-------------------------------------------------------------------------

#ifdef _WIN32
#include "Base/Platform/PlatformUtils_Win32.h"
#endif

//-------------------------------------------------------------------------

#include "Base/Render/RenderAPI.h"
#include "Base/RenderGraph/RenderGraphResource.h"
#include "Base/RHI/RHIDevice.h"
#include "Base/RHI/Resource/RHIRenderPass.h"

namespace EE
{
    namespace
    {
        constexpr static char const* const g_physicsMaterialDatabaseResourceID = "data://Physics/PhysicsMaterials.pmdb";
    }

    //-------------------------------------------------------------------------

    #if EE_DEVELOPMENT_TOOLS
    bool EnsureResourceServerIsRunning( FileSystem::Path const& resourceServerExecutablePath )
    {
        #if _WIN32
        bool shouldStartResourceServer = false;

        // If the resource server is not running then start it
        String const resourceServerExecutableName = resourceServerExecutablePath.GetFilename();
        uint32_t const resourceServerProcessID = Platform::Win32::GetProcessID( resourceServerExecutableName.c_str() );
        shouldStartResourceServer = ( resourceServerProcessID == 0 );

        // Ensure we are running the correct build of the resource server
        if ( !shouldStartResourceServer )
        {
            String const resourceServerPath = Platform::Win32::GetProcessPath( resourceServerProcessID );
            if ( !resourceServerPath.empty() )
            {
                FileSystem::Path const resourceServerProcessPath = FileSystem::Path( resourceServerPath ).GetParentDirectory();
                FileSystem::Path const applicationDirectoryPath = FileSystem::Path( Platform::Win32::GetCurrentModulePath() ).GetParentDirectory();

                if ( resourceServerProcessPath != applicationDirectoryPath )
                {
                    Platform::Win32::KillProcess( resourceServerProcessID );
                    shouldStartResourceServer = true;
                }
            }
            else
            {
                return false;
            }
        }

        // Try to start the resource server
        if ( shouldStartResourceServer )
        {
            FileSystem::Path const applicationDirectoryPath = FileSystem::Path( Platform::Win32::GetCurrentModulePath() ).GetParentDirectory();
            return Platform::Win32::StartProcess( resourceServerExecutablePath.c_str() ) != 0;
        }

        return true;
        #else
        return false;
        #endif
    }
    #endif

    //-------------------------------------------------------------------------

    void EngineModule::GetListOfAllRequiredModuleResources( TVector<ResourceID>& outResourceIDs )
    {
        outResourceIDs.emplace_back( ResourceID( g_physicsMaterialDatabaseResourceID ) );
    }

    //-------------------------------------------------------------------------

    bool EngineModule::InitializeCoreSystems( IniFile const& iniFile )
    {
        #if EE_DEVELOPMENT_TOOLS
        EntityModel::InitializeLogQueue();
        #endif

        if ( !Network::NetworkSystem::Initialize() )
        {
            EE_LOG_ERROR( "Render", nullptr, "Failed to initialize network system" );
            return false;
        }

        // Create and initialize resource provider
        //-------------------------------------------------------------------------

        Resource::ResourceSettings settings;
        if ( !settings.ReadSettings( iniFile ) )
        {
            EE_LOG_ERROR( "Resource Provider", nullptr, "Failed to read resource settings from ini file!" );
            return false;
        }

        #if EE_DEVELOPMENT_TOOLS
        {
            if ( !EnsureResourceServerIsRunning( settings.m_resourceServerExecutablePath ) )
            {
                EE_LOG_ERROR( "Resource Provider", nullptr, "Couldn't start resource server (%s)!", settings.m_resourceServerExecutablePath.c_str() );
                return false;
            }

            m_pResourceProvider = EE::New<Resource::NetworkResourceProvider>( settings );
        }
        #else
        {
            m_pResourceProvider = EE::New<Resource::PackagedResourceProvider>( settings );
        }
        #endif

        if ( m_pResourceProvider == nullptr )
        {
            EE_LOG_ERROR( "Resource", nullptr, "Failed to create resource provider" );
            return false;
        }

        if ( !m_pResourceProvider->Initialize() )
        {
            EE_LOG_ERROR( "Resource", nullptr, "Failed to intialize resource provider" );
            EE::Delete( m_pResourceProvider );
            return false;
        }

        // Initialize core systems
        //-------------------------------------------------------------------------

        m_taskSystem.Initialize();
        m_resourceSystem.Initialize( m_pResourceProvider );
        m_inputSystem.Initialize();
        Physics::Core::Initialize();
        m_physicsMaterialRegistry.Initialize();

        #if EE_ENABLE_NAVPOWER
        Navmesh::NavPower::Initialize();
        #endif

        m_pRenderDevice = EE::New<Render::RenderDevice>();
        if ( !m_pRenderDevice->Initialize( iniFile ) )
        {
            EE_LOG_ERROR( "Render", nullptr, "Failed to create render device" );
            EE::Delete( m_pRenderDevice );
            return false;
        }

        #if EE_DEVELOPMENT_TOOLS
        m_imguiSystem.Initialize( m_pRenderDevice, &m_inputSystem, true );
        #endif

        // Initialize and register renderers
        //-------------------------------------------------------------------------

        if ( m_worldRenderer.Initialize( m_pRenderDevice ) )
        {
            m_rendererRegistry.RegisterRenderer( &m_worldRenderer );
        }
        else
        {
            EE_LOG_ERROR( "Render", nullptr, "Failed to initialize world renderer" );
            return false;
        }

        #if EE_DEVELOPMENT_TOOLS
        if ( m_debugRenderer.Initialize( m_pRenderDevice ) )
        {
            m_rendererRegistry.RegisterRenderer( &m_debugRenderer );
        }
        else
        {
            EE_LOG_ERROR( "Render", nullptr, "Failed to initialize debug renderer" );
            return false;
        }

        if ( m_imguiRenderer.Initialize( m_pRenderDevice ) )
        {
            m_rendererRegistry.RegisterRenderer( &m_imguiRenderer );
        }
        else
        {
            EE_LOG_ERROR( "Render", nullptr, "Failed to initialize imgui renderer" );
            return false;
        }

        if ( m_physicsRenderer.Initialize( m_pRenderDevice ) )
        {
            m_rendererRegistry.RegisterRenderer( &m_physicsRenderer );
        }
        else
        {
            EE_LOG_ERROR( "Render", nullptr, "Failed to initialize physics renderer" );
            return false;
        }
        #endif

        return true;
    }

    void EngineModule::ShutdownCoreSystems()
    {
        EE_ASSERT( !m_moduleInitialized );

        bool const coreSystemsInitialized = m_pRenderDevice != nullptr;

        // Unregister and shutdown renderers
        //-------------------------------------------------------------------------

        if ( m_pRenderDevice != nullptr )
        {
            #if EE_DEVELOPMENT_TOOLS
            if ( m_physicsRenderer.IsInitialized() )
            {
                m_rendererRegistry.UnregisterRenderer( &m_physicsRenderer );
            }
            m_physicsRenderer.Shutdown();

            if ( m_imguiRenderer.IsInitialized() )
            {
                m_rendererRegistry.UnregisterRenderer( &m_imguiRenderer );
            }
            m_imguiRenderer.Shutdown();

            if ( m_debugRenderer.IsInitialized() )
            {
                m_rendererRegistry.UnregisterRenderer( &m_debugRenderer );
            }
            m_debugRenderer.Shutdown();
            #endif

            if ( m_worldRenderer.IsInitialized() )
            {
                m_rendererRegistry.UnregisterRenderer( &m_worldRenderer );
            }
            m_worldRenderer.Shutdown();
        }

        // Shutdown core systems
        //-------------------------------------------------------------------------

        if ( coreSystemsInitialized )
        {
            #if EE_DEVELOPMENT_TOOLS
            m_imguiSystem.Shutdown();
            #endif

            #if EE_ENABLE_NAVPOWER
            Navmesh::NavPower::Shutdown();
            #endif

            m_physicsMaterialRegistry.Shutdown();
            Physics::Core::Shutdown();
            m_inputSystem.Shutdown();
            m_resourceSystem.Shutdown();
            m_taskSystem.Shutdown();
        }

        // Destroy render device and resource provider
        //-------------------------------------------------------------------------

        if ( m_pRenderDevice != nullptr )
        {
            m_pRenderDevice->Shutdown();
            EE::Delete( m_pRenderDevice );
        }

        if ( m_pResourceProvider != nullptr )
        {
            m_pResourceProvider->Shutdown();
            EE::Delete( m_pResourceProvider );
        }

        Network::NetworkSystem::Shutdown();

        #if EE_DEVELOPMENT_TOOLS
        EntityModel::ShutdownLogQueue();
        #endif
    }

    //-------------------------------------------------------------------------

    bool EngineModule::InitializeModule()
    {
        EE_ASSERT( m_pRenderDevice != nullptr );

        // Register systems
        //-------------------------------------------------------------------------

        m_systemRegistry.RegisterSystem( &m_typeRegistry );
        m_systemRegistry.RegisterSystem( &m_taskSystem );
        m_systemRegistry.RegisterSystem( &m_resourceSystem );
        m_systemRegistry.RegisterSystem( &m_inputSystem );
        m_systemRegistry.RegisterSystem( &m_entityWorldManager );
        m_systemRegistry.RegisterSystem( &m_rendererRegistry );
        m_systemRegistry.RegisterSystem( &m_physicsMaterialRegistry );

        // Register resource loaders
        //-------------------------------------------------------------------------

        m_entityCollectionLoader.SetTypeRegistryPtr( &m_typeRegistry );
        m_resourceSystem.RegisterResourceLoader( &m_entityCollectionLoader );

        //-------------------------------------------------------------------------

        m_renderMeshLoader.SetRenderDevicePtr( m_pRenderDevice );
        m_shaderLoader.SetRenderDevicePtr( m_pRenderDevice );
        m_textureLoader.SetRenderDevicePtr( m_pRenderDevice );

        m_resourceSystem.RegisterResourceLoader( &m_renderMeshLoader );
        m_resourceSystem.RegisterResourceLoader( &m_shaderLoader );
        m_resourceSystem.RegisterResourceLoader( &m_textureLoader );
        m_resourceSystem.RegisterResourceLoader( &m_materialLoader );

        //-------------------------------------------------------------------------

        m_animationClipLoader.SetTypeRegistryPtr( &m_typeRegistry );
        m_graphLoader.SetTypeRegistryPtr( &m_typeRegistry );

        m_resourceSystem.RegisterResourceLoader( &m_skeletonLoader );
        m_resourceSystem.RegisterResourceLoader( &m_animationClipLoader );
        m_resourceSystem.RegisterResourceLoader( &m_graphLoader );

        //-------------------------------------------------------------------------

        m_physicsMaterialLoader.SetMaterialRegistryPtr( &m_physicsMaterialRegistry );

        m_resourceSystem.RegisterResourceLoader( &m_physicsCollisionMeshLoader );
        m_resourceSystem.RegisterResourceLoader( &m_physicsMaterialLoader );
        m_resourceSystem.RegisterResourceLoader( &m_physicsRagdollLoader );

        //-------------------------------------------------------------------------

        m_resourceSystem.RegisterResourceLoader( &m_navmeshLoader );

        //-------------------------------------------------------------------------

        m_renderPipelineRegistry.Initialize( &m_resourceSystem );
        m_renderGraph.AttachToPipelineRegistry( m_renderPipelineRegistry );
        RHI::RHIDevice* pDevice = m_pRenderDevice->GetRHIDevice();

        //pDevice->BeginFrame();
        //m_renderGraph.AllocateCommandContext( pDevice );

        auto bufferDesc = RG::BufferDesc::NewSize( 512 );
        auto handle0 = m_renderGraph.CreateResource( bufferDesc );
        EE_ASSERT( handle0.GetDesc().m_desc.m_desireSize == 512 );

        bufferDesc.m_desc.m_desireSize = 256;
        auto handle1 = m_renderGraph.CreateResource( bufferDesc );
        EE_ASSERT( handle1.GetDesc().m_desc.m_desireSize == 256 );

        auto textureDesc = RG::TextureDesc::New2D( 512, 512, RHI::EPixelFormat::BGRA8Unorm );
        auto handle2 = m_renderGraph.CreateResource( textureDesc );

        {
            auto node = m_renderGraph.AddNode( "Clear Color RT" );
            auto handle0_ref = node.CommonRead( handle0, RHI::RenderResourceBarrierState::ComputeShaderReadOther );
            auto handle1_ref = node.CommonRead( handle1, RHI::RenderResourceBarrierState::VertexBuffer );

            RHI::RHIRenderPassCreateDesc renderPassCreateDesc = {};
            renderPassCreateDesc.m_colorAttachments.push_back( RHI::RHIRenderPassAttachmentDesc::TrivialColor( RHI::EPixelFormat::BGRA8Unorm ) );
            m_pImguiRenderPass = m_pRenderDevice->GetRHIDevice()->CreateRenderPass( renderPassCreateDesc );

            auto pipelineDesc = RHI::RHIRasterPipelineStateCreateDesc{};
            pipelineDesc.AddShader( RHI::RHIPipelineShader( ResourcePath( "data://shaders/imgui/imgui.vsdr" ) ));
            pipelineDesc.AddShader( RHI::RHIPipelineShader( ResourcePath( "data://shaders/imgui/imgui.psdr" ) ));
            pipelineDesc.SetRasterizerState( RHI::RHIPipelineRasterizerState::NoCulling() );
            pipelineDesc.SetBlendState( RHI::RHIPipelineBlendState::ColorAdditiveAlpha() );
            pipelineDesc.SetRenderPass( m_pImguiRenderPass );
            pipelineDesc.DepthTest( false );
            pipelineDesc.DepthWrite( false );
            node.RegisterRasterPipeline( std::move( pipelineDesc ) );

            EE_ASSERT( handle0_ref.GetDesc().m_desc.m_desireSize == 512 );
            EE_ASSERT( handle1_ref.GetDesc().m_desc.m_desireSize == 256 );
        }

        //{
        //    auto node = m_renderGraph.AddNode( "Draw Shadow" );
        //    auto handle1_ref = node.RasterRead( handle1, RHI::RenderResourceBarrierState::ColorAttachmentRead );
        //    auto handle2_ref = node.CommonWrite( handle2, RHI::RenderResourceBarrierState::ColorAttachmentWrite );

        //    EE_ASSERT( handle2_ref.GetDesc().m_desc.m_width == 512 );
        //    EE_ASSERT( handle2_ref.GetDesc().m_desc.m_height == 512 );
        //    EE_ASSERT( handle2_ref.GetDesc().m_desc.m_format == RHI::EPixelFormat::BGRA8Unorm );
        //}

        //m_renderGraph.AddNode( "Draw Opache" );
        //m_renderGraph.AddNode( "Draw Transparent" );
        //m_renderGraph.AddNode( "Post Processing" );
        //m_renderGraph.AddNode( "Draw Debug" );

        m_renderGraph.LogGraphNodes();

        // force all loading and preparing ready at first frame
        while ( m_renderPipelineRegistry.IsBusy() )
        {
            m_renderPipelineRegistry.Update();
            m_renderPipelineRegistry.UpdatePipelines( pDevice );

            while ( m_resourceSystem.IsBusy() )
            {
                Network::NetworkSystem::Update();
                m_resourceSystem.Update( true );
            }
        }

        m_renderGraph.Compile( pDevice );
        //m_renderGraph.Execute();
        //m_renderGraph.Present();

        //m_renderGraph.FlushCommandContext();
        pDevice->EndFrame();

        //-------------------------------------------------------------------------

        m_moduleInitialized = true;

        return true;
    }

    void EngineModule::ShutdownModule()
    {
        EE_ASSERT( m_pRenderDevice != nullptr );

        //-------------------------------------------------------------------------

        m_pRenderDevice->GetRHIDevice()->DestroyRenderPass( m_pImguiRenderPass );
        m_renderPipelineRegistry.DestroyAllPipelineState( m_pRenderDevice->GetRHIDevice() );
        m_pImguiRenderPass = nullptr;

        m_renderGraph.Retire();
        m_renderGraph.DestroyAllResources( m_pRenderDevice->GetRHIDevice() );
        m_renderPipelineRegistry.Shutdown();

        // Unregister resource loaders
        //-------------------------------------------------------------------------

        m_resourceSystem.UnregisterResourceLoader( &m_navmeshLoader );

        //-------------------------------------------------------------------------

        m_resourceSystem.UnregisterResourceLoader( &m_physicsRagdollLoader );
        m_resourceSystem.UnregisterResourceLoader( &m_physicsMaterialLoader );
        m_resourceSystem.UnregisterResourceLoader( &m_physicsCollisionMeshLoader );

        m_physicsMaterialLoader.ClearMaterialRegistryPtr();
        m_physicsMaterialRegistry.Shutdown();

        //-------------------------------------------------------------------------

        m_resourceSystem.UnregisterResourceLoader( &m_animationClipLoader );
        m_resourceSystem.UnregisterResourceLoader( &m_graphLoader );
        m_resourceSystem.UnregisterResourceLoader( &m_skeletonLoader );

        m_animationClipLoader.ClearTypeRegistryPtr();
        m_graphLoader.ClearTypeRegistryPtr();

        //-------------------------------------------------------------------------

        m_resourceSystem.UnregisterResourceLoader( &m_renderMeshLoader );
        m_resourceSystem.UnregisterResourceLoader( &m_shaderLoader );
        m_resourceSystem.UnregisterResourceLoader( &m_textureLoader );
        m_resourceSystem.UnregisterResourceLoader( &m_materialLoader );

        m_renderMeshLoader.ClearRenderDevicePtr();
        m_shaderLoader.ClearRenderDevicePtr();
        m_textureLoader.ClearRenderDevice();

        //-------------------------------------------------------------------------

        m_resourceSystem.UnregisterResourceLoader( &m_entityCollectionLoader );
        m_entityCollectionLoader.ClearTypeRegistryPtr();

        // Unregister systems
        //-------------------------------------------------------------------------

        m_systemRegistry.UnregisterSystem( &m_physicsMaterialRegistry );
        m_systemRegistry.UnregisterSystem( &m_rendererRegistry );
        m_systemRegistry.UnregisterSystem( &m_entityWorldManager );
        m_systemRegistry.UnregisterSystem( &m_inputSystem );
        m_systemRegistry.UnregisterSystem( &m_resourceSystem );
        m_systemRegistry.UnregisterSystem( &m_taskSystem );
        m_systemRegistry.UnregisterSystem( &m_typeRegistry );

        //-------------------------------------------------------------------------

        m_moduleInitialized = false;
    }

    //-------------------------------------------------------------------------

    void EngineModule::LoadModuleResources( Resource::ResourceSystem& resourceSystem )
    {
        m_physicsMaterialDB = ResourceID( g_physicsMaterialDatabaseResourceID );
        EE_ASSERT( m_physicsMaterialDB.IsSet() );
        resourceSystem.LoadResource( m_physicsMaterialDB );
    }

    bool EngineModule::VerifyModuleResourceLoadingComplete()
    {
        return m_physicsMaterialDB.IsLoaded() && m_physicsMaterialDB->IsValid();
    }

    void EngineModule::UnloadModuleResources( Resource::ResourceSystem& resourceSystem )
    {
        resourceSystem.UnloadResource( m_physicsMaterialDB );
    }
}