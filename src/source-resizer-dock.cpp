#include "source-resizer-dock.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QMetaObject>
#include <QTimer>
#include <QKeyEvent>
#include <QApplication>
#include <QGridLayout>
#include <QStackedLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <functional>
#include <utility>
#include "anchor-button.hpp"
#include "rect-transform.hpp"

// Global callback wrapper
static void frontend_event_callback(enum obs_frontend_event event, void *param)
{
    SourceResizerDock *dock = reinterpret_cast<SourceResizerDock*>(param);
    dock->HandleFrontendEvent(event);
}

// Helper for recursive enumeration of selected items
struct EnumContext {
    std::function<void(obs_sceneitem_t*, uint32_t, uint32_t)> callback;
};

static void EnumRecurse(obs_scene_t *scene, uint32_t pW, uint32_t pH, EnumContext *ctx) {
    struct Params {
        EnumContext *ctx;
        uint32_t pW, pH;
    } p = { ctx, pW, pH };

    obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
        Params *pp = (Params*)param;
        if (obs_sceneitem_selected(item)) {
            pp->ctx->callback(item, pp->pW, pp->pH);
        }
        if (obs_sceneitem_is_group(item)) {
             obs_scene_t *gScene = obs_sceneitem_group_get_scene(item);
             if (gScene) {
                 obs_source_t *gs = obs_sceneitem_get_source(item);
                 uint32_t gw = obs_source_get_width(gs);
                 uint32_t gh = obs_source_get_height(gs);
                 EnumRecurse(gScene, gw, gh, pp->ctx);
             }
        }
        return true;
    }, &p);
}

// 1. New version with Parent Dimensions
static void EnumSelectedItemsRecursive(obs_scene_t *scene, std::function<void(obs_sceneitem_t*, uint32_t, uint32_t)> callback) {
    if (!scene) return;
    obs_source_t *s = obs_scene_get_source(scene);
    uint32_t w = obs_source_get_width(s);
    uint32_t h = obs_source_get_height(s);
    EnumContext ctx = { callback };
    EnumRecurse(scene, w, h, &ctx);
}

// 2. Legacy overload for existing code
static void EnumSelectedItemsRecursive(obs_scene_t *scene, std::function<void(obs_sceneitem_t*)> callback) {
    EnumSelectedItemsRecursive(scene, [callback](obs_sceneitem_t *item, uint32_t, uint32_t) {
        callback(item);
    });
}

