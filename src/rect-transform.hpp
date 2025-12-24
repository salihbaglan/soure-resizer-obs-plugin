#pragma once

#include <obs.h>
#include <cstdint>

/**
 * Unity-style RectTransform for OBS Scene Items
 * 
 * Coordinate System:
 * - State is stored in Unity-space: Y=0 bottom, Y=1 top (bottom-origin)
 * - When applying to OBS: Y is flipped (OBS uses top-origin)
 * 
 * Key Concepts:
 * - anchorMin/Max: Normalized (0-1) anchor points relative to parent (canvas)
 * - pivot: Object's rotation/position reference point (0-1 local)
 * - anchoredPosition: Offset from anchor pivot point
 * - sizeDelta: Extra size beyond anchor rect
 */
struct RectTransform {
    // Anchor points (Unity-space: 0=bottom, 1=top for Y)
    float anchorMinX = 0.5f;
    float anchorMinY = 0.5f;
    float anchorMaxX = 0.5f;
    float anchorMaxY = 0.5f;
    
    // Pivot point (0-1, local to this rect)
    float pivotX = 0.5f;
    float pivotY = 0.5f;
    
    // Offset from anchor reference point
    float anchoredPosX = 0.0f;
    float anchoredPosY = 0.0f;
    
    // Size delta:
    // - When anchorMin == anchorMax: actual width/height
    // - When anchors differ (stretch): extra size beyond anchor rect
    float sizeDeltaX = 100.0f;
    float sizeDeltaY = 100.0f;
    
    // ===== Core Calculations (Unity-space) =====
    
    /**
     * Calculate final rect in Unity-space (bottom-origin)
     * Returns top-left position and size
     */
    void CalculateFinalRect(float parentW, float parentH,
                           float& outX, float& outY,
                           float& outW, float& outH) const;
    
    /**
     * Get the pivot point in world coordinates (Unity-space)
     */
    void GetPivotWorld(float parentW, float parentH,
                      float& outPivotX, float& outPivotY) const;
    
    // ===== OBS Integration =====
    
    /**
     * Apply this RectTransform to an OBS scene item
     * Handles Unity→OBS Y-axis flip, sets bounds + alignment + position
     */
    void ApplyToSceneItem(obs_sceneitem_t* item,
                          uint32_t canvasW, uint32_t canvasH) const;
    
    /**
     * Save RectTransform state to scene item's private settings
     */
    void SaveToItem(obs_sceneitem_t* item) const;
    
    /**
     * Load RectTransform from scene item's private settings
     * Falls back to inferring from current OBS state if no saved data
     */
    static RectTransform LoadFromItem(obs_sceneitem_t* item,
                                      uint32_t canvasW, uint32_t canvasH);
    
    // ===== Utility =====
    
    /** Check if this is a stretch anchor (min != max) */
    bool IsStretchX() const { return anchorMinX != anchorMaxX; }
    bool IsStretchY() const { return anchorMinY != anchorMaxY; }
    
    /** Get final size for given parent dimensions */
    float GetWidth(float parentW) const;
    float GetHeight(float parentH) const;
};

// ===== Anchor Preset Helper =====

struct AnchorPreset {
    float minX, minY;   // anchorMin (Unity-space: bottom=0)
    float maxX, maxY;   // anchorMax
    float pivotX, pivotY;
    
    /** Create preset from enum values */
    static AnchorPreset FromEnums(int hAlign, int vAlign);
};
