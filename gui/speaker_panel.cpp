// speaker_panel.cpp
#include "speaker_panel.h"

#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>   // REQUIRED for transparency
#include <wx/graphics.h>
#include <wx/image.h>     
#include <wx/filename.h>  
#include <cmath>
#include <algorithm>
#include <vector>

// Fallback for M_PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    // Set internal variable, though OnPaint handles the actual drawing now
    SetBackgroundColour(wxColour(30, 30, 30)); 
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    // --- SETUP IMAGE ---
    if (!wxImage::FindHandler(wxBITMAP_TYPE_PNG))
        wxImage::AddHandler(new wxPNGHandler);

    wxImage iconImage;
    std::vector<wxString> possiblePaths;
    possiblePaths.push_back("assets/icons/speaker.png");       
    possiblePaths.push_back("../assets/icons/speaker.png");    
    possiblePaths.push_back("../../assets/icons/speaker.png"); 
    
    wxString foundPath = "";
    for (const wxString& path : possiblePaths) {
        if (wxFileName::FileExists(path)) {
            foundPath = path;
            break;
        }
    }

    if (!foundPath.IsEmpty() && iconImage.LoadFile(foundPath, wxBITMAP_TYPE_PNG))
    {
        iconImage.Rescale(32, 32, wxIMAGE_QUALITY_HIGH);
        m_speakerBitmap = wxBitmap(iconImage);
    }

    // --- CAPTURE POSITIONS ---
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
    m_allowListenerDrag = interactive;
    m_mouseDown = false;
    m_draggingListener = false;
    m_draggingYaw = false;
    m_dragSpeakerIndex = -1;
    m_selectedSpeaker = -1;
    if (HasCapture()) ReleaseMouse();
    Refresh();
}

void SpeakerPanel::ResetPositions()
{
    if (!m_data) return;
    m_data->currentListenerPosition = m_initialListenerPosition;
    m_data->listenerYaw = m_initialListenerYaw;

    for (int i = 0; i < CHANNEL_COUNT; ++i)
        m_data->speakerPositions[i] = m_initialSpeakerPositions[i];

    m_mouseDown = false;
    m_draggingListener = false;
    m_draggingYaw = false;
    m_dragSpeakerIndex = -1;
    m_selectedSpeaker = -1;
    if (HasCapture()) ReleaseMouse();
    Refresh();
}

float SpeakerPanel::computeScaleForCurrentBounds(int w, int h, float &minX, float &minY, float &maxX, float &maxY) const
{
    if (!m_data) { minX = minY = -1.0f; maxX = maxY = 1.0f; return 1.0f; }
    minX = m_data->subjectBounds[0].x;
    minY = m_data->subjectBounds[0].y;
    maxX = m_data->subjectBounds[1].x;
    maxY = m_data->subjectBounds[1].y;

    const int margin = 20;
    float maxAbsX = std::max(std::fabs(minX), std::fabs(maxX));
    float maxAbsY = std::max(std::fabs(minY), std::fabs(maxY));

    if (maxAbsX < 0.001f) maxAbsX = 1.0f;
    if (maxAbsY < 0.001f) maxAbsY = 1.0f;

    float scaleX = (w / 2.0f - margin) / maxAbsX;
    float scaleY = (h / 2.0f - margin) / maxAbsY;
    float scale = (scaleX < scaleY ? scaleX : scaleY);
    if (scale <= 0.0f) scale = 1.0f;
    return scale;
}

wxPoint SpeakerPanel::worldToScreen(float x, float y, int w, int h, float scale) const
{
    float cx = w * 0.5f;
    float cy = h * 0.5f;
    float sx = cx + x * scale;
    float sy = cy - y * scale; 
    return wxPoint((int)sx, (int)sy);
}

Point SpeakerPanel::screenToWorld(int px, int py, int w, int h, float scale) const
{
    float cx = w * 0.5f;
    float cy = h * 0.5f;
    float x = (px - cx) / scale;
    float y = (cy - py) / scale; 
    return Point{x, y};
}