SourceResizerDock::SourceResizerDock(QWidget *parent) : QWidget(parent)
{
    // Main Stack Layout
    mainStack = new QStackedLayout(this);

    // 1. No Selection Widget
    noSelectionLabel = new QLabel("Select a source to edit", this);
    noSelectionLabel->setAlignment(Qt::AlignCenter);
    noSelectionLabel->setStyleSheet("color: gray; font-style: italic;");
    mainStack->addWidget(noSelectionLabel);

    // 2. Controls Widget
    controlsWidget = new QWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(controlsWidget);
    rootLayout->setContentsMargins(5, 5, 5, 5);
    rootLayout->setSpacing(5);

    // TOP: Name and Visibility
    QHBoxLayout *topLayout = new QHBoxLayout();
    visCheck = new QCheckBox(this);
    visCheck->setToolTip("Toggle Visibility");
    connect(visCheck, &QCheckBox::checkStateChanged, this, &SourceResizerDock::handleVisibility);
    
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText("Source Name");
    connect(nameEdit, &QLineEdit::editingFinished, this, &SourceResizerDock::handleRenaming);

    topLayout->addWidget(visCheck);
    topLayout->addWidget(nameEdit);
    rootLayout->addLayout(topLayout);

    // MIDDLE: Unity RectTransform Area
    QHBoxLayout *mainLayout = new QHBoxLayout();
    mainLayout->setSpacing(10);
    rootLayout->addLayout(mainLayout);

    // LEFT: Main Anchor Button
    mainAnchorBtn = new AnchorButton(AnchorH::Center, AnchorV::Middle, this);
    mainAnchorBtn->setFixedSize(60, 60);
    mainAnchorBtn->setCursor(Qt::PointingHandCursor);
    mainAnchorBtn->setToolTip("Anchor Presets");
    connect(mainAnchorBtn, &QPushButton::clicked, this, &SourceResizerDock::toggleAnchorPopup);
    
    // Container for Left side to align top
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(mainAnchorBtn);
    leftLayout->addStretch();
    mainLayout->addLayout(leftLayout);

    // RIGHT: Fields Grid (Pos X, Pos Y, W, H)
    QGridLayout *fieldGrid = new QGridLayout();
    fieldGrid->setSpacing(5);
    
    // Row 0: Labels
    fieldGrid->addWidget(new QLabel("Pos X"), 0, 0);
    fieldGrid->addWidget(new QLabel("Pos Y"), 0, 1);
    
    // Row 1: SpinBoxes
    xSpin = new QSpinBox(this);
    xSpin->setRange(-10000, 10000);
    xSpin->setButtonSymbols(QAbstractSpinBox::NoButtons); 
    
    ySpin = new QSpinBox(this);
    ySpin->setRange(-10000, 10000);
    ySpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

    fieldGrid->addWidget(xSpin, 1, 0);
    fieldGrid->addWidget(ySpin, 1, 1);

    // Row 2: Labels
    fieldGrid->addWidget(new QLabel("Width"), 2, 0);
    fieldGrid->addWidget(new QLabel("Height"), 2, 1);

    // Row 3: SpinBoxes
    widthSpin = new QSpinBox(this);
    widthSpin->setRange(1, 10000);
    widthSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    
    heightSpin = new QSpinBox(this);
    heightSpin->setRange(1, 10000);
    heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

    fieldGrid->addWidget(widthSpin, 3, 0);
    fieldGrid->addWidget(heightSpin, 3, 1);
    
    // Add Row 4
    fieldGrid->setRowStretch(4, 1);

    mainLayout->addLayout(fieldGrid);
    
    mainStack->addWidget(controlsWidget);
    mainStack->setCurrentWidget(noSelectionLabel);

    // Create the Popup (Hidden by default)
    CreateAnchorPopup();

    // Connect input signals
    connect(widthSpin, &QSpinBox::valueChanged, this, &SourceResizerDock::handleResize);
    connect(heightSpin, &QSpinBox::valueChanged, this, &SourceResizerDock::handleResize);
    connect(xSpin, &QSpinBox::valueChanged, this, &SourceResizerDock::handlePositionChange);
    connect(ySpin, &QSpinBox::valueChanged, this, &SourceResizerDock::handlePositionChange);

    // Modifier Timer + Selection Polling (for group children which don't emit signals)
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        updateModifierLabels();
        RefreshFromSelection(); // Poll for selection changes
    });
    timer->start(100);

    // Init OBS
    obs_frontend_add_event_callback(frontend_event_callback, this);
    
    // Initial subscription
    obs_source_t *source = obs_frontend_get_current_scene();
    if (source) {
        obs_scene_t *scene = obs_scene_from_source(source);
        SubscribeToScene(scene);
        obs_source_release(source);
    }
    RefreshFromSelection();
}

void SourceResizerDock::CreateAnchorPopup()
{
    anchorPopup = new QWidget(this, Qt::Popup);
    anchorPopup->setStyleSheet("background-color: #333; border: 1px solid #555;");
    QVBoxLayout *layout = new QVBoxLayout(anchorPopup);
    layout->setContentsMargins(5, 5, 5, 5);
    
    QHBoxLayout *modLayout = new QHBoxLayout();
    shiftLabel = new QLabel("Shift: Pivot", anchorPopup);
    altLabel = new QLabel("Alt: Position", anchorPopup);
    shiftLabel->setStyleSheet("color: gray;");
    altLabel->setStyleSheet("color: gray;");
    modLayout->addWidget(shiftLabel);
    modLayout->addWidget(altLabel);
    layout->addLayout(modLayout);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(2);
    
    AnchorV vRows[] = { AnchorV::Top, AnchorV::Middle, AnchorV::Bottom, AnchorV::Stretch };
    AnchorH hCols[] = { AnchorH::Left, AnchorH::Center, AnchorH::Right, AnchorH::Stretch };

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            AnchorButton *btn = new AnchorButton(hCols[c], vRows[r], anchorPopup);
            connect(btn, &QPushButton::clicked, this, &SourceResizerDock::onAnchorClicked);
            grid->addWidget(btn, r, c);
        }
    }
    layout->addLayout(grid);
}

void SourceResizerDock::toggleAnchorPopup()
{
    if (anchorPopup->isVisible()) {
        anchorPopup->hide();
    } else {
        QPoint p = mainAnchorBtn->mapToGlobal(QPoint(0, mainAnchorBtn->height()));
        anchorPopup->move(p);
        anchorPopup->show();
    }
}

