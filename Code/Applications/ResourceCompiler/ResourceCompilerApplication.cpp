#include "ResourceCompilerApplication.h"
#include "CompiledResourceDatabase.h"
#include "_AutoGenerated/ToolsTypeRegistration.h"
#include "EngineTools/Resource/ResourceCompilerRegistry.h"
#include "System/Application/ApplicationGlobalState.h"
#include "System/ThirdParty/cmdParser/cmdParser.h"
#include "System/Resource/ResourceSettings.h"
#include "System/FileSystem/FileSystemUtils.h"
#include "System/IniFile.h"


#include <windows.h>
#include <iostream>

//-------------------------------------------------------------------------

using namespace EE;

//-------------------------------------------------------------------------
// Command Line Argument Parsing
//-------------------------------------------------------------------------

namespace EE
{
    struct CommandLineArgumentParser
    {
        CommandLineArgumentParser( int argc, char* argv[] )
        {
            cli::Parser cmdParser( argc, argv );
            cmdParser.set_default<bool>( false );
            cmdParser.set_optional<std::string>( "compile", "compile", "", "Compile resource" );
            cmdParser.set_optional<bool>( "debug", "debug", false, "Trigger debug break before execution." );
            cmdParser.set_optional<bool>( "force", "force", false, "Force compilation" );
            cmdParser.set_optional<bool>( "package", "package", false, "Compile resource for packaged build." );

            if ( cmdParser.run() )
            {
                m_triggerDebugBreak = cmdParser.get<bool>( "debug" );
                m_isForcedCompilation = cmdParser.get<bool>( "force" );
                m_isForPackagedBuild = cmdParser.get<bool>( "package" );

                // Get compile argument
                ResourcePath const resourcePath( cmdParser.get<std::string>( "compile" ).c_str() );
                if ( resourcePath.IsValid() )
                {
                    m_resourceID = ResourceID( resourcePath );

                    if ( m_resourceID.IsValid() )
                    {
                        m_isValid = true;
                    }
                    else
                    {
                        EE_LOG_ERROR( "Resource", "Resource Compiler", "Invalid compile request: %s\n", m_resourceID.ToString().c_str() );
                    }

                    return;
                }
            }
        }

        bool IsValid() const { return m_isValid; }

    public:

        ResourceID          m_resourceID;
        bool                m_triggerDebugBreak = false;
        bool                m_isForPackagedBuild = false;
        bool                m_isForcedCompilation = false;
        bool                m_isValid = false;
    };
}

//-------------------------------------------------------------------------
// Resource Compiler
//-------------------------------------------------------------------------

namespace EE::Resource
{
    void ResourceCompilerApplication::CompileDependencyNode::Reset()
    {
        m_ID.Clear();
        m_compiledRecord.Clear();
        m_sourcePath.Clear();
        m_targetPath.Clear();
        m_timestamp = m_combinedHash = 0;
        m_sourceExists = m_targetExists = false;
        m_errorOccurredReadingDependencies = false;
        m_compilerVersion = -1;
        DestroyDependencies();
    }

    void ResourceCompilerApplication::CompileDependencyNode::DestroyDependencies()
    {
        for ( auto pDep : m_dependencies )
        {
            pDep->DestroyDependencies();
            EE::Delete( pDep );
        }

        m_dependencies.clear();
    }

