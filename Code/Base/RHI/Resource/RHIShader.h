#pragma once

#include "RHIResource.h"

namespace EE::RHI
{
    class EE_BASE_API RHIShader : public RHIResource
    {
    public:

        RHIShader( ERHIType rhiType = ERHIType::Invalid )
            : RHIResource( rhiType )
        {}
        virtual ~RHIShader() = default;

        virtual bool IsValid() const = 0;

    private:

    };
}