SourceResizerDock::~SourceResizerDock()
{
    UnsubscribeFromScene();
    obs_frontend_remove_event_callback(frontend_event_callback, this);
}

void SourceResizerDock::updateModifierLabels()
{
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    shiftLabel->setStyleSheet(mods & Qt::ShiftModifier ? "color: #00AAFF; font-weight: bold;" : "color: gray;");
    altLabel->setStyleSheet(mods & Qt::AltModifier ? "color: #00AAFF; font-weight: bold;" : "color: gray;");
}

void SourceResizerDock::keyPressEvent(QKeyEvent *event) { updateModifierLabels(); QWidget::keyPressEvent(event); }
void SourceResizerDock::keyReleaseEvent(QKeyEvent *event) { updateModifierLabels(); QWidget::keyReleaseEvent(event); }

void SourceResizerDock::SubscribeToScene(obs_scene_t *scene)
{
    UnsubscribeAll();
    SubscribeRecursive(scene);
}

void SourceResizerDock::SubscribeRecursive(obs_scene_t *scene)
{
    obs_source_t *source = obs_scene_get_source(scene);
    if (!source) return;

    // Check if already tracked
    for (auto *s : trackedSources) {
        if (s == source) return;
    }

    obs_source_get_ref(source);
    trackedSources.push_back(source);

    signal_handler_t *sh = obs_source_get_signal_handler(source);
    if (sh) {
        signal_handler_connect(sh, "item_select", OBSSceneItemSignal, this);
        signal_handler_connect(sh, "item_deselect", OBSSceneItemSignal, this);
        signal_handler_connect(sh, "item_transform", OBSSceneItemSignal, this);
    }
    
    // Recurse into groups
    obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
        if (obs_sceneitem_is_group(item)) {
            obs_scene_t *gScene = obs_sceneitem_group_get_scene(item);
            if (gScene) {
                SourceResizerDock *dock = reinterpret_cast<SourceResizerDock*>(param);
                dock->SubscribeRecursive(gScene);
            }
        }
        return true;
    }, this);
}

void SourceResizerDock::UnsubscribeAll()
{
    for (obs_source_t *source : trackedSources) {
        signal_handler_t *sh = obs_source_get_signal_handler(source);
        if (sh) {
            signal_handler_disconnect(sh, "item_select", OBSSceneItemSignal, this);
            signal_handler_disconnect(sh, "item_deselect", OBSSceneItemSignal, this);
            signal_handler_disconnect(sh, "item_transform", OBSSceneItemSignal, this);
        }
        obs_source_release(source);
    }
    trackedSources.clear();
}

void SourceResizerDock::UnsubscribeFromScene()
{
    UnsubscribeAll();
}

void SourceResizerDock::OBSSceneItemSignal(void *data, calldata_t *cd)
{
    SourceResizerDock *dock = reinterpret_cast<SourceResizerDock*>(data);
    QMetaObject::invokeMethod(dock, "RefreshFromSelection", Qt::QueuedConnection);
}

void SourceResizerDock::HandleFrontendEvent(enum obs_frontend_event event)
{
    if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
        obs_source_t *source = obs_frontend_get_current_scene();
        if (source) {
            obs_scene_t *scene = obs_scene_from_source(source);
            SubscribeToScene(scene);
            obs_source_release(source);
            RefreshFromSelection();
        }
    }
}

void SourceResizerDock::handleRenaming()
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) { obs_source_release(source); return; }

    std::function<void(obs_sceneitem_t*)> action = [&](obs_sceneitem_t *item) {
         obs_source_t *itemSource = obs_sceneitem_get_source(item);
         if (itemSource) {
             obs_source_set_name(itemSource, nameEdit->text().toUtf8().constData());
         }
    };

    EnumSelectedItemsRecursive(scene, action);

    obs_source_release(source);
}

void SourceResizerDock::handleVisibility(int state)
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) { obs_source_release(source); return; }

    bool visible = (state == Qt::Checked);

    std::function<void(obs_sceneitem_t*)> action = [&](obs_sceneitem_t *item) {
         obs_sceneitem_set_visible(item, visible);
    };

    EnumSelectedItemsRecursive(scene, action);

    obs_source_release(source);
}

