#pragma once

#include <QFrame>
#include <QString>

class QLabel;

class TemplateCardWidget : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(bool Selected READ IsSelected WRITE SetSelected)

public:
    TemplateCardWidget(
        const QString& TemplateId,
        const QString& Title,
        const QString& Description,
        const QPixmap& Thumbnail,
        bool bEmptyStyle,
        QWidget* Parent = nullptr);

    const QString& TemplateId() const { return CardTemplateId; }
    bool IsSelected() const { return bSelected; }
    void SetSelected(bool bInSelected);

signals:
    void Clicked(const QString& TemplateId);

protected:
    void mousePressEvent(QMouseEvent* Event) override;
    void paintEvent(QPaintEvent* Event) override;

private:
    void RefreshStyle();

    QString CardTemplateId;
    bool bSelected = false;
    bool bEmptyCard = false;
    QLabel* ThumbnailLabel = nullptr;
    QLabel* TitleLabel = nullptr;
    QLabel* DescriptionLabel = nullptr;
    QLabel* CheckLabel = nullptr;
};
