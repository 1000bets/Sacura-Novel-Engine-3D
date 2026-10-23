#pragma once
#include <QWidget>

class QLabel;
class QMenuBar;
class QPushButton;

namespace EditorUi
{
// Command wiring is supplied by the shell; the view owns only widgets and placement.
class StudioHeaderView final : public QWidget
{
public:
    StudioHeaderView(QMenuBar& Menus, QLabel& ProjectTitle, QWidget* Parent = nullptr);
    QPushButton* Save = nullptr;
    QPushButton* Play = nullptr;
    QPushButton* Pause = nullptr;
    QPushButton* Step = nullptr;
    QPushButton* Home = nullptr;
    QLabel* Mode = nullptr;
};
}