void SourceResizerDock::RefreshFromSelection()
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) {
        obs_source_release(source);
        return;
    }

    // Ensure we are tracking this scene
    SubscribeToScene(scene);

    obs_sceneitem_t *selectedItem = nullptr;
    uint32_t selectedParentW = 0;
    uint32_t selectedParentH = 0;
    
    // Find first selected item and its parent dims
    EnumSelectedItemsRecursive(scene, [&](obs_sceneitem_t *item, uint32_t pW, uint32_t pH) {
        if (!selectedItem) {
            selectedItem = item;
            selectedParentW = pW;
            selectedParentH = pH;
        }
    });

    // Update UI
    if (selectedItem) {
        mainStack->setCurrentWidget(controlsWidget);

        RectTransform rt = RectTransform::LoadFromItem(selectedItem, selectedParentW, selectedParentH);
        
        // We display Actual Size (visually correct) and Anchored Position (logically correct)
        float displayW = rt.GetWidth((float)selectedParentW);
        float displayH = rt.GetHeight((float)selectedParentH);
        float displayX = rt.anchoredPosX;
        float displayY = rt.anchoredPosY;

        obs_source_t *itemSource = obs_sceneitem_get_source(selectedItem);
        const char* name = itemSource ? obs_source_get_name(itemSource) : "";
        bool visible = obs_sceneitem_visible(selectedItem);

        // Block signals to prevent feedback loop
        widthSpin->blockSignals(true);
        heightSpin->blockSignals(true);
        xSpin->blockSignals(true);
        ySpin->blockSignals(true);
        nameEdit->blockSignals(true);
        visCheck->blockSignals(true);

        widthSpin->setValue((int)displayW);
        heightSpin->setValue((int)displayH);
        xSpin->setValue((int)displayX);
        ySpin->setValue((int)displayY);
        nameEdit->setText(QString::fromUtf8(name));
        visCheck->setChecked(visible);

        widthSpin->blockSignals(false);
        heightSpin->blockSignals(false);
        xSpin->blockSignals(false);
        ySpin->blockSignals(false);
        nameEdit->blockSignals(false);
        visCheck->blockSignals(false);
        
        this->setEnabled(true);
    } else {
        mainStack->setCurrentWidget(noSelectionLabel);
    }

    obs_source_release(source);
}

void SourceResizerDock::handleResize()
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;
    
    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) { obs_source_release(source); return; }

    EnumSelectedItemsRecursive(scene, [&](obs_sceneitem_t *item, uint32_t pW, uint32_t pH) {
        RectTransform rt = RectTransform::LoadFromItem(item, pW, pH);
        
        float targetW = (float)widthSpin->value();
        float targetH = (float)heightSpin->value();
        
        // Calculate needed sizeDelta to achieve target size
        float ax0 = (float)pW * rt.anchorMinX;
        float ay0 = (float)pH * rt.anchorMinY;
        float ax1 = (float)pW * rt.anchorMaxX;
        float ay1 = (float)pH * rt.anchorMaxY;
        float anchorRectW = ax1 - ax0;
        float anchorRectH = ay1 - ay0;
        
        rt.sizeDeltaX = targetW - anchorRectW;
        rt.sizeDeltaY = targetH - anchorRectH;
        
        rt.ApplyToSceneItem(item, pW, pH);
    });

    obs_source_release(source);
}


void SourceResizerDock::handlePositionChange()
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;
    
    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) { obs_source_release(source); return; }

    EnumSelectedItemsRecursive(scene, [&](obs_sceneitem_t *item, uint32_t pW, uint32_t pH) {
        RectTransform rt = RectTransform::LoadFromItem(item, pW, pH);
        
        rt.anchoredPosX = (float)xSpin->value();
        rt.anchoredPosY = (float)ySpin->value();
        
        rt.ApplyToSceneItem(item, pW, pH);
    });

    obs_source_release(source);
}

void SourceResizerDock::onAnchorClicked()
{
    AnchorButton *btn = qobject_cast<AnchorButton*>(sender());
    if (!btn) return;
    
    ApplyAnchorPreset(btn->horizontal(), btn->vertical());
}