    bool ResourceCompilerApplication::CompileDependencyNode::IsUpToDate() const
    {
        if ( m_forceRecompile )
        {
            return false;
        }

        //-------------------------------------------------------------------------

        if ( !m_sourceExists )
        {
            return false;
        }

        //-------------------------------------------------------------------------

        if ( IsCompileableResource() )
        {
            if ( !m_targetExists )
            {
                return false;
            }

            if ( !m_compiledRecord.IsValid() )
            {
                return false;
            }

            if ( m_compiledRecord.m_compilerVersion != m_compilerVersion )
            {
                return false;
            }

            if ( m_compiledRecord.m_sourceTimestampHash != m_combinedHash )
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------

        for ( auto const& pDep : m_dependencies )
        {
            if ( !pDep->IsUpToDate() )
            {
                return false;
            }
        }

        //-------------------------------------------------------------------------

        return true;
    }

    //-------------------------------------------------------------------------

    ResourceCompilerApplication::ResourceCompilerApplication( CommandLineArgumentParser& argParser, ResourceSettings const& settings )
        : m_compileContext( settings.m_rawResourcePath, argParser.m_isForPackagedBuild ? settings.m_packagedBuildCompiledResourcePath : settings.m_compiledResourcePath, argParser.m_resourceID, argParser.m_isForPackagedBuild )
        , m_forceCompilation( argParser.m_isForcedCompilation )
    {
        AutoGenerated::Tools::RegisterTypes( m_typeRegistry );
        m_pCompilerRegistry = EE::New<CompilerRegistry>( m_typeRegistry, settings.m_rawResourcePath );

        //-------------------------------------------------------------------------

        m_compileContext.m_rawResourceDirectoryPath.EnsureDirectoryExists();
        m_compileContext.m_compiledResourceDirectoryPath.EnsureDirectoryExists();

        //-------------------------------------------------------------------------

        m_compiledResourceDB.Connect( settings.m_compiledResourceDatabasePath );
    }

    ResourceCompilerApplication::~ResourceCompilerApplication()
    {
        m_compileDependencyTreeRoot.DestroyDependencies();
        EE::Delete( m_pCompilerRegistry );
        AutoGenerated::Tools::UnregisterTypes( m_typeRegistry );
    }

    bool ResourceCompilerApplication::ShouldCheckCompileDependenciesForResourceType( ResourceID const& resourceID )
    {
        if ( resourceID.GetResourceTypeID() == ResourceTypeID( "map" ) )
        {
            return false;
        }

        if ( resourceID.GetResourceTypeID() == ResourceTypeID( "nav" ) )
        {
            return false;
        }

        return true;
    }

    CompilationResult ResourceCompilerApplication::Run()
    {
        if ( !m_compiledResourceDB.IsConnected() )
        {
            EE_LOG_ERROR( "Resource", "Resource Compiler", "Database connection error: %s", m_compiledResourceDB.GetError().c_str() );
            return Resource::CompilationResult::Failure;
        }

        // Try create compilation context
        if ( !m_compileContext.IsValid() )
        {
            return Resource::CompilationResult::Failure;
        }

        // Try find compiler
        auto pCompiler = m_pCompilerRegistry->GetCompilerForResourceType( m_compileContext.m_resourceID.GetResourceTypeID() );
        if ( pCompiler == nullptr )
        {
            EE_LOG_ERROR( "Resource", "Resource Compiler", "Cant find appropriate resource compiler for type: %u", m_compileContext.m_resourceID.GetResourceTypeID() );
            return Resource::CompilationResult::Failure;
        }

        // Validate request
        //-------------------------------------------------------------------------

        // Validate input path
        if ( pCompiler->IsInputFileRequired() && !FileSystem::Exists( m_compileContext.m_inputFilePath ) )
        {
            EE_LOG_ERROR( "Resource", "Resource Compiler", "Source file for data path ('%s') does not exist: '%s'\n", m_compileContext.m_rawResourceDirectoryPath.c_str(), m_compileContext.m_inputFilePath.c_str() );
            return Resource::CompilationResult::Failure;
        }

        // Try create target directory
        if ( !m_compileContext.m_outputFilePath.EnsureDirectoryExists() )
        {
            EE_LOG_ERROR( "Resource", "Resource Compiler", "Error: Destination path (%s) doesnt exist!", m_compileContext.m_outputFilePath.GetParentDirectory().c_str() );
            return Resource::CompilationResult::Failure;
        }

        // Check that target file isnt read-only
        if ( FileSystem::Exists( m_compileContext.m_outputFilePath ) && FileSystem::IsFileReadOnly( m_compileContext.m_outputFilePath ) )
        {
            EE_LOG_ERROR( "Resource", "Resource Compiler", "Error: Destination file (%s) is read-only!", m_compileContext.m_outputFilePath.GetFullPath().c_str() );
            return Resource::CompilationResult::Failure;
        }

        // Up-To-Date Check
        //-------------------------------------------------------------------------

        // Check compile dependency and if this resource needs compilation
        m_uniqueCompileDependencies.clear();
        m_compileDependencyTreeRoot.Reset();
        if ( !FillCompileDependencyNode( &m_compileDependencyTreeRoot, m_compileContext.m_resourceID ) )
        {
            EE_LOG_ERROR( "Resource", "Resource Compiler", "Failed to create dependency tree: %s", m_errorMessage.c_str() );
            return Resource::CompilationResult::Failure;
        }

        // If we are not forcing the compilation and we're up to date, there's nothing to do
        if ( m_compileDependencyTreeRoot.IsUpToDate() && !m_forceCompilation )
        {
            return Resource::CompilationResult::SuccessUpToDate;
        }

        m_compileContext.m_sourceResourceHash = m_compileDependencyTreeRoot.m_combinedHash;

        // Compile
        //-------------------------------------------------------------------------

        Resource::CompilationResult const compilationResult = pCompiler->Compile( m_compileContext );

        // Update database
        if ( compilationResult == Resource::CompilationResult::Success )
        {
            Resource::CompiledResourceRecord record;
            record.m_resourceID = m_compileContext.m_resourceID;
            record.m_compilerVersion = m_compileDependencyTreeRoot.m_compilerVersion;
            record.m_fileTimestamp = m_compileDependencyTreeRoot.m_timestamp;
            record.m_sourceTimestampHash = m_compileDependencyTreeRoot.m_combinedHash;
            m_compiledResourceDB.WriteRecord( record );
        }

        return compilationResult;
    }

    bool ResourceCompilerApplication::BuildCompileDependencyTree( ResourceID const& resourceID )
    {
        EE_ASSERT( resourceID.IsValid() );

        //-------------------------------------------------------------------------

        m_errorMessage.clear();
        m_uniqueCompileDependencies.clear();
        m_compileDependencyTreeRoot.Reset();
        return FillCompileDependencyNode( &m_compileDependencyTreeRoot, resourceID );
    }

    bool ResourceCompilerApplication::TryReadCompileDependencies( FileSystem::Path const& resourceFilePath, TVector<ResourceID>& outDependencies ) const
    {
        EE_ASSERT( resourceFilePath.IsValid() );

        auto pDescriptor = ResourceDescriptor::TryReadFromFile( m_typeRegistry, resourceFilePath );
        if ( pDescriptor == nullptr )
        {
            return false;
        }

        pDescriptor->GetCompileDependencies( outDependencies );

        EE::Delete( pDescriptor );

        return true;
    }

    bool ResourceCompilerApplication::FillCompileDependencyNode( CompileDependencyNode* pNode, ResourceID const& resourceID )
    {
        EE_ASSERT( pNode != nullptr );

        // Basic resource info
        //-------------------------------------------------------------------------

        pNode->m_ID = resourceID;

        pNode->m_sourcePath = ResourcePath::ToFileSystemPath( m_compileContext.m_rawResourceDirectoryPath, resourceID.GetResourcePath() );
        pNode->m_sourceExists = FileSystem::Exists( pNode->m_sourcePath );
        pNode->m_timestamp = pNode->m_sourceExists ? FileSystem::GetFileModifiedTime( pNode->m_sourcePath ) : 0;

        // Handle compilable resources
        //-------------------------------------------------------------------------

        auto pCompiler = m_pCompilerRegistry->GetCompilerForResourceType( resourceID.GetResourceTypeID() );
        bool const isCompilableResource = pCompiler != nullptr;
        bool skipDependencyCheck = !isCompilableResource || !ShouldCheckCompileDependenciesForResourceType( resourceID );
        if ( isCompilableResource )
        {
            pNode->m_targetPath = ResourcePath::ToFileSystemPath( m_compileContext.m_compiledResourceDirectoryPath, resourceID.GetResourcePath() );
            pNode->m_targetExists = FileSystem::Exists( pNode->m_targetPath );

            pNode->m_compilerVersion = pCompiler->GetVersion();
            m_compiledResourceDB.GetRecord( resourceID, pNode->m_compiledRecord );

            // Some compilers dont require an input file to run - these resources should always be recompiled!
            if ( !pNode->m_sourceExists && !pCompiler->IsInputFileRequired() )
            {
                pNode->m_forceRecompile = true;
                skipDependencyCheck = true;
            }
        }

        // Generate dependencies
        //-------------------------------------------------------------------------

        if ( !skipDependencyCheck )
        {
            TVector<ResourceID> dependencies;
            if ( TryReadCompileDependencies( pNode->m_sourcePath, dependencies ) )
            {
                for ( auto const& dependencyResourceID : dependencies )
                {
                    // Skip resources already in the tree!
                    if ( VectorContains( m_uniqueCompileDependencies, dependencyResourceID ) )
                    {
                        continue;
                    }

                    // Check for circular references
                    //-------------------------------------------------------------------------

                    auto pNodeToCheck = pNode;
                    while ( pNodeToCheck != nullptr )
                    {
                        if ( pNodeToCheck->m_ID == dependencyResourceID )
                        {
                            m_errorMessage = "Circular dependency detected!";
                            return false;
                        }

                        pNodeToCheck = pNodeToCheck->m_pParentNode;
                    }

                    // Create dependency
                    //-------------------------------------------------------------------------

                    auto pChildDependencyNode = pNode->m_dependencies.emplace_back( EE::New<CompileDependencyNode>() );
                    pChildDependencyNode->m_pParentNode = pNode;
                    if ( !FillCompileDependencyNode( pChildDependencyNode, dependencyResourceID ) )
                    {
                        return false;
                    }

                    m_uniqueCompileDependencies.emplace_back( dependencyResourceID );
                }
            }
            else
            {
                pNode->m_errorOccurredReadingDependencies = true;
                return false;
            }
        }

        // Generate combined hash
        //-------------------------------------------------------------------------

        pNode->m_combinedHash = pNode->m_timestamp;
        for ( auto const pDep : pNode->m_dependencies )
        {
            pNode->m_combinedHash += pDep->m_combinedHash;
        }

        return true;
    }
}

//-------------------------------------------------------------------------
// Application Entry Point
//-------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    ApplicationGlobalState State;

