#pragma once

#include <QPushButton>
#include <QPaintEvent>

enum class AnchorH { Left, Center, Right, Stretch };
enum class AnchorV { Top, Middle, Bottom, Stretch };

// Forward declaration
struct AnchorPreset;

class AnchorButton : public QPushButton {
    Q_OBJECT

public:
    AnchorButton(AnchorH h, AnchorV v, QWidget *parent = nullptr);

    AnchorH horizontal() const { return hAlign; }
    AnchorV vertical() const { return vAlign; }
    
    // Get Unity-style anchor preset values
    AnchorPreset GetPresetValues() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    AnchorH hAlign;
    AnchorV vAlign;
};