void SourceResizerDock::ApplyAnchorPreset(AnchorH h, AnchorV v)
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) { obs_source_release(source); return; }

    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    bool shiftHeld = (mods & Qt::ShiftModifier);
    bool altHeld = (mods & Qt::AltModifier);
    
    // Get preset anchor/pivot values
    AnchorPreset preset = AnchorPreset::FromEnums(static_cast<int>(h), static_cast<int>(v));
    
    // Use the new helper that provides parent dimensions
    EnumSelectedItemsRecursive(scene, [&](obs_sceneitem_t *item, uint32_t parentW, uint32_t parentH) {
        
        // Load current RectTransform state (inferred from live OBS state)
        RectTransform rt = RectTransform::LoadFromItem(item, parentW, parentH);
        
        if (!shiftHeld && !altHeld) {
            // === Normal click: Change anchor, preserve world rect position ===
            float oldX, oldY, oldW, oldH;
            rt.CalculateFinalRect((float)parentW, (float)parentH, oldX, oldY, oldW, oldH);
            float oldPivotWorldX = oldX + oldW * rt.pivotX;
            float oldPivotWorldY = oldY + oldH * rt.pivotY;
            
            // Apply new anchors
            rt.anchorMinX = preset.minX;
            rt.anchorMinY = preset.minY;
            rt.anchorMaxX = preset.maxX;
            rt.anchorMaxY = preset.maxY;
            
            // Calculate new anchor rect
            float ax0 = (float)parentW * rt.anchorMinX;
            float ay0 = (float)parentH * rt.anchorMinY;
            float ax1 = (float)parentW * rt.anchorMaxX;
            float ay1 = (float)parentH * rt.anchorMaxY;
            float anchorRectW = ax1 - ax0;
            float anchorRectH = ay1 - ay0;
            
            // Recalculate sizeDelta to maintain old size
            rt.sizeDeltaX = oldW - anchorRectW;
            rt.sizeDeltaY = oldH - anchorRectH;
            
            // Calculate new anchor pivot point
            float newAnchorPivotX = ax0 + anchorRectW * rt.pivotX;
            float newAnchorPivotY = ay0 + anchorRectH * rt.pivotY;
            
            // Recalculate anchoredPosition to keep pivot at same world pos
            rt.anchoredPosX = oldPivotWorldX - newAnchorPivotX;
            rt.anchoredPosY = oldPivotWorldY - newAnchorPivotY;
            
            rt.ApplyToSceneItem(item, parentW, parentH);
        }
        else if (shiftHeld && !altHeld) {
            // === Shift: Change anchor + move to anchor position ===
            rt.anchorMinX = preset.minX;
            rt.anchorMinY = preset.minY;
            rt.anchorMaxX = preset.maxX;
            rt.anchorMaxY = preset.maxY;
            
            // Reset offset (snap to anchor)
            rt.anchoredPosX = 0.0f;
            rt.anchoredPosY = 0.0f;
            
            rt.ApplyToSceneItem(item, parentW, parentH);
        }
        else if (shiftHeld && altHeld) {
            // === Shift+Alt: Full preset reset (anchor + pivot + pos + size) ===
            rt.anchorMinX = preset.minX;
            rt.anchorMinY = preset.minY;
            rt.anchorMaxX = preset.maxX;
            rt.anchorMaxY = preset.maxY;
            rt.pivotX = preset.pivotX;
            rt.pivotY = preset.pivotY;
            
            // Reset offset
            rt.anchoredPosX = 0.0f;
            rt.anchoredPosY = 0.0f;
            
            // Reset size: For stretch axes, sizeDelta=0 implies filling the anchor rect.
            if (preset.minX != preset.maxX) {
                rt.sizeDeltaX = 0.0f;  // Horizontal stretch
            } else {
                // For fixed axes, reset to 'default' or keep current?
                // Unity resets to 100x100 usually. Let's try to keep meaningful size if possible, 
                // but "reset" implies reset. Let's use 100 if we can't determine.
                // Or better: Reset means "match preset". 
                // We'll set arbitrary default size 200px if it was stretched before, 
                // or keep current size if it's already fixed? 
                // User said "Shift+Alt ... sizeDelta value 0".
                // If I set sizeDelta to 0 for a non-stretch anchor, size becomes 0! That's bad (invisible).
                
                // Let's use the current "Width" of the item as the delta.
                float currentW = rt.GetWidth((float)parentW);
                rt.sizeDeltaX = (currentW > 1.0f) ? currentW : 200.0f;
            }
            
            if (preset.minY != preset.maxY) {
                rt.sizeDeltaY = 0.0f;  // Vertical stretch
            } else {
                float currentH = rt.GetHeight((float)parentH);
                rt.sizeDeltaY = (currentH > 1.0f) ? currentH : 200.0f;
            }
            
            rt.ApplyToSceneItem(item, parentW, parentH);
        }
        else if (altHeld && !shiftHeld) {
            // === Alt only: Just move to position (legacy behavior) ===
            rt.anchoredPosX = 0.0f;
            rt.anchoredPosY = 0.0f;
            
            // Temporarily apply preset anchors for calculation (simulate "what if we were anchored here")
            // BUT we don't change the actual anchors stored in 'rt'.
            // Wait, legacy behavior sets position based on those anchors? 
            // In Unity, Alt-click sets position BUT NOT ANCHORS. 
            // It moves the pivot to the anchor point defined by the preset.
            // So we calculate where that point is, and move there.
            
            // Calculate usage of preset anchors
            float ax0 = (float)parentW * preset.minX;
            float ay0 = (float)parentH * preset.minY;
            float ax1 = (float)parentW * preset.maxX;
            float ay1 = (float)parentH * preset.maxY;
            float anchorRectW = ax1 - ax0;
            float anchorRectH = ay1 - ay0;
            
            // Target anchor pivot position
            float targetPivotX = ax0 + anchorRectW * rt.pivotX;
            float targetPivotY = ay0 + anchorRectH * rt.pivotY;
            
            // Current Pivot World Pos
            float currentX, currentY, currentW, currentH;
            rt.CalculateFinalRect((float)parentW, (float)parentH, currentX, currentY, currentW, currentH);
            float currentPivotWorldX = currentX + currentW * rt.pivotX;
            float currentPivotWorldY = currentY + currentH * rt.pivotY;
            
            // We want to move 'currentPivotWorld' to 'targetPivot'.
            // anchoredPos = (TargetPos - AnchorPivot) ... 
            // If we don't change anchors, 'AnchorPivot' is based on OLD anchors.
            // anchoredPos = targetPivot - oldAnchorPivot.
            
            // Wait, Unity Alt Click: "Sets position".
            // If I Alt-click "Top Left", it moves the object to Top-Left corner.
            // It modifies 'anchoredPosition' such that the object is visually at Top Left.
            // It does NOT change anchors.
            
            float oldAx0 = (float)parentW * rt.anchorMinX;
            float oldAy0 = (float)parentH * rt.anchorMinY;
            float oldAx1 = (float)parentW * rt.anchorMaxX;
            float oldAy1 = (float)parentH * rt.anchorMaxY;
            float oldAnchorRectW = oldAx1 - oldAx0;
            float oldAnchorRectH = oldAy1 - oldAy0;
            
            float oldAnchorPivotX = oldAx0 + oldAnchorRectW * rt.pivotX;
            float oldAnchorPivotY = oldAy0 + oldAnchorRectH * rt.pivotY;
            
            // We want the object's pivot to be at targetPivotX/Y
            // WorldPivot = OldAnchorPivot + NewAnchoredPos
            // NewAnchoredPos = TargetPivot - OldAnchorPivot
            
            rt.anchoredPosX = targetPivotX - oldAnchorPivotX;
            rt.anchoredPosY = targetPivotY - oldAnchorPivotY;
            
            // Special case: If preset is stretch, we also resize?
            // Unity Alt-Click on Stretch Preset: Resizes to fill that dimension.
            // So if I accept resizing...
            if (preset.minX != preset.maxX) {
                // Resize width to match parent width
                // sizeDeltaX = width - anchorRectW
                // We want width = parentW
                // So sizeDeltaX = parentW - anchorRectW
                 rt.sizeDeltaX = (float)parentW - oldAnchorRectW;
                 
                 // And we also align X to 0? 
                 // If we stretch, position should center.
                 // anchoredPos for X becomes 0 if fully centered?
                 // Let's stick to standard "Fill" logic: 0 offset from anchors.
                 // But we aren't changing anchors!
                 // If anchors are center, and we stretch:
                 // Width becomes parentW. Pivot is center.
                 // Position is center.
                 // So yes, anchoredPosX = 0.
            }
            if (preset.minY != preset.maxY) {
                 rt.sizeDeltaY = (float)parentH - oldAnchorRectH;
            }
            
            // Re-apply
            rt.ApplyToSceneItem(item, parentW, parentH);
        }
    });

    obs_source_release(source);
    RefreshFromSelection();
}

