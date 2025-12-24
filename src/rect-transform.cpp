#include "rect-transform.hpp"
#include <cmath>
#include <algorithm>

// ===== Core Calculations =====

void RectTransform::CalculateFinalRect(float parentW, float parentH,
                                       float& outX, float& outY,
                                       float& outW, float& outH) const
{
    // 1. Anchor rect in pixels (Unity-space, bottom-origin)
    float ax0 = parentW * anchorMinX;
    float ay0 = parentH * anchorMinY;
    float ax1 = parentW * anchorMaxX;
    float ay1 = parentH * anchorMaxY;
    float anchorRectW = ax1 - ax0;
    float anchorRectH = ay1 - ay0;
    
    // 2. Final size = anchor rect size + sizeDelta
    outW = anchorRectW + sizeDeltaX;
    outH = anchorRectH + sizeDeltaY;
    
    // Clamp to minimum size
    outW = std::max(1.0f, outW);
    outH = std::max(1.0f, outH);
    
    // 3. Anchor pivot point (reference point in parent space)
    //    This is where anchoredPosition offsets from
    float anchorPivotX = ax0 + anchorRectW * pivotX;
    float anchorPivotY = ay0 + anchorRectH * pivotY;
    
    // 4. Final position (rect min corner, Unity-space)
    //    pos = anchorPivot + anchoredPos - (size * pivot)
    outX = anchorPivotX + anchoredPosX - (outW * pivotX);
    outY = anchorPivotY + anchoredPosY - (outH * pivotY);
}

void RectTransform::GetPivotWorld(float parentW, float parentH,
                                  float& outPivotX, float& outPivotY) const
{
    float x, y, w, h;
    CalculateFinalRect(parentW, parentH, x, y, w, h);
    
    outPivotX = x + w * pivotX;
    outPivotY = y + h * pivotY;
}

float RectTransform::GetWidth(float parentW) const
{
    float anchorRectW = parentW * (anchorMaxX - anchorMinX);
    return std::max(1.0f, anchorRectW + sizeDeltaX);
}

float RectTransform::GetHeight(float parentH) const
{
    float anchorRectH = parentH * (anchorMaxY - anchorMinY);
    return std::max(1.0f, anchorRectH + sizeDeltaY);
}

// ===== OBS Integration =====

void RectTransform::ApplyToSceneItem(obs_sceneitem_t* item,
                                     uint32_t canvasW, uint32_t canvasH) const
{
    if (!item) return;
    
    float posX, posY, w, h;
    CalculateFinalRect((float)canvasW, (float)canvasH, posX, posY, w, h);
    
    // Calculate pivot world point in Unity-space
    float pivotWorldX = posX + w * pivotX;
    float pivotWorldY = posY + h * pivotY;
    
    // Unity → OBS Y flip
    // Unity: Y=0 bottom, Y increases upward
    // OBS: Y=0 top, Y increases downward
    float obsPivotY = (float)canvasH - pivotWorldY;
    
    // Alignment from pivot (3x3 grid quantization)
    // OBS alignment determines which point of the item 'pos' refers to
    uint32_t align = 0;
    
    // Horizontal alignment
    if (pivotX < 0.25f) {
        align |= OBS_ALIGN_LEFT;
    } else if (pivotX > 0.75f) {
        align |= OBS_ALIGN_RIGHT;
    }
    // else: center (no flag = center)
    
    // Vertical alignment (already accounting for Y flip in pivot meaning)
    // In Unity: pivotY=0 means bottom, pivotY=1 means top
    // In OBS: bottom alignment = item's bottom edge at pos
    if (pivotY < 0.25f) {
        align |= OBS_ALIGN_BOTTOM;  // Unity bottom → OBS bottom
    } else if (pivotY > 0.75f) {
        align |= OBS_ALIGN_TOP;     // Unity top → OBS top
    }
    // else: middle (no flag = center)
    
    obs_sceneitem_set_alignment(item, align);
    
    // Position (pivot point in OBS coordinates)
    vec2 pos;
    pos.x = pivotWorldX;
    pos.y = obsPivotY;
    obs_sceneitem_set_pos(item, &pos);
    
    // Size via bounds (more flexible than scale)
    obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_STRETCH);
    obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
    
    vec2 bounds;
    bounds.x = w;
    bounds.y = h;
    obs_sceneitem_set_bounds(item, &bounds);
    
    // Persist state
    SaveToItem(item);
}

