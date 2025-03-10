#include "ResourceLoader_RenderMesh.h"
#include "Engine/Render/Mesh/StaticMesh.h"
#include "Engine/Render/Mesh/SkeletalMesh.h"
#include "Base/Render/RenderDevice.h"
#include "Base/Serialization/BinarySerialization.h"
#include "Base/RHI/RHIDevice.h"
#include "Base/RHI/Resource/RHIResourceCreationCommons.h"

//-------------------------------------------------------------------------

namespace EE::Render
{
    MeshLoader::MeshLoader()
    {
        m_loadableTypes.push_back( StaticMesh::GetStaticResourceTypeID() );
        m_loadableTypes.push_back( SkeletalMesh::GetStaticResourceTypeID() );
    }

    bool MeshLoader::LoadInternal( ResourceID const& resourceID, Resource::ResourceRecord* pResourceRecord, Serialization::BinaryInputArchive& archive ) const
    {
        EE_ASSERT( m_pRenderDevice != nullptr );

        Mesh* pMeshResource = nullptr;

        // Static Mesh
        if ( resourceID.GetResourceTypeID() == StaticMesh::GetStaticResourceTypeID() )
        {
            StaticMesh* pStaticMesh = EE::New<StaticMesh>();
            archive << *pStaticMesh;
            pMeshResource = pStaticMesh;
        }
        else // Skeletal Mesh
        {
            SkeletalMesh* pSkeletalMesh = EE::New<SkeletalMesh>();
            archive << *pSkeletalMesh;
            pMeshResource = pSkeletalMesh;
        }

        EE_ASSERT( !pMeshResource->m_vertices.empty() );
        EE_ASSERT( !pMeshResource->m_indices.empty() );

        //-------------------------------------------------------------------------

        pResourceRecord->SetResourceData( pMeshResource );

        return true;
    }

    Resource::InstallResult MeshLoader::Install( ResourceID const& resourceID, Resource::ResourceRecord* pResourceRecord, Resource::InstallDependencyList const& installDependencies ) const
    {
        auto pMesh = pResourceRecord->GetResourceData<Mesh>();

        // Create GPU buffers
        //-------------------------------------------------------------------------
        // BLOCKING FOR NOW! TODO: request the load and return Resource::InstallResult::InProgress

        m_pRenderDevice->LockDevice();
        {
            // TODO: vertex buffer refactoring
            RHI::RHIBufferUploadData uploadData;
            uploadData.m_pData = pMesh->m_vertices.data();
            RHI::RHIBufferCreateDesc bufferDesc = RHI::RHIBufferCreateDesc::NewVertexBuffer( (uint32_t) pMesh->m_vertices.size() );
            bufferDesc.WithInitialData( uploadData );
            pMesh->m_vertexBuffer.m_pBuffer = m_pRenderDevice->GetRHIDevice()->CreateBuffer( bufferDesc );
            EE_ASSERT( pMesh->m_vertexBuffer.IsValid() );

            //m_pRenderDevice->CreateBuffer( pMesh->m_vertexBuffer, pMesh->m_vertices.data() );
            //EE_ASSERT( pMesh->m_vertexBuffer.IsValid() );

            uploadData.m_pData = pMesh->m_indices.data();
            bufferDesc = RHI::RHIBufferCreateDesc::NewIndexBuffer( (uint32_t) pMesh->m_indices.size() * sizeof( uint32_t ) );
            bufferDesc.WithInitialData( uploadData );
            pMesh->m_indexBuffer.m_pBuffer = m_pRenderDevice->GetRHIDevice()->CreateBuffer( bufferDesc );
            EE_ASSERT( pMesh->m_indexBuffer.IsValid() );

            //m_pRenderDevice->CreateBuffer( pMesh->m_indexBuffer, pMesh->m_indices.data() );
        }
        m_pRenderDevice->UnlockDevice();

        // Set materials
        //-------------------------------------------------------------------------

        for ( auto& pMaterial : pMesh->m_materials )
        {
            // Default materials are allowed to be unset
            if ( !pMaterial.GetResourceID().IsValid() )
            {
                continue;
            }

            pMaterial = GetInstallDependency( installDependencies, pMaterial.GetResourceID() );
        }

        //-------------------------------------------------------------------------

        ResourceLoader::Install( resourceID, pResourceRecord, installDependencies );
        return Resource::InstallResult::Succeeded;
    }

    void MeshLoader::Uninstall( ResourceID const& resourceID, Resource::ResourceRecord* pResourceRecord ) const
    {
        auto pMesh = pResourceRecord->GetResourceData<Mesh>();
        if ( pMesh != nullptr )
        {
            //m_pRenderDevice->LockDevice();
            m_pRenderDevice->GetRHIDevice()->DestroyBuffer( pMesh->m_vertexBuffer.m_pBuffer );
            m_pRenderDevice->GetRHIDevice()->DestroyBuffer( pMesh->m_indexBuffer.m_pBuffer );
            //m_pRenderDevice->DestroyBuffer( pMesh->m_vertexBuffer );
            //m_pRenderDevice->DestroyBuffer( pMesh->m_indexBuffer );
            //m_pRenderDevice->UnlockDevice();

            pMesh->m_vertexBuffer.m_pBuffer = nullptr;
            pMesh->m_indexBuffer.m_pBuffer = nullptr;
        }
    }

    Resource::InstallResult MeshLoader::UpdateInstall( ResourceID const& resourceID, Resource::ResourceRecord* pResourceRecord ) const
    {
        EE_UNIMPLEMENTED_FUNCTION();
        return Resource::InstallResult::Failed;
    }
}