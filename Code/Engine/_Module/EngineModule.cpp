#include "EngineModule.h"
#include "Engine/ModuleContext.h"
#include "Engine/Console/Console.h"
#include "Engine/Entity/EntityLog.h"
#include "Engine/Navmesh/NavPower.h"
#include "Engine/Physics/Physics.h"
#include "Base/Resource/ResourceSystem.h"
#include "Base/Network/NetworkSystem.h"

//-------------------------------------------------------------------------

#ifdef _WIN32
#include "Base/Platform/PlatformUtils_Win32.h"
#endif

namespace EE
{
    constexpr static char const* const g_physicsMaterialDatabaseResourceID = "data://Physics/PhysicsMaterials.pmdb";

    //-------------------------------------------------------------------------

    void EngineModule::GetListOfAllRequiredModuleResources( TVector<ResourceID>& outResourceIDs )
    {
        outResourceIDs.emplace_back( ResourceID( g_physicsMaterialDatabaseResourceID ) );
    }

    //-------------------------------------------------------------------------

    bool EngineModule::InitializeModule( ModuleContext& context )
    {
        //-------------------------------------------------------------------------
        // Initialize core systems
        //-------------------------------------------------------------------------

        #if EE_DEVELOPMENT_TOOLS
        m_pConsole = EE::New<Console>( *context.m_pSettingsRegistry );
        EntityModel::InitializeLogQueue();
        #endif

        Physics::Core::Initialize();
        m_physicsMaterialRegistry.Initialize();

        #if EE_ENABLE_NAVPOWER
        Navmesh::NavPower::Initialize();
        #endif

        //-------------------------------------------------------------------------
        // Initialize and register renderers
        //-------------------------------------------------------------------------

        if ( m_worldRenderer.Initialize( context.m_pRenderDevice ) )
        {
            m_rendererRegistry.RegisterRenderer( &m_worldRenderer );
        }
        else
        {
            EE_LOG_ERROR( "Render", nullptr, "Failed to initialize world renderer" );
            return false;
        }

        #if EE_DEVELOPMENT_TOOLS
        //if ( m_debugRenderer.Initialize( m_pRenderDevice ) )
        //{
        //    m_rendererRegistry.RegisterRenderer( &m_debugRenderer );
        //}
        //else
        //{
        //    EE_LOG_ERROR( "Render", nullptr, "Failed to initialize debug renderer" );
        //    return false;
        //}

        if ( m_imguiRenderer.Initialize( context.m_pRenderDevice ) )
        {
            m_rendererRegistry.RegisterRenderer( &m_imguiRenderer );
        }
        else
        {
            EE_LOG_ERROR( "Render", nullptr, "Failed to initialize imgui renderer" );
            return false;
        }

        //if ( m_physicsRenderer.Initialize( m_pRenderDevice ) )
        //{
        //    m_rendererRegistry.RegisterRenderer( &m_physicsRenderer );
        //}
        //else
        //{
        //    EE_LOG_ERROR( "Render", nullptr, "Failed to initialize physics renderer" );
        //    return false;
        //}
        #endif

        //-------------------------------------------------------------------------
        // Register systems
        //-------------------------------------------------------------------------

        #if EE_DEVELOPMENT_TOOLS
        context.m_pSystemRegistry->RegisterSystem( m_pConsole );
        #endif

        context.m_pSystemRegistry->RegisterSystem( &m_entityWorldManager );
        context.m_pSystemRegistry->RegisterSystem( &m_rendererRegistry );
        context.m_pSystemRegistry->RegisterSystem( &m_physicsMaterialRegistry );

        //-------------------------------------------------------------------------
        // Register resource loaders
        //-------------------------------------------------------------------------

        m_entityCollectionLoader.SetTypeRegistryPtr( context.m_pTypeRegistry );
        context.m_pResourceSystem->RegisterResourceLoader( &m_entityCollectionLoader );

        //-------------------------------------------------------------------------

        m_renderMeshLoader.SetRenderDevicePtr( context.m_pRenderDevice );
        m_shaderLoader.SetRenderDevicePtr( context.m_pRenderDevice );
        m_textureLoader.SetRenderDevicePtr( context.m_pRenderDevice );

        context.m_pResourceSystem->RegisterResourceLoader( &m_renderMeshLoader );
        context.m_pResourceSystem->RegisterResourceLoader( &m_shaderLoader );
        context.m_pResourceSystem->RegisterResourceLoader( &m_textureLoader );
        context.m_pResourceSystem->RegisterResourceLoader( &m_materialLoader );

        //-------------------------------------------------------------------------

        m_animationClipLoader.SetTypeRegistryPtr( context.m_pTypeRegistry );
        m_graphLoader.SetTypeRegistryPtr( context.m_pTypeRegistry );

        context.m_pResourceSystem->RegisterResourceLoader( &m_skeletonLoader );
        context.m_pResourceSystem->RegisterResourceLoader( &m_animationClipLoader );
        context.m_pResourceSystem->RegisterResourceLoader( &m_graphLoader );

        //-------------------------------------------------------------------------

        m_physicsMaterialLoader.SetMaterialRegistryPtr( &m_physicsMaterialRegistry );

        context.m_pResourceSystem->RegisterResourceLoader( &m_physicsCollisionMeshLoader );
        context.m_pResourceSystem->RegisterResourceLoader( &m_physicsMaterialLoader );
        context.m_pResourceSystem->RegisterResourceLoader( &m_physicsRagdollLoader );

        //-------------------------------------------------------------------------

        context.m_pResourceSystem->RegisterResourceLoader( &m_navmeshLoader );

        return true;
    }