void RectTransform::SaveToItem(obs_sceneitem_t* item) const
{
    if (!item) return;
    
    obs_data_t* settings = obs_sceneitem_get_private_settings(item);
    if (!settings) return;
    
    obs_data_set_double(settings, "rt_anchorMinX", anchorMinX);
    obs_data_set_double(settings, "rt_anchorMinY", anchorMinY);
    obs_data_set_double(settings, "rt_anchorMaxX", anchorMaxX);
    obs_data_set_double(settings, "rt_anchorMaxY", anchorMaxY);
    obs_data_set_double(settings, "rt_pivotX", pivotX);
    obs_data_set_double(settings, "rt_pivotY", pivotY);
    obs_data_set_double(settings, "rt_anchoredPosX", anchoredPosX);
    obs_data_set_double(settings, "rt_anchoredPosY", anchoredPosY);
    obs_data_set_double(settings, "rt_sizeDeltaX", sizeDeltaX);
    obs_data_set_double(settings, "rt_sizeDeltaY", sizeDeltaY);
    
    obs_data_release(settings);
}

RectTransform RectTransform::LoadFromItem(obs_sceneitem_t* item,
                                          uint32_t parentW, uint32_t parentH)
{
    RectTransform rt;
    if (!item) return rt;
    
    // Default to Center-Middle if no settings found
    rt.anchorMinX = rt.anchorMaxX = 0.5f;
    rt.anchorMinY = rt.anchorMaxY = 0.5f;
    rt.pivotX = 0.5f;
    rt.pivotY = 0.5f;
    
    obs_data_t* settings = obs_sceneitem_get_private_settings(item);
    if (settings) {
        if (obs_data_has_user_value(settings, "rt_anchorMinX")) {
            rt.anchorMinX = (float)obs_data_get_double(settings, "rt_anchorMinX");
            rt.anchorMinY = (float)obs_data_get_double(settings, "rt_anchorMinY");
            rt.anchorMaxX = (float)obs_data_get_double(settings, "rt_anchorMaxX");
            rt.anchorMaxY = (float)obs_data_get_double(settings, "rt_anchorMaxY");
            rt.pivotX = (float)obs_data_get_double(settings, "rt_pivotX");
            rt.pivotY = (float)obs_data_get_double(settings, "rt_pivotY");
            // We ignore stored anchoredPos/sizeDelta to support reparenting/external moves
            // We will recalculate them below from actual OBS state
        } else {
             // Fallback inference for pivot if not stored
             uint32_t align = obs_sceneitem_get_alignment(item);
             
             if (align & OBS_ALIGN_LEFT) rt.pivotX = 0.0f;
             else if (align & OBS_ALIGN_RIGHT) rt.pivotX = 1.0f;
             else rt.pivotX = 0.5f;

             if (align & OBS_ALIGN_TOP) rt.pivotY = 1.0f;      // OBS top → Unity top
             else if (align & OBS_ALIGN_BOTTOM) rt.pivotY = 0.0f;  // OBS bottom → Unity bottom
             else rt.pivotY = 0.5f;
        }
        obs_data_release(settings);
    }
    
    // Calculate current World Rect (Unity-space) from OBS Item
    obs_source_t* source = obs_sceneitem_get_source(item);
    float itemW = 0.0f, itemH = 0.0f;
    
    if (obs_sceneitem_get_bounds_type(item) != OBS_BOUNDS_NONE) {
        vec2 bounds;
        obs_sceneitem_get_bounds(item, &bounds);
        itemW = bounds.x;
        itemH = bounds.y;
    } else if (source) {
        vec2 scale;
        obs_sceneitem_get_scale(item, &scale);
        itemW = (float)obs_source_get_width(source) * scale.x;
        itemH = (float)obs_source_get_height(source) * scale.y;
    }
    
    vec2 pos;
    obs_sceneitem_get_pos(item, &pos);
    
    // OBS Pos is the Pivot Point in world space (OBS coords)
    // Convert to Unity Pivot World Point
    float pivotWorldX = pos.x;
    float pivotWorldY = (float)parentH - pos.y; // Flip Y
    
    // Determine Top-Left (Unity: Top-Left is minX, maxY... wait. 
    // Unity coords: Bottom-Left is 0,0. Top-Right is W,H. 
    // So Top-Left is x, y+h? No.
    // Let's stick to our CalculateFinalRect logic: 
    // outX, outY is the BOTTOM-LEFT corner of the rect (min coords).
    
    float rectLeft = pivotWorldX - (itemW * rt.pivotX);
    float rectBottom = pivotWorldY - (itemH * rt.pivotY);
    
    // Now Reverse Engineer anchoredPos and sizeDelta
    
    // 1. Anchor Rect (Unity space)
    float ax0 = (float)parentW * rt.anchorMinX;
    float ay0 = (float)parentH * rt.anchorMinY;
    float ax1 = (float)parentW * rt.anchorMaxX;
    float ay1 = (float)parentH * rt.anchorMaxY;
    float anchorRectW = ax1 - ax0;
    float anchorRectH = ay1 - ay0;
    
    // 2. sizeDelta
    rt.sizeDeltaX = itemW - anchorRectW;
    rt.sizeDeltaY = itemH - anchorRectH;
    
    // 3. Anchored Position
    // The formula for World Position (Rect Bottom-Left):
    // outX = anchorPivotX + anchoredPosX - (outW * pivotX)
    // anchoredPosX = outX - anchorPivotX + (outW * pivotX)
    //
    // Also anchorPivot is:
    // anchorPivotX = ax0 + anchorRectW * pivotX
    
    float anchorPivotX = ax0 + anchorRectW * rt.pivotX;
    float anchorPivotY = ay0 + anchorRectH * rt.pivotY;
    
    // We calculated rectLeft (outX) and rectBottom (outY) above
    rt.anchoredPosX = rectLeft - anchorPivotX + (itemW * rt.pivotX);
    rt.anchoredPosY = rectBottom - anchorPivotY + (itemH * rt.pivotY);
    
    return rt;
}