void SpeakerPanel::OnPaint(wxPaintEvent &event)
{
    wxAutoBufferedPaintDC dc(this);
    
    // Enable GCDC for transparency
    wxGCDC gdc(dc); 

    // --- FIX: Explicitly set the background color before clearing ---
    wxColour darkGrey(30, 30, 30);
    gdc.SetBackground(wxBrush(darkGrey));
    gdc.Clear();
    // -------------------------------------------------------------

    if (!m_data) return;

    int w, h;
    GetClientSize(&w, &h);

    float minX, minY, maxX, maxY;
    float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);

    // 1. Draw Room Bounds
    gdc.SetPen(wxPen(wxColour(255, 255, 255), 3));
    gdc.SetBrush(*wxTRANSPARENT_BRUSH);
    wxPoint topLeft = worldToScreen(minX, maxY, w, h, scale);
    wxPoint bottomRight = worldToScreen(maxX, minY, w, h, scale);
    gdc.DrawRectangle(topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);

    // 2. Setup Dimensions
    const int iconDisplaySize = 32; 
    const int gap = 2;
    const int sliderHeight = 8;
    const int sliderWidth = 32;
    
    wxFont mainFont = gdc.GetFont();
    wxFont smallFont = mainFont;
    smallFont.SetPointSize(std::max(7, mainFont.GetPointSize() - 2));

    // 3. Draw Speakers
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
        gdc.SetFont(smallFont);
        gdc.SetTextForeground(*wxWHITE);
        wxString nameStr;
        nameStr.Printf("Ch %d", i);
        wxSize nameSize = gdc.GetTextExtent(nameStr);
        gdc.DrawText(nameStr, centerX - (nameSize.GetWidth() / 2), iconTop - gap - nameSize.GetHeight());

        // B. Icon
        if (m_speakerBitmap.IsOk())
        {
            gdc.DrawBitmap(m_speakerBitmap, centerX - iconHalf, iconTop, true);
        }
        else
        {
            gdc.SetPen(*wxWHITE_PEN);
            gdc.SetBrush(*wxTRANSPARENT_BRUSH);
            gdc.DrawRectangle(centerX - iconHalf, iconTop, iconDisplaySize, iconDisplaySize);
        }

        // C. Volume Ramp (Background)
        int triTop = iconBottom + gap;
        int triBottom = triTop + sliderHeight;
        int triLeft = centerX - (sliderWidth / 2);
        int triRight = centerX + (sliderWidth / 2);

        wxPoint rampPoints[3];
        rampPoints[0] = wxPoint(triLeft, triBottom);
        rampPoints[1] = wxPoint(triRight, triBottom);
        rampPoints[2] = wxPoint(triRight, triTop);

        gdc.SetPen(*wxTRANSPARENT_PEN);
        gdc.SetBrush(wxBrush(wxColour(80, 80, 80))); 
        gdc.DrawPolygon(3, rampPoints);

        // Active Volume (Green)
        if (vol > 0.001f)
        {
            int fillWidth = (int)(sliderWidth * vol);
            if (fillWidth > sliderWidth) fillWidth = sliderWidth;
            
            gdc.SetClippingRegion(triLeft, triTop, fillWidth, sliderHeight + 1);
            int greenVal = (int)(100 + 155 * vol);
            if (greenVal > 255) greenVal = 255;
            gdc.SetBrush(wxBrush(wxColour(50, greenVal, 50))); 
            gdc.DrawPolygon(3, rampPoints); 
            gdc.DestroyClippingRegion();
        }

        // D. Number
        wxString volStr;
        volStr.Printf("%.2f", vol);
        wxSize volSize = gdc.GetTextExtent(volStr);
        gdc.DrawText(volStr, centerX - (volSize.GetWidth() / 2), triBottom + gap);
    }

    gdc.SetFont(mainFont);

    // 4. Draw Listener & FOV Cone
    Point L = m_data->currentListenerPosition;
    wxPoint lp = worldToScreen(L.x, L.y, w, h, scale);

    float yawRad = -m_data->listenerYaw * 2.0f * M_PI;
    const float dirLen = 0.5f;     
    const float coneAngle = 20.0f * (M_PI / 180.0f); 

    wxPoint tip = worldToScreen(L.x + dirLen * sin(yawRad), 
                                L.y + dirLen * cos(yawRad), w, h, scale);

    wxPoint coneLeft = worldToScreen(L.x + dirLen * sin(yawRad - coneAngle), 
                                     L.y + dirLen * cos(yawRad - coneAngle), w, h, scale);
    wxPoint coneRight = worldToScreen(L.x + dirLen * sin(yawRad + coneAngle), 
                                      L.y + dirLen * cos(yawRad + coneAngle), w, h, scale);

    // --- DRAW CONE ---
    wxPoint conePoly[3] = { lp, coneLeft, coneRight };
    gdc.SetPen(*wxTRANSPARENT_PEN);
    gdc.SetBrush(wxBrush(wxColour(255, 0, 0, 80))); // Semi-transparent Red
    gdc.DrawPolygon(3, conePoly);

    // --- DRAW DIRECTION LINE ---
    gdc.SetPen(wxPen(*wxRED, 2));
    gdc.DrawLine(lp, tip);

    gdc.SetBrush(*wxRED_BRUSH);
    gdc.DrawCircle(tip, 4);

    gdc.SetPen(*wxBLUE_PEN);
    gdc.SetBrush(*wxBLUE_BRUSH);
    gdc.DrawCircle(lp, 6);
    gdc.DrawText("Listener", lp.x + 8, lp.y - 8); 

    // 5. Draw Distance Lines
    if (m_mouseDown && m_selectedSpeaker >= 0)
    {
        wxPoint pSel = worldToScreen(m_data->speakerPositions[m_selectedSpeaker].x, 
                                     m_data->speakerPositions[m_selectedSpeaker].y, w, h, scale);
        
        gdc.SetPen(wxPen(wxColour(200, 200, 200), 1, wxPENSTYLE_DOT));
        
        for(int i=0; i<CHANNEL_COUNT; ++i) {
            if(i == m_selectedSpeaker) continue;
            
            wxPoint pTarget = worldToScreen(m_data->speakerPositions[i].x, 
                                            m_data->speakerPositions[i].y, w, h, scale);
            
            gdc.DrawLine(pSel, pTarget);
            
            float dist = std::hypot(m_data->speakerPositions[i].x - m_data->speakerPositions[m_selectedSpeaker].x,
                                    m_data->speakerPositions[i].y - m_data->speakerPositions[m_selectedSpeaker].y);
            
            wxPoint mid = (pSel + pTarget) / 2;
            wxString distStr = wxString::Format("%.2fm", dist);
            wxSize dSize = gdc.GetTextExtent(distStr);
            
            gdc.SetBrush(wxBrush(wxColour(0,0,0,180))); 
            gdc.SetPen(*wxTRANSPARENT_PEN);
            gdc.DrawRectangle(mid.x, mid.y, dSize.GetWidth()+2, dSize.GetHeight()+2);
            
            gdc.SetTextForeground(*wxWHITE);
            gdc.DrawText(distStr, mid.x + 1, mid.y + 1);
            
            gdc.SetPen(wxPen(wxColour(200, 200, 200), 1, wxPENSTYLE_DOT)); 
        }
    }
}

