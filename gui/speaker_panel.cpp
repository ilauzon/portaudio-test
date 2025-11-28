// speaker_panel.cpp
#include "speaker_panel.h"

#include <wx/dcbuffer.h>
#include <wx/bitmap.h> // for wxBitmap, wxBitmapBundle
#include <cmath>
#include <algorithm>

wxBEGIN_EVENT_TABLE(SpeakerPanel, wxPanel)
    EVT_PAINT(SpeakerPanel::OnPaint)
        EVT_LEFT_DOWN(SpeakerPanel::OnLeftDown)
            EVT_LEFT_UP(SpeakerPanel::OnLeftUp)
                EVT_MOTION(SpeakerPanel::OnMouseMove)
                    EVT_LEAVE_WINDOW(SpeakerPanel::OnMouseLeave)
                        wxEND_EVENT_TABLE()

                            SpeakerPanel::SpeakerPanel(wxWindow *parent, paTestData *data, bool interactiveMode)
    : wxPanel(parent), m_data(data), m_allowListenerDrag(interactiveMode)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    // Capture initial positions for reset
    if (m_data)
    {
        m_initialListenerPosition = m_data->currentListenerPosition;
        m_initialListenerYaw = m_data->listenerYaw;
        for (int i = 0; i < CHANNEL_COUNT; ++i)
        {
            m_initialSpeakerPositions[i] = m_data->speakerPositions[i];
        }
    }
}

void SpeakerPanel::SetInteractive(bool interactive)
{
    // interactive here = can drag/rotate the listener (head)
    m_allowListenerDrag = interactive;

    // Clear drag state when changing modes
    m_mouseDown = false;
    m_draggingListener = false;
    m_draggingYaw = false;
    m_dragSpeakerIndex = -1;
    m_selectedSpeaker = -1;
    if (HasCapture())
        ReleaseMouse();
    Refresh();
}

void SpeakerPanel::ResetPositions()
{
    if (!m_data)
        return;

    m_data->currentListenerPosition = m_initialListenerPosition;
    m_data->listenerYaw = m_initialListenerYaw;

    for (int i = 0; i < CHANNEL_COUNT; ++i)
    {
        m_data->speakerPositions[i] = m_initialSpeakerPositions[i];
    }

    m_mouseDown = false;
    m_draggingListener = false;
    m_draggingYaw = false;
    m_dragSpeakerIndex = -1;
    m_selectedSpeaker = -1;
    if (HasCapture())
        ReleaseMouse();

    Refresh();
}

float SpeakerPanel::computeScaleForCurrentBounds(
    int w, int h,
    float &minX, float &minY,
    float &maxX, float &maxY) const
{
    if (!m_data)
    {
        minX = minY = -1.0f;
        maxX = maxY = 1.0f;
        return 1.0f;
    }

    minX = m_data->subjectBounds[0].x;
    minY = m_data->subjectBounds[0].y;
    maxX = m_data->subjectBounds[1].x;
    maxY = m_data->subjectBounds[1].y;

    const int margin = 20;

    float maxAbsX = std::max(std::fabs(minX), std::fabs(maxX));
    float maxAbsY = std::max(std::fabs(minY), std::fabs(maxY));

    if (maxAbsX < 0.001f)
        maxAbsX = 1.0f;
    if (maxAbsY < 0.001f)
        maxAbsY = 1.0f;

    float scaleX = (w / 2.0f - margin) / maxAbsX;
    float scaleY = (h / 2.0f - margin) / maxAbsY;
    float scale = (scaleX < scaleY ? scaleX : scaleY);
    if (scale <= 0.0f)
        scale = 1.0f;

    return scale;
}

wxPoint SpeakerPanel::worldToScreen(float x, float y, int w, int h, float scale) const
{
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    float sx = cx + x * scale;
    float sy = cy - y * scale; // invert Y so +Y is up

    return wxPoint((int)sx, (int)sy);
}

Point SpeakerPanel::screenToWorld(int px, int py, int w, int h, float scale) const
{
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    float x = (px - cx) / scale;
    float y = (cy - py) / scale; // invert Y back

    return Point{x, y};
}