// ===== Anchor Preset =====

AnchorPreset AnchorPreset::FromEnums(int hAlign, int vAlign)
{
    AnchorPreset p;
    
    // Horizontal: 0=Left, 1=Center, 2=Right, 3=Stretch
    switch (hAlign) {
        case 0: // Left
            p.minX = p.maxX = 0.0f;
            p.pivotX = 0.0f;
            break;
        case 1: // Center
            p.minX = p.maxX = 0.5f;
            p.pivotX = 0.5f;
            break;
        case 2: // Right
            p.minX = p.maxX = 1.0f;
            p.pivotX = 1.0f;
            break;
        case 3: // Stretch
            p.minX = 0.0f;
            p.maxX = 1.0f;
            p.pivotX = 0.5f;
            break;
        default:
            p.minX = p.maxX = 0.5f;
            p.pivotX = 0.5f;
    }
    
    // Vertical: 0=Top, 1=Middle, 2=Bottom, 3=Stretch
    // Remember: Unity-space, Y=0 is bottom, Y=1 is top
    switch (vAlign) {
        case 0: // Top (Unity Y=1)
            p.minY = p.maxY = 1.0f;
            p.pivotY = 1.0f;
            break;
        case 1: // Middle
            p.minY = p.maxY = 0.5f;
            p.pivotY = 0.5f;
            break;
        case 2: // Bottom (Unity Y=0)
            p.minY = p.maxY = 0.0f;
            p.pivotY = 0.0f;
            break;
        case 3: // Stretch
            p.minY = 0.0f;
            p.maxY = 1.0f;
            p.pivotY = 0.5f;
            break;
        default:
            p.minY = p.maxY = 0.5f;
            p.pivotY = 0.5f;
    }
    
    return p;
}
