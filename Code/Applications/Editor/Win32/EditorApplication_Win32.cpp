#include "EditorApplication_Win32.h"
#include "Resource.h"
#include "Applications/Editor/EditorUI.h"
#include "Applications/Shared/LivePP/LivePP.h"
#include "Base/ThirdParty/cmdParser/cmdParser.h"

#include "_AutoGenerated/ToolsTypeRegistration.h"

#include <tchar.h>
#include <windows.h>

//-------------------------------------------------------------------------

namespace EE
{
    EditorEngine::EditorEngine( TFunction<bool( EE::String const& error )>&& errorHandler )
        : Engine( eastl::forward<TFunction<bool( EE::String const& error )>&&>( errorHandler ) )
    {}

    void EditorEngine::CreateToolsUI()
    {
        auto pEditorUI = EE::New<EditorUI>();
        if ( m_editorStartupMap.IsValid() )
        {
            pEditorUI->SetStartupMap( m_editorStartupMap );
        }

        m_pToolsUI = pEditorUI;
    }

    void EditorEngine::RegisterTypes()
    {
        TypeSystem::Reflection::RegisterTypes( *m_pTypeRegistry );
    }

    void EditorEngine::UnregisterTypes()
    {
        TypeSystem::Reflection::UnregisterTypes( *m_pTypeRegistry );
    }

    bool EditorEngine::InitializeToolsModulesAndSystems( ModuleContext& moduleContext )
    {
        if ( !m_engineToolsModule.InitializeModule( moduleContext ) )
        {
            return m_fatalErrorHandler( "Failed to initialize engine tools module!" );
        }

        if ( !m_gameToolsModule.InitializeModule( moduleContext ) )
        {
            return m_fatalErrorHandler( "Failed to initialize game tools module!" );
        }

        return true;
    }

    void EditorEngine::ShutdownToolsModulesAndSystems( ModuleContext& moduleContext )
    {
        m_gameToolsModule.ShutdownModule( moduleContext );
        m_engineToolsModule.ShutdownModule( moduleContext );
    }

    //-------------------------------------------------------------------------

    EditorApplication::EditorApplication( HINSTANCE hInstance )
        : Win32Application( hInstance, "Esoterica Editor", IDI_EDITOR_ICON, TBitFlags<InitOptions>( Win32Application::InitOptions::Borderless ) )
        , m_engine( TFunction<bool( String const& error )>( [this] ( String const& error )-> bool  { return FatalError( error ); } ) )
    {}

    void EditorApplication::GetBorderlessTitleBarInfo( Math::ScreenSpaceRectangle& outTitlebarRect, bool& isInteractibleWidgetHovered ) const
    {
        if ( m_engine.m_pToolsUI == nullptr )
        {
            isInteractibleWidgetHovered = false;
            return;
        }

        static_cast<EditorUI*>( m_engine.m_pToolsUI )->GetBorderlessTitleBarInfo( outTitlebarRect, isInteractibleWidgetHovered );
    }

    void EditorApplication::ProcessWindowResizeMessage( Int2 const& newWindowSize )
    {
        m_engine.GetRenderingSystem()->ResizePrimaryRenderTarget( newWindowSize );
    }

    void EditorApplication::ProcessInputMessage( UINT message, WPARAM wParam, LPARAM lParam )
    {
        m_engine.GetInputSystem()->ForwardInputMessageToInputDevices( { message, (uintptr_t) wParam, (uintptr_t) lParam } );
    }

    void EditorApplication::ProcessWindowDestructionMessage()
    {
        m_engine.m_exitRequested = true;
        Win32Application::ProcessWindowDestructionMessage();
    }

    bool EditorApplication::ProcessCommandline( int32_t argc, char** argv )
    {
        cli::Parser cmdParser( argc, argv );
        cmdParser.set_optional<std::string>( "map", "map", "", "The startup map." );

        if ( !cmdParser.run() )
        {
            return FatalError( "Invalid command line arguments!" );
        }

        std::string const map = cmdParser.get<std::string>( "map" );
        if ( !map.empty() )
        {
            m_engine.m_editorStartupMap = ResourcePath( map.c_str() );
        }

        return true;
    }

    bool EditorApplication::Initialize()
    {
        Int2 const windowDimensions( ( m_windowRect.right - m_windowRect.left ), ( m_windowRect.bottom - m_windowRect.top ) );
        if ( !m_engine.Initialize( windowDimensions ) )
        {
            return false;
        }

        return true;
    }

    bool EditorApplication::Shutdown()
    {
        return m_engine.Shutdown();
    }

    bool EditorApplication::ApplicationLoop()
    {
        return m_engine.Update();
    }
}

//-------------------------------------------------------------------------

int APIENTRY _tWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )
{
    int32_t result = 0;
    {
        #if EE_ENABLE_LPP
        auto lppAgent = EE::ScopedLPPAgent();
        #endif

        //-------------------------------------------------------------------------

        EE::ApplicationGlobalState globalState;
        EE::EditorApplication editorApplication( hInstance );
        result = editorApplication.Run( __argc, __argv );
    }

    return result;
}