    // Read CMD line arguments and process request
    //-------------------------------------------------------------------------

    CommandLineArgumentParser argParser( argc, argv );

    for ( int i = 0; i < argc; i++ )
    {
        std::cout << argv[i] << std::endl;
    }

    if ( !argParser.IsValid() )
    {
        EE_LOG_ERROR( "Resource", "Resource Compiler", "Invalid command line arguments" );
        return -1;
    }

    // Read INI settings
    //-------------------------------------------------------------------------

    FileSystem::Path const iniFilePath = FileSystem::GetCurrentProcessPath().Append( "Esoterica.ini" );
    IniFile iniFile( iniFilePath );
    if ( !iniFile.IsValid() )
    {
        EE_LOG_ERROR( "Resource", "Resource Compiler", "Failed to read INI file: %s", iniFilePath.c_str() );
        return -1;
    }

    Resource::ResourceSettings settings;
    if ( !settings.ReadSettings( iniFile ) )
    {
        EE_LOG_ERROR( "Resource", "Resource Compiler", "Failed to read settings from INI file: %s", iniFilePath.c_str() );
        return -1;
    }

    // Debug Hook
    //-------------------------------------------------------------------------

    if ( argParser.m_triggerDebugBreak )
    {
        EE_HALT();
    }

    // Compilation DB
    //-------------------------------------------------------------------------

    Resource::CompiledResourceDatabase compiledResourceDB;
    if ( !compiledResourceDB.Connect( settings.m_compiledResourceDatabasePath ) )
    {
        EE_LOG_ERROR( "Resource", "Resource Compiler", "Database connection error: %s", compiledResourceDB.GetError().c_str() );
        return -1;
    }

    // Compile Resource
    //-------------------------------------------------------------------------

    Resource::ResourceCompilerApplication application( argParser, settings );
    return (int32_t)application.Run();
}