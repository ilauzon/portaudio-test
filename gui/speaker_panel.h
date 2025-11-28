// speaker_panel.h
#pragma once

#include <wx/wx.h>
#include <wx/bitmap.h> // Explicitly include bitmap header
#include "../portaudio_listener.h"  // paTestData, CHANNEL_COUNT, Point

class SpeakerPanel : public wxPanel
{
public:
    SpeakerPanel(wxWindow* parent, paTestData* data, bool interactiveMode);

    // interactiveMode here means "listener draggable/rotatable or not"
    void SetInteractive(bool interactive);

    // Reset listener + speakers to their initial positions
    void ResetPositions();

private:
    // --- NEW: Variable to store the loaded PNG ---
    wxBitmap m_speakerBitmap; 
    // ---------------------------------------------

    // Which edge (if any) is being resized
    enum class DragEdge { None, Left, Right, Top, Bottom };

    paTestData* m_data = nullptr;

    // Whether the listener (head) is allowed to move/rotate.
    // Speakers are always draggable in both modes.
    bool     m_allowListenerDrag = true;

    bool     m_mouseDown         = false;
    bool     m_draggingListener  = false;
    bool     m_draggingYaw       = false;  // dragging the red direction handle
    int      m_dragSpeakerIndex  = -1;     // which speaker we're dragging
    int      m_selectedSpeaker   = -1;     // which speaker to show distances from
    DragEdge m_dragEdge          = DragEdge::None;

    // Initial positions captured at construction (for Reset button)
    Point m_initialListenerPosition{};
    float m_initialListenerYaw = 0.0f;
    Point m_initialSpeakerPositions[CHANNEL_COUNT]{};

    // Screen positions captured at start of a box-resize drag,
    // so we can keep everything visually fixed while bounds change.
    wxPoint m_listenerScreenAtResize;
    wxPoint m_speakerScreenAtResize[CHANNEL_COUNT];

    float computeScaleForCurrentBounds(int w, int h,
                                       float& minX, float& minY,
                                       float& maxX, float& maxY) const;

    wxPoint worldToScreen(float x, float y, int w, int h, float scale) const;
    Point   screenToWorld(int px, int py, int w, int h, float scale) const;

    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseLeave(wxMouseEvent& evt);

    wxDECLARE_EVENT_TABLE();
};