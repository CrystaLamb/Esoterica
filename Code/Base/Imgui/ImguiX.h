#pragma once

#include "Base/_Module/API.h"
#include "ImguiFont.h"
#include "ImguiStyle.h"
#include "Base/ThirdParty/imgui/imgui_internal.h"
#include "Base/Math/Transform.h"
#include "Base/Math/Rectangle.h"
#include "Base/Types/Color.h"
#include "Base/Types/String.h"
#include "Base/Types/BitFlags.h"
#include "Base/Types/Function.h"
#include "Base/Types/Arrays.h"
#include "Base/RHI/Resource/RHIResourceCreationCommons.h"

//-------------------------------------------------------------------------

namespace EE::Render { class Viewport; }
namespace EE::RHI { class RHIDevice; class RHITexture; }

//-------------------------------------------------------------------------
// ImGui Extensions
//-------------------------------------------------------------------------
// This is the primary integration of DearImgui in Esoterica.
//
// * Provides the necessary imgui state updates through the frame start/end functions
// * Provides helpers for common operations
//-------------------------------------------------------------------------

#if EE_DEVELOPMENT_TOOLS
namespace EE::ImGuiX
{
    //-------------------------------------------------------------------------
    // General helpers
    //-------------------------------------------------------------------------

    EE_BASE_API void MakeTabVisible( char const* const pWindowName );

    EE_BASE_API ImVec2 ClampToRect( ImRect const& rect, ImVec2 const& inPoint );

    // Returns the closest point on the rect border to the specified point
    EE_BASE_API ImVec2 GetClosestPointOnRectBorder( ImRect const& rect, ImVec2 const& inPoint );

    // Is this a valid name ID character (i.e. A-Z, a-z, 0-9, _ )
    inline bool IsValidNameIDChar( ImWchar c )
    {
        return isalnum( c ) || c == '_';
    }

    // Filter a text callback restricting it to valid name ID characters
    inline int FilterNameIDChars( ImGuiInputTextCallbackData* data )
    {
        if ( IsValidNameIDChar( data->EventChar ) )
        {
            return 0;
        }
        return 1;
    }

    // Display a modal popup that is restricted to the current window's viewport
    EE_BASE_API bool BeginViewportPopupModal( char const* pPopupName, bool* pIsPopupOpen, ImVec2 const& size = ImVec2( 0, 0 ), ImGuiCond windowSizeCond = ImGuiCond_Always, ImGuiWindowFlags windowFlags = ( ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize ) );

    // Cancels an option dialog via ESC
    inline bool CancelDialogViaEsc( bool isDialogOpen )
    {
        if ( ImGui::IsKeyPressed( ImGuiKey_Escape ) )
        {
            ImGui::CloseCurrentPopup();
            return false;
        }

        return isDialogOpen;
    }

    //-------------------------------------------------------------------------
    // Layout and Separators
    //-------------------------------------------------------------------------

    // Same as the Imgui::SameLine except it also draws a vertical separator.
    EE_BASE_API void SameLineSeparator( float width = 0, Color const& color = Colors::Transparent );

    // Create a collapsible framed child window - Must always call EndCollapsibleChildWindow if you call begin child window
    EE_BASE_API bool BeginCollapsibleChildWindow( char const* pLabelAndID, bool initiallyOpen = true, Color const& backgroundColor = ImGuiX::Style::s_colorGray7 );

    // End a collapsible framed child window - must always be called if you call begin child window to match ImGui child window behavior
    EE_BASE_API void EndCollapsibleChildWindow();

    //-------------------------------------------------------------------------
    // Basic Widgets
    //-------------------------------------------------------------------------

    // Draw a tooltip for the immediately preceding item
    EE_BASE_API void ItemTooltip( const char* fmt, ... );

    // Draw a tooltip with a custom hover delay for the immediately preceding item
    EE_BASE_API void ItemTooltipDelayed( float tooltipDelay, const char* fmt, ... );

    // For use with text widget
    EE_BASE_API void TextTooltip( const char* fmt, ... );

    // A smaller check box allowing us to use a larger frame padding value
    EE_BASE_API bool Checkbox( char const* pLabel, bool* pValue );

    // Draw a button with an explicit icon
    EE_BASE_API bool IconButton( char const* pIcon, char const* pLabel, Color const& iconColor = ImGui::ColorConvertFloat4ToU32( ImGui::GetStyle().Colors[ImGuiCol_Text] ), ImVec2 const& size = ImVec2( 0, 0 ), bool shouldCenterContents = false );

    // Draw a colored button
    EE_BASE_API bool ColoredButton( Color const& backgroundColor, Color const& foregroundColor, char const* label, ImVec2 const& size = ImVec2( 0, 0 ) );