void SpeakerPanel::OnLeftDown(wxMouseEvent &evt)
{
    if (!m_data) return;
    int w, h; GetClientSize(&w, &h);
    float minX, minY, maxX, maxY;
    float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);
    wxPoint mousePos = evt.GetPosition();

    // Listener Handle
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
        if ((dx * dx + dy * dy) <= 144) 
        {
            m_mouseDown = true; m_draggingYaw = true; m_draggingListener = false;
            m_dragSpeakerIndex = -1; m_selectedSpeaker = -1;
            CaptureMouse(); Refresh(); return;
        }
    }

    // Listener Body
    if (m_allowListenerDrag)
    {
        Point L = m_data->currentListenerPosition;
        wxPoint lp = worldToScreen(L.x, L.y, w, h, scale);
        int dx = mousePos.x - lp.x;
        int dy = mousePos.y - lp.y;
        if ((dx * dx + dy * dy) <= 100) 
        {
            m_mouseDown = true; m_draggingListener = true; m_draggingYaw = false;
            m_dragSpeakerIndex = -1; m_selectedSpeaker = -1;
            CaptureMouse(); Refresh(); return;
        }
    }

    // Speakers
    const int speakerBoxSize = 36;
    for (int i = 0; i < CHANNEL_COUNT; ++i)
    {
        Point sp = m_data->speakerPositions[i];
        wxPoint p = worldToScreen(sp.x, sp.y, w, h, scale);
        wxRect rect(p.x - speakerBoxSize/2, p.y - speakerBoxSize/2, speakerBoxSize, speakerBoxSize);

        if (rect.Contains(mousePos))
        {
            m_mouseDown = true; m_draggingListener = false; m_draggingYaw = false;
            m_dragSpeakerIndex = i; m_selectedSpeaker = i;
            CaptureMouse(); Refresh(); return;
        }
    }
}