void SpeakerPanel::OnPaint(wxPaintEvent &event)
{
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    if (!m_data)
        return;

    int w, h;
    GetClientSize(&w, &h);

    float minX, minY, maxX, maxY;
    float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);

    // 1. Draw Room Bounds
    dc.SetPen(wxPen(wxColour(255, 255, 255), 3));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    wxPoint topLeft = worldToScreen(minX, maxY, w, h, scale);
    wxPoint bottomRight = worldToScreen(maxX, minY, w, h, scale);
    dc.DrawRectangle(topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);

    // 2. Load SVG
    static wxBitmapBundle s_speakerIcon;
    static bool s_iconLoaded = false;
    const int iconDisplaySize = 32; 

    if (!s_iconLoaded)
    {
        s_speakerIcon = wxBitmapBundle::FromSVGFile("assets/icons/speaker.svg", wxSize(iconDisplaySize, iconDisplaySize));
        s_iconLoaded = true;
    }

    // 3. Draw Speakers (Name, Icon, Ramp, Number)
    const int gap = 2;
    const int sliderHeight = 8;
    const int sliderWidth = 32;
    wxFont mainFont = dc.GetFont();
    wxFont smallFont = mainFont;
    smallFont.SetPointSize(std::max(7, mainFont.GetPointSize() - 2));

    for (int i = 0; i < CHANNEL_COUNT; ++i)
    {
        float vol = m_data->channelGains[i];
        Point sp = m_data->speakerPositions[i];
        wxPoint p = worldToScreen(sp.x, sp.y, w, h, scale);

        int centerX = p.x;
        int centerY = p.y; 
        int iconHalf = iconDisplaySize / 2;
        int iconTop = centerY - iconHalf;
        int iconBottom = centerY + iconHalf;

        // A. Name
        dc.SetFont(smallFont);
        dc.SetTextForeground(*wxWHITE);
        wxString nameStr;
        nameStr.Printf("Ch %d", i);
        wxSize nameSize = dc.GetTextExtent(nameStr);
        dc.DrawText(nameStr, centerX - (nameSize.GetWidth() / 2), iconTop - gap - nameSize.GetHeight());

        // B. Icon
        if (s_speakerIcon.IsOk())
        {
            dc.SetTextForeground(*wxWHITE); 
            wxBitmap bmp = s_speakerIcon.GetBitmap(wxSize(iconDisplaySize, iconDisplaySize));
            dc.DrawBitmap(bmp, centerX - iconHalf, iconTop, true);
        }
        else
        {
            dc.SetPen(*wxWHITE_PEN);
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(centerX - iconHalf, iconTop, iconDisplaySize, iconDisplaySize);
        }

        // C. Volume Ramp
        int triTop = iconBottom + gap;
        int triBottom = triTop + sliderHeight;
        int triLeft = centerX - (sliderWidth / 2);
        int triRight = centerX + (sliderWidth / 2);

        wxPoint rampPoints[3];
        rampPoints[0] = wxPoint(triLeft, triBottom);
        rampPoints[1] = wxPoint(triRight, triBottom);
        rampPoints[2] = wxPoint(triRight, triTop);

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(80, 80, 80))); 
        dc.DrawPolygon(3, rampPoints);

        if (vol > 0.001f)
        {
            int fillWidth = (int)(sliderWidth * vol);
            if (fillWidth > sliderWidth) fillWidth = sliderWidth;
            dc.SetClippingRegion(triLeft, triTop, fillWidth, sliderHeight + 1);
            int greenVal = (int)(100 + 155 * vol);
            if (greenVal > 255) greenVal = 255;
            dc.SetBrush(wxBrush(wxColour(50, greenVal, 50))); 
            dc.DrawPolygon(3, rampPoints); 
            dc.DestroyClippingRegion();
        }

        // D. Number
        wxString volStr;
        volStr.Printf("%.2f", vol);
        wxSize volSize = dc.GetTextExtent(volStr);
        dc.DrawText(volStr, centerX - (volSize.GetWidth() / 2), triBottom + gap);
    }

    dc.SetFont(mainFont);

    // 4. Draw Listener & FOV Cone
    Point L = m_data->currentListenerPosition;
    wxPoint lp = worldToScreen(L.x, L.y, w, h, scale);

    // --- MATH FOR DIRECTION AND CONE ---
    float yawRad = -m_data->listenerYaw * 2.0f * M_PI;
    const float dirLen = 0.5f;     // Length of the center line
    const float coneAngle = 20.0f * (M_PI / 180.0f); // Width of the cone (FOV)

    // Calculate tip of the red line (the handle)
    wxPoint tip = worldToScreen(L.x + dirLen * sin(yawRad), 
                                L.y + dirLen * cos(yawRad), w, h, scale);

    // Calculate corners of the cone base
    wxPoint coneLeft = worldToScreen(L.x + dirLen * sin(yawRad - coneAngle), 
                                     L.y + dirLen * cos(yawRad - coneAngle), w, h, scale);
    wxPoint coneRight = worldToScreen(L.x + dirLen * sin(yawRad + coneAngle), 
                                      L.y + dirLen * cos(yawRad + coneAngle), w, h, scale);

    // --- DRAW CONE (First, so it's behind the head) ---
    wxPoint conePoly[3] = { lp, coneLeft, coneRight };
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(255, 0, 0, 80))); // Semi-transparent Red
    dc.DrawPolygon(3, conePoly);

    // --- DRAW DIRECTION LINE ---
    dc.SetPen(wxPen(*wxRED, 2));
    dc.DrawLine(lp, tip);

    // --- DRAW HANDLE (Red Circle at tip) ---
    dc.SetBrush(*wxRED_BRUSH);
    dc.DrawCircle(tip, 4);

    // --- DRAW LISTENER HEAD (Blue Circle) ---
    dc.SetPen(*wxBLUE_PEN);
    dc.SetBrush(*wxBLUE_BRUSH);
    dc.DrawCircle(lp, 6);
    dc.DrawText("Listener", lp.x + 8, lp.y - 8); 

    // 5. Draw Distance Lines (if dragging)
    if (m_mouseDown && m_selectedSpeaker >= 0)
    {
        wxPoint pSel = worldToScreen(m_data->speakerPositions[m_selectedSpeaker].x, 
                                     m_data->speakerPositions[m_selectedSpeaker].y, w, h, scale);
        
        dc.SetPen(wxPen(wxColour(200, 200, 200), 1, wxPENSTYLE_DOT));
        
        for(int i=0; i<CHANNEL_COUNT; ++i) {
            if(i == m_selectedSpeaker) continue;
            
            wxPoint pTarget = worldToScreen(m_data->speakerPositions[i].x, 
                                            m_data->speakerPositions[i].y, w, h, scale);
            
            dc.DrawLine(pSel, pTarget);
            
            float dist = std::hypot(m_data->speakerPositions[i].x - m_data->speakerPositions[m_selectedSpeaker].x,
                                    m_data->speakerPositions[i].y - m_data->speakerPositions[m_selectedSpeaker].y);
            
            wxPoint mid = (pSel + pTarget) / 2;
            wxString distStr = wxString::Format("%.2fm", dist);
            wxSize dSize = dc.GetTextExtent(distStr);
            
            dc.SetBrush(wxBrush(wxColour(0,0,0,180))); 
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(mid.x, mid.y, dSize.GetWidth()+2, dSize.GetHeight()+2);
            
            dc.SetTextForeground(*wxWHITE);
            dc.DrawText(distStr, mid.x + 1, mid.y + 1);
            
            dc.SetPen(wxPen(wxColour(200, 200, 200), 1, wxPENSTYLE_DOT)); 
        }
    }
}