    void EngineModule::ShutdownModule( ModuleContext& context )
    {
        //-------------------------------------------------------------------------
        // Unregister resource loaders
        //-------------------------------------------------------------------------

        context.m_pResourceSystem->UnregisterResourceLoader( &m_navmeshLoader );

        //-------------------------------------------------------------------------

        context.m_pResourceSystem->UnregisterResourceLoader( &m_physicsRagdollLoader );
        context.m_pResourceSystem->UnregisterResourceLoader( &m_physicsMaterialLoader );
        context.m_pResourceSystem->UnregisterResourceLoader( &m_physicsCollisionMeshLoader );

        m_physicsMaterialLoader.ClearMaterialRegistryPtr();
        m_physicsMaterialRegistry.Shutdown();

        //-------------------------------------------------------------------------

        context.m_pResourceSystem->UnregisterResourceLoader( &m_animationClipLoader );
        context.m_pResourceSystem->UnregisterResourceLoader( &m_graphLoader );
        context.m_pResourceSystem->UnregisterResourceLoader( &m_skeletonLoader );

        m_animationClipLoader.ClearTypeRegistryPtr();
        m_graphLoader.ClearTypeRegistryPtr();

        //-------------------------------------------------------------------------

        context.m_pResourceSystem->UnregisterResourceLoader( &m_renderMeshLoader );
        context.m_pResourceSystem->UnregisterResourceLoader( &m_shaderLoader );
        context.m_pResourceSystem->UnregisterResourceLoader( &m_textureLoader );
        context.m_pResourceSystem->UnregisterResourceLoader( &m_materialLoader );

        m_renderMeshLoader.ClearRenderDevicePtr();
        m_shaderLoader.ClearRenderDevicePtr();
        m_textureLoader.ClearRenderDevice();

        //-------------------------------------------------------------------------

        context.m_pResourceSystem->UnregisterResourceLoader( &m_entityCollectionLoader );
        m_entityCollectionLoader.ClearTypeRegistryPtr();

        //-------------------------------------------------------------------------
        // Unregister systems
        //-------------------------------------------------------------------------

        context.m_pSystemRegistry->UnregisterSystem( &m_physicsMaterialRegistry );
        context.m_pSystemRegistry->UnregisterSystem( &m_rendererRegistry );
        context.m_pSystemRegistry->UnregisterSystem( &m_entityWorldManager );

        #if EE_DEVELOPMENT_TOOLS
        context.m_pSystemRegistry->UnregisterSystem( m_pConsole );
        #endif

        //-------------------------------------------------------------------------
        // Unregister and shutdown renderers
        //-------------------------------------------------------------------------

        if ( context.m_pRenderDevice != nullptr )
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

        //-------------------------------------------------------------------------
        // Shutdown core systems
        //-------------------------------------------------------------------------

        #if EE_ENABLE_NAVPOWER
        Navmesh::NavPower::Shutdown();
        #endif

        m_physicsMaterialRegistry.Shutdown();
        Physics::Core::Shutdown();

        #if EE_DEVELOPMENT_TOOLS
        EE::Delete( m_pConsole );
        EntityModel::ShutdownLogQueue();
        #endif
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