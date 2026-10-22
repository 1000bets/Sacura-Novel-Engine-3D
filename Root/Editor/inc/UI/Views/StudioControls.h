#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPointer>
#include <QVariantAnimation>

class QAbstractButton;
class QListWidget;
class QLineEdit;
namespace EditorUi
{
class StudioNumberEdit : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    QString textFromValue(double Value) const override;
};

// Same model/signals as QComboBox; the chosen value is never a text editor.
class StudioSelect : public QComboBox
{
public:
    explicit StudioSelect(QWidget* Parent = nullptr);
    ~StudioSelect() override;
    void showPopup() override;
    void hidePopup() override;
protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;
    void keyPressEvent(QKeyEvent* Event) override;
private:
    void Choose();
    void ClosePopup();
    void SetPopupOpen(bool Open);
    void TransitionPopup(bool Opening);
    QPointer<QWidget> Popup;
    QPointer<QListWidget> Options;
    QPointer<QLineEdit> Search;
    QPoint PopupPosition;
    QPoint PopupRestPosition;
    bool bClosing = false;
};

class StudioCheckBox : public QCheckBox
{
public:
    explicit StudioCheckBox(QWidget* Parent = nullptr);
    StudioCheckBox(const QString& Text, QWidget* Parent = nullptr);
protected:
    void paintEvent(QPaintEvent* Event) override;
    bool hitButton(const QPoint& Position) const override;
private:
    QVariantAnimation Motion;
    qreal Progress = 0;
};

// Animation owns only presentation; toggle signals remain available to persist disclosure state.
void AnimateDisclosure(QAbstractButton& Toggle, QWidget& Fields);
}