void SpeakerPanel::OnMouseMove(wxMouseEvent &evt)
{
    if (!m_data) return;
    int w, h; GetClientSize(&w, &h);
    float minX, minY, maxX, maxY;
    float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);
    if (minX > maxX) std::swap(minX, maxX);
    if (minY > maxY) std::swap(minY, maxY);
    wxPoint mousePos = evt.GetPosition();

    if (!m_mouseDown && m_allowListenerDrag)
    {
        Point L = m_data->currentListenerPosition;
        float yawRadians = -m_data->listenerYaw * 2.0f * float(M_PI);
        wxPoint endPoint = worldToScreen(L.x + 0.5f * std::sin(yawRadians), 
                                         L.y + 0.5f * std::cos(yawRadians), w, h, scale);
        int dx = mousePos.x - endPoint.x;
        int dy = mousePos.y - endPoint.y;
        if ((dx*dx + dy*dy) <= 144) SetCursor(wxCursor(wxCURSOR_SIZEWE));
        else SetCursor(wxNullCursor);
        return; 
    }

    if (!m_mouseDown) return;

    Point world = screenToWorld(mousePos.x, mousePos.y, w, h, scale);
    world.x = std::max(minX, std::min(maxX, world.x));
    world.y = std::max(minY, std::min(maxY, world.y));

    if (m_draggingYaw && m_allowListenerDrag)
    {
        Point L = m_data->currentListenerPosition;
        float vx = world.x - L.x;
        float vy = world.y - L.y;
        if (vx*vx + vy*vy > 1e-6f)
        {
            float phi = std::atan2(vx, vy); 
            float yaw = -phi / (2.0f * float(M_PI)); 
            if (yaw < 0.0f) yaw += 1.0f;
            if (yaw >= 1.0f) yaw -= 1.0f;
            m_data->listenerYaw = yaw;
        }
        Refresh(); return;
    }
    if (m_draggingListener && m_allowListenerDrag)
    {
        m_data->currentListenerPosition = world; Refresh(); return;
    }
    if (m_dragSpeakerIndex >= 0 && m_dragSpeakerIndex < CHANNEL_COUNT)
    {
        m_data->speakerPositions[m_dragSpeakerIndex] = world; Refresh(); return;
    }
}

void SpeakerPanel::OnLeftUp(wxMouseEvent &evt)
{
    m_mouseDown = false; m_draggingListener = false; m_draggingYaw = false;
    m_dragSpeakerIndex = -1; m_selectedSpeaker = -1;
    if (HasCapture()) ReleaseMouse();
    SetCursor(wxNullCursor); Refresh();
}

void SpeakerPanel::OnMouseLeave(wxMouseEvent &evt)
{
    if (m_mouseDown && HasCapture()) ReleaseMouse();
    m_mouseDown = false; m_draggingListener = false; m_draggingYaw = false;
    m_dragSpeakerIndex = -1; m_selectedSpeaker = -1;
    SetCursor(wxNullCursor); Refresh();
}