void SpeakerPanel::OnLeftDown(wxMouseEvent &evt)
{
    if (!m_data)
        return;

    int w, h;
    GetClientSize(&w, &h);

    float minX, minY, maxX, maxY;
    float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);

    wxPoint mousePos = evt.GetPosition();

    // Listener rotation handle hit-test (tip of red line)
    if (m_allowListenerDrag)
    {
        const float dirLength = 0.5f;

        Point L = m_data->currentListenerPosition;
        float yawRadians = -m_data->listenerYaw * 2.0f * float(M_PI);

        float endX = L.x + dirLength * std::sin(yawRadians);
        float endY = L.y + dirLength * std::cos(yawRadians);

        wxPoint endPoint = worldToScreen(endX, endY, w, h, scale);

        int dx = mousePos.x - endPoint.x;
        int dy = mousePos.y - endPoint.y;
        int dist2 = dx * dx + dy * dy;
        const int handleRadiusPx = 12; // clickable radius around tip

        if (dist2 <= handleRadiusPx * handleRadiusPx)
        {
            m_mouseDown = true;
            m_draggingYaw = true;
            m_draggingListener = false;
            m_dragSpeakerIndex = -1;
            m_selectedSpeaker = -1;
            CaptureMouse();
            Refresh();
            return;
        }
    }

    // Listener position hit-test (circle) — only if allowed to drag head
    if (m_allowListenerDrag)
    {
        Point L = m_data->currentListenerPosition;
        wxPoint lp = worldToScreen(L.x, L.y, w, h, scale);

        int dx = mousePos.x - lp.x;
        int dy = mousePos.y - lp.y;
        int dist2 = dx * dx + dy * dy;
        const int listenerRadiusPx = 10;

        if (dist2 <= listenerRadiusPx * listenerRadiusPx)
        {
            m_mouseDown = true;
            m_draggingListener = true;
            m_draggingYaw = false;
            m_dragSpeakerIndex = -1;
            m_selectedSpeaker = -1;
            CaptureMouse();
            Refresh();
            return;
        }
    }

    // Speaker hit-test (always allowed)
    // Speaker hit-test (always allowed)
    const int speakerBoxSize = 36;
    int halfBox = speakerBoxSize / 2;

    for (int i = 0; i < CHANNEL_COUNT; ++i)
    {
        Point sp = m_data->speakerPositions[i];
        wxPoint p = worldToScreen(sp.x, sp.y, w, h, scale);

        wxRect rect(p.x - halfBox, p.y - halfBox,
                    speakerBoxSize, speakerBoxSize);

        if (rect.Contains(mousePos))
        {
            m_mouseDown = true;
            m_draggingListener = false;
            m_draggingYaw = false;
            m_dragSpeakerIndex = i;
            m_selectedSpeaker = i;
            CaptureMouse();
            Refresh();
            return;
        }
    }
}