    // Draw a colored icon button
    EE_BASE_API bool ColoredIconButton( Color const& backgroundColor, Color const& foregroundColor, Color const& iconColor, char const* pIcon, char const* label, ImVec2 const& size = ImVec2( 0, 0 ), bool shouldCenterContents = false );

    // Draws a flat button - a button with no background
    EE_BASE_API bool FlatButton( char const* label, ImVec2 const& size = ImVec2( 0, 0 ) );

    // Draws a flat button - with a custom text color
    EE_FORCE_INLINE bool FlatButtonColored( Color const& foregroundColor, char const* label, ImVec2 const& size = ImVec2( 0, 0 ) )
    {
        ImGui::PushStyleColor( ImGuiCol_Button, 0 );
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( foregroundColor ) );
        bool const result = ImGui::Button( label, size );
        ImGui::PopStyleColor( 2 );

        return result;
    }

    // Draw a colored icon button
    EE_BASE_API bool FlatIconButton( char const* pIcon, char const* pLabel, Color const& iconColor = ImGui::ColorConvertFloat4ToU32( ImGui::GetStyle().Colors[ImGuiCol_Text] ), ImVec2 const& size = ImVec2( 0, 0 ), bool shouldCenterContents = false );

    // Button with extra drop down options - returns true if the primary button was pressed
    EE_BASE_API bool IconButtonWithDropDown( char const* widgetID, char const* pIcon, char const* pButtonLabel, Color const& iconColor, float buttonWidth, TFunction<void()> const& comboCallback, bool shouldCenterContents = false );

    // Toggle button
    EE_BASE_API bool ToggleButton( char const* pOnLabel, char const* pOffLabel, bool& value, ImVec2 const& size = ImVec2( 0, 0 ), Color const& onColor = ImGuiX::Style::s_colorAccent0, Color const& offColor = ImGui::ColorConvertFloat4ToU32( ImGui::GetStyle().Colors[ImGuiCol_Text] ) );

    // Toggle button
    EE_BASE_API bool FlatToggleButton( char const* pOnLabel, char const* pOffLabel, bool& value, ImVec2 const& size = ImVec2( 0, 0 ), Color const& onColor = ImGuiX::Style::s_colorAccent0, Color const& offColor = ImGui::ColorConvertFloat4ToU32( ImGui::GetStyle().Colors[ImGuiCol_Text] ) );

    // Button that creates a drop down menu once clicked
    EE_BASE_API void DropDownButton( char const* pLabel, TFunction<void()> const& contextMenuCallback, ImVec2 const& size = ImVec2( 0, 0 ) );

    // Draw an arrow between two points
    EE_BASE_API void DrawArrow( ImDrawList* pDrawList, ImVec2 const& arrowStart, ImVec2 const& arrowEnd, Color const& color, float arrowWidth, float arrowHeadWidth = 5.0f );

    // Draw an overlaid icon in a window, returns true if clicked
    EE_BASE_API bool DrawOverlayIcon( ImVec2 const& iconPos, char icon[4], void* iconID, bool isSelected = false, Color const& selectedColor = ImGui::ColorConvertFloat4ToU32( ImGui::GetStyle().Colors[ImGuiCol_ButtonActive] ) );

    // Draw a basic spinner
    EE_BASE_API bool DrawSpinner( char const* pLabel, Color const& color = ImGui::ColorConvertFloat4ToU32( ImGui::GetStyle().Colors[ImGuiCol_Text] ), ImVec2 size = ImVec2( 0, 0 ), float thickness = 3.0f, float padding = ImGui::GetStyle().FramePadding.y );

    //-------------------------------------------------------------------------

    EE_BASE_API bool InputFloat2( char const* pID, Float2& value, float width = -1 );
    EE_BASE_API bool InputFloat3( char const* pID, Float3& value, float width = -1 );
    EE_BASE_API bool InputFloat4( char const* pID, Float4& value, float width = -1 );

    EE_BASE_API bool InputTransform( char const* pID, Transform& value, float width = -1 );

    EE_BASE_API void DrawFloat2( Float2 const& value, float width = -1 );
    EE_BASE_API void DrawFloat3( Float3 const& value, float width = -1 );
    EE_BASE_API void DrawFloat4( Float4 const& value, float width = -1 );

    EE_BASE_API void DrawTransform( Transform const& value, float width = -1 );

    //-------------------------------------------------------------------------

    static void HelpMarker( const char* pHelpText )
    {
        ImGui::TextDisabled( EE_ICON_HELP_CIRCLE_OUTLINE );
        if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort ) && ImGui::BeginTooltip() )
        {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
            ImGui::TextUnformatted( pHelpText );
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    //-------------------------------------------------------------------------
    // Notifications
    //-------------------------------------------------------------------------

    EE_BASE_API void NotifyInfo( const char* format, ... );
    EE_BASE_API void NotifySuccess( const char* format, ... );
    EE_BASE_API void NotifyWarning( const char* format, ... );
    EE_BASE_API void NotifyError( const char* format, ... );

    //-------------------------------------------------------------------------
    // Images
    //-------------------------------------------------------------------------

    EE_FORCE_INLINE void Image( ImTextureID imageID, ImVec2 const& size, ImVec2 const& uv0 = ImVec2( 0, 0 ), ImVec2 const& uv1 = ImVec2( 1, 1 ), Color const& tintColor = Colors::White, Color const& borderColor = Colors::Transparent )
    {
        ImGui::Image( imageID, size, uv0, uv1, ImVec4( tintColor ), ImVec4( borderColor ) );
    }

    EE_FORCE_INLINE void ImageButton( char const* pButtonID, ImTextureID imageID, ImVec2 const& size, ImVec2 const& uv0 = ImVec2( 0, 0 ), ImVec2 const& uv1 = ImVec2( 1, 1 ), Color const& backgroundColor = Colors::Transparent, Color const& tintColor = Colors::White )
    {
        ImGui::ImageButton( pButtonID, imageID, size, uv0, uv1, ImVec4( backgroundColor ), ImVec4( tintColor ) );
    }

    //-------------------------------------------------------------------------
    // Advanced widgets
    //-------------------------------------------------------------------------

    // A simple filter entry widget that allows you to string match to some entered text
    class EE_BASE_API FilterWidget
    {
        constexpr static uint32_t const s_bufferSize = 255;

    public:

        enum Flags : uint8_t
        {
            TakeInitialFocus = 0
        };

    public:

        // Draws the filter. Returns true if the filter has been updated
        bool UpdateAndDraw( float width = -1, TBitFlags<Flags> flags = TBitFlags<Flags>() );

        // Manually set the filter buffer
        void SetFilter( String const& filterText );

        // Set the help text shown when we dont have focus and the filter is empty
        void SetFilterHelpText( String const& helpText ) { m_filterHelpText = helpText; }

        // Clear the filter
        inline void Clear();

        // Do we have a filter set?
        inline bool HasFilterSet() const { return !m_tokens.empty(); }

        // Get the split filter text token
        inline TVector<String> const& GetFilterTokens() const { return m_tokens; }

        // Does a provided string match the current filter - the string copy is intentional!
        bool MatchesFilter( String string );

        // Does a provided string match the current filter - the string copy is intentional!
        bool MatchesFilter( InlineString string );

        // Does a provided string match the current filter
        bool MatchesFilter( char const* pString ) { return MatchesFilter( InlineString( pString ) ); }

    private:

        void OnBufferUpdated();

    private:

        char                m_buffer[s_bufferSize] = { 0 };
        TVector<String>     m_tokens;
        String              m_filterHelpText = "Filter...";
    };

    //-------------------------------------------------------------------------

    // A simple 3D gizmo to show the orientation of a camera in a scene
    struct EE_BASE_API OrientationGuide
    {
        constexpr static float const g_windowPadding = 4.0f;
        constexpr static float const g_windowRounding = 2.0f;
        constexpr static float const g_guideDimension = 55.0f;
        constexpr static float const g_axisHeadRadius = 3.0f;
        constexpr static float const g_axisHalfLength = ( g_guideDimension / 2 ) - g_axisHeadRadius - 4.0f;
        constexpr static float const g_worldRenderDistanceZ = 5.0f;
        constexpr static float const g_axisThickness = 2.0f;

    public:

        static Float2 GetSize() { return Float2( g_guideDimension, g_guideDimension ); }
        static float GetWidth() { return g_guideDimension / 2; }
        static void Draw( Float2 const& guideOrigin, Render::Viewport const& viewport );
    };

    //-------------------------------------------------------------------------
    // Application level widgets
    //-------------------------------------------------------------------------

    struct EE_BASE_API ApplicationTitleBar
    {
        constexpr static float const s_windowControlButtonWidth = 45;
        constexpr static float const s_minimumDraggableGap = 24; // Minimum open gap left open to allow dragging
        constexpr static float const s_sectionPadding = 8; // Padding between the window frame/window controls and the menu/control sections

        static inline float GetWindowsControlsWidth() { return s_windowControlButtonWidth * 3; }
        static void DrawWindowControls();

    public:

        // This function takes two delegates and sizes each representing the title bar menu and an extra optional controls section
        void Draw( TFunction<void()>&& menuSectionDrawFunction = TFunction<void()>(), float menuSectionWidth = 0, TFunction<void()>&& controlsSectionDrawFunction = TFunction<void()>(), float controlsSectionWidth = 0 );

        // Get the screen space rectangle for this title bar
        Math::ScreenSpaceRectangle const& GetScreenRectangle() const { return m_rect; }

    private:

        Math::ScreenSpaceRectangle  m_rect;
    };
}
#endif