void SpeakerPanel::OnMouseMove(wxMouseEvent &evt)
{
    if (!m_data)
        return;

    int w, h;
    GetClientSize(&w, &h);

    float minX, minY, maxX, maxY;
    float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);

    if (minX > maxX)
        std::swap(minX, maxX);
    if (minY > maxY)
        std::swap(minY, maxY);

    wxPoint mousePos = evt.GetPosition();

    // Hover behaviour: change cursor when near the rotation handle (tip)
    if (!m_mouseDown && m_allowListenerDrag)
    {
        const float dirLength = 0.5f;

        Point L = m_data->currentListenerPosition;
        float yawRadians = -m_data->listenerYaw * 2.0f * float(M_PI);

        float endX = L.x + dirLength * std::sin(yawRadians);
        float endY = L.y + dirLength * std::cos(yawRadians);

        wxPoint endPoint = worldToScreen(endX, endY, w, h, scale);

        int dx = mousePos.x - endPoint.x;
        int dy = mousePos.y - endPoint.y;
        int dist2 = dx * dx + dy * dy;
        const int handleRadiusPx = 12;

        if (dist2 <= handleRadiusPx * handleRadiusPx)
        {
            // Horizontal double-arrow cursor so user knows they can rotate
            SetCursor(wxCursor(wxCURSOR_SIZEWE));
        }
        else
        {
            SetCursor(wxNullCursor);
        }

        return; // no drag happening
    }

    // Dragging behaviour
    if (!m_mouseDown)
        return;

    Point world = screenToWorld(mousePos.x, mousePos.y, w, h, scale);

    // Clamp to room bounds
    world.x = std::max(minX, std::min(maxX, world.x));
    world.y = std::max(minY, std::min(maxY, world.y));

    // Dragging yaw (rotation) – interactive mode only
    if (m_draggingYaw && m_allowListenerDrag)
    {
        Point L = m_data->currentListenerPosition;

        float vx = world.x - L.x;
        float vy = world.y - L.y;
        float len2 = vx * vx + vy * vy;

        if (len2 > 1e-6f)
        {
            // Angle φ with 0 at "up" (0, +1), π at "down" (0, -1)
            // Note: atan2(x, y) gives that convention (unusual order but convenient)
            float phi = std::atan2(vx, vy); // [-π, π]

            // listenerYaw in [0,1): 0 = up, 0.5 = down
            float yaw = -phi / (2.0f * float(M_PI)); // raw
            if (yaw < 0.0f)
                yaw += 1.0f;
            if (yaw >= 1.0f)
                yaw -= 1.0f;

            m_data->listenerYaw = yaw;
        }

        Refresh();
        return;
    }

    // Dragging listener position
    if (m_draggingListener && m_allowListenerDrag)
    {
        m_data->currentListenerPosition = world;
        Refresh();
        return;
    }

    // Dragging speaker position
    if (m_dragSpeakerIndex >= 0 &&
        m_dragSpeakerIndex < CHANNEL_COUNT)
    {
        m_data->speakerPositions[m_dragSpeakerIndex] = world;
        Refresh();
        return;
    }
}

void SpeakerPanel::OnLeftUp(wxMouseEvent &evt)
{
    m_mouseDown = false;
    m_draggingListener = false;
    m_draggingYaw = false;
    m_dragSpeakerIndex = -1;
    m_selectedSpeaker = -1;

    if (HasCapture())
        ReleaseMouse();

    SetCursor(wxNullCursor);
    Refresh();
}

void SpeakerPanel::OnMouseLeave(wxMouseEvent &evt)
{
    if (m_mouseDown && HasCapture())
        ReleaseMouse();

    m_mouseDown = false;
    m_draggingListener = false;
    m_draggingYaw = false;
    m_dragSpeakerIndex = -1;
    m_selectedSpeaker = -1;

    SetCursor(wxNullCursor);
    Refresh();
}