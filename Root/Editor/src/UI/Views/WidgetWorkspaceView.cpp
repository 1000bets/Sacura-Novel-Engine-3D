#include "UI/Views/WidgetWorkspaceView.h"
#include "UI/Views/WidgetCanvasView.h"
#include "UI/Views/StudioControls.h"
#include "UI/Styles/StudioIcons.h"
#include "UI/WidgetLayout.h"
#include <QApplication>
#include <QBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QDialog>
#include <QDialogButtonBox>
#include <QImageReader>
#include <QBuffer>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QSet>
#include <algorithm>

namespace EditorUi
{
namespace
{
class WidgetHeaderLayout final : public QLayout
{
  public:
    explicit WidgetHeaderLayout(QWidget *Parent) : QLayout(Parent)
    {
        setContentsMargins(12, 8, 12, 8);
        setSpacing(8);
    }
    ~WidgetHeaderLayout() override
    {
        while (auto *Item = takeAt(0))
            delete Item;
    }
    void addItem(QLayoutItem *Item) override
    {
        Items.append(Item);
    }
    int count() const override
    {
        return static_cast<int>(Items.size());
    }
    QLayoutItem *itemAt(int I) const override
    {
        return I >= 0 && I < count() ? Items[I] : nullptr;
    }
    QLayoutItem *takeAt(int I) override
    {
        return I >= 0 && I < count() ? Items.takeAt(I) : nullptr;
    }
    bool hasHeightForWidth() const override
    {
        return true;
    }
    int heightForWidth(int W) const override
    {
        return Arrange(QRect(0, 0, W, 0), false);
    }
    QSize minimumSize() const override
    {
        QSize S;
        for (auto *I : Items)
            S = S.expandedTo(I->minimumSize());
        return S + QSize(24, 16);
    }
    QSize sizeHint() const override
    {
        int W = 24;
        for (auto *I : Items)
            W += I->sizeHint().width() + spacing();
        return {W, heightForWidth(W)};
    }
    void setGeometry(const QRect &R) override
    {
        QLayout::setGeometry(R);
        Arrange(R, true);
    }

  private:
    int Arrange(const QRect &R, bool Apply) const
    {
        auto Area = R.marginsRemoved(contentsMargins());
        int X = Area.x(), Y = Area.y(), Row = 0;
        for (auto *Item : Items)
        {
            auto S = Item->sizeHint()
                         .expandedTo(Item->minimumSize())
                         .boundedTo(QSize(std::max(1, Area.width()), QWIDGETSIZE_MAX));
            if (X > Area.x() && X + S.width() > Area.x() + Area.width())
            {
                X = Area.x();
                Y += Row + spacing();
                Row = 0;
            }
            if (Apply)
                Item->setGeometry(QRect(QPoint(X, Y), S));
            X += S.width() + spacing();
            Row = std::max(Row, S.height());
        }
        return Y - R.y() + Row + contentsMargins().bottom();
    }
    QList<QLayoutItem *> Items;
};
class TextEdit : public QPlainTextEdit
{
  public:
    using QPlainTextEdit::QPlainTextEdit;
    std::function<void(QString)> Commit;

  protected:
    void focusOutEvent(QFocusEvent *E) override
    {
        QPlainTextEdit::focusOutEvent(E);
        if (Commit)
            Commit(toPlainText());
    }
};
QPushButton *Button(const QString &Text, const QString &Icon, QWidget *Parent, QLayout *Layout,
                    std::function<void()> Callback)
{
    auto *B = new QPushButton(MakeFrontendIcon(Icon), Text, Parent);
    B->setProperty("uiRole", "widgetButton");
    B->setIconSize(QSize(16, 16));
    Layout->addWidget(B);
    QObject::connect(B, &QPushButton::clicked, Parent, std::move(Callback));
    return B;
}
void Heading(QVBoxLayout *L, const QString &Text)
{
    auto *Label = new QLabel(Text);
    Label->setProperty("uiRole", "widgetHeading");
    L->addWidget(Label);
}
QJsonObject FindElement(const QJsonObject &W, const QString &Id)
{
    for (auto V : W["elements"].toArray())
        if (V.toObject()["id"] == Id)
            return V.toObject();
    return {};
}
QPointF Point(const QJsonObject &L, const char *Key)
{
    auto A = L[Key].toArray();
    return {A[0].toDouble(), A[1].toDouble()};
}
} // namespace
WidgetWorkspaceView::WidgetWorkspaceView(QWidget *Parent) : QWidget(Parent)
{
    setObjectName("WidgetWorkspace");
    auto *Root = new QVBoxLayout(this);
    Root->setContentsMargins(0, 0, 0, 0);
    Root->setSpacing(0);
    auto *Header = new QWidget(this);
    Header->setObjectName("WidgetHeader");
    auto *Top = new WidgetHeaderLayout(Header);
    auto *Title = new QLabel(tr("Редактор виджетов"), Header);
    Title->setProperty("uiRole", "widgetHeading");
    Top->addWidget(Title);
    Picker = new StudioSelect(Header);
    Picker->setObjectName("WidgetPicker");
    Picker->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    Picker->setFixedWidth(220);
    Top->addWidget(Picker);
    EditTools = new QWidget(Header);
    auto *Tools = new QHBoxLayout(EditTools);
    Tools->setContentsMargins(0, 0, 0, 0);
    Tools->setSpacing(4);
    Button(tr("Копия"), "Copy", EditTools, Tools, [this]() { Create(true); });
    Button(tr("Создать"), "Plus", EditTools, Tools, [this]() { Create(false); });
    Button(tr("Импорт"), "Download", EditTools, Tools, [this]() { Import(); });
    Button(tr("Экспорт"), "Upload", EditTools, Tools, [this]() { Export(); });
    Top->addWidget(EditTools);
    Preview = Button(tr("Проверить в игре"), "Play", Header, Top,
                     [this]()
                     {
                         FlushEdits();
                         if (PreviewRequested)
                             PreviewRequested(CurrentWidget());
                     });
    Root->addWidget(Header);
    auto *Body = new QSplitter(Qt::Horizontal, this);
    Body->setObjectName("WidgetBody");
    Body->setChildrenCollapsible(false);
    Body->setHandleWidth(1);
    Root->addWidget(Body, 1);
    auto *LibraryScroll = new QScrollArea(Body);
    LibraryScroll->setWidgetResizable(true);
    LibraryScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    LibraryScroll->setMinimumWidth(145);
    auto *Library = new QWidget;
    Library->setObjectName("WidgetLibrary");
    auto *Left = new QVBoxLayout(Library);
    Left->setContentsMargins(12, 8, 12, 12);
    Left->setSpacing(5);
    Heading(Left, tr("Интерфейс проекта"));
    auto Kinds = WidgetLayout::Kinds();
    for (int I = 0; I < Kinds.size(); ++I)
    {
        auto Kind = Kinds[I];
        Left->addWidget(new QLabel(WidgetLayout::KindLabel(Kind)));
        auto *C = new StudioSelect(Library);
        C->setObjectName("WidgetAssignment_" + Kind);
        Assignments[I] = C;
        Left->addWidget(C);
        connect(C, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this, C, Kind]()
                {
                    if (Refreshing)
                        return;
                    auto Id = C->currentData().toString();
                    Mutate(
                        [&](QJsonObject &D)
                        {
                            auto Ui = D["ui"].toObject();
                            Ui[Kind] = Id;
                            D["ui"] = Ui;
                        });
                });
    }
    Heading(Left, tr("Элементы"));
    Elements = new QListWidget(Library);
    Elements->setObjectName("WidgetElements");
    Elements->setIconSize(QSize(14, 14));
    Elements->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    Elements->setMinimumHeight(180);
    Left->addWidget(Elements, 1);
    Add = new StudioSelect(Library);
    Add->addItem(tr("＋ Добавить элемент…"), "");
    for (auto T : WidgetLayout::Types())
        Add->addItem(WidgetLayout::TypeLabel(T), T);
    Left->addWidget(Add);
    auto *Help = new QLabel(
        tr("Новый элемент добавляется внутрь выбранной панели. Стрелки перемещают элемент; Shift — шаг 10."));
    Help->setWordWrap(true);
    Help->setProperty("uiRole", "widgetHint");
    Left->addWidget(Help);
    LibraryScroll->setWidget(Library);
    auto *Center = new QWidget(Body);
    Center->setObjectName("WidgetCanvasArea");
    auto *Middle = new QVBoxLayout(Center);
    Middle->setContentsMargins(10, 10, 10, 10);
    Middle->setSpacing(8);
    auto *Bar = new QHBoxLayout;
    auto *Resolution = new StudioSelect(Center);
    Resolution->setObjectName("WidgetResolution");
    Resolution->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    Resolution->setFixedWidth(120);
    for (auto S : {QSize(1280, 720), QSize(1920, 1080), QSize(1024, 768), QSize(720, 1280), QSize(2560, 1080)})
        Resolution->addItem(QString("%1x%2").arg(S.width()).arg(S.height()), S);
    Bar->addWidget(Resolution);
    auto *Snap = new StudioCheckBox(tr("Привязка 8 px"), Center);
    Snap->setChecked(true);
    Bar->addWidget(Snap);
    Bar->addStretch();
    Middle->addLayout(Bar);
    Canvas = new WidgetCanvasView(Center);
    Middle->addWidget(Canvas, 1);
    Properties = new QScrollArea(Body);
    Properties->setObjectName("WidgetProperties");
    Properties->setWidgetResizable(true);
    Properties->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    Properties->setMinimumWidth(210);
    Body->setStretchFactor(0, 0);
    Body->setStretchFactor(1, 1);
    Body->setStretchFactor(2, 0);
    Body->setSizes({190, 800, 270});
    Note = new QLabel(this);
    Note->setObjectName("WidgetSaveNote");
    Note->setWordWrap(true);
    Root->addWidget(Note);
    connect(Picker, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this]()
            {
                if (!Refreshing)
                    SelectWidget(Picker->currentData().toString());
            });
    connect(Elements, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *Item)
            {
                if (Refreshing)
                    return;
                ElementId = Item ? Item->data(Qt::UserRole).toString() : QString();
                Canvas->SetWidget(CurrentWidget(), ElementId);
                Inspector();
            });
    connect(Add, qOverload<int>(&QComboBox::activated), this,
            [this](int I)
            {
                auto Type = Add->itemData(I).toString();
                Add->setCurrentIndex(0);
                if (!Type.isEmpty())
                    AddElement(Type);
            });
    connect(Resolution, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, Resolution]() { Canvas->SetFrame(Resolution->currentData().toSize()); });
    connect(Snap, &QCheckBox::toggled, this, [this](bool Value) { Canvas->SetSnap(Value); });
    Canvas->Selected = [this](QString Id)
    {
        ElementId = Id;
        Refreshing = true;
        for (int I = 0; I < Elements->count(); ++I)
            if (Elements->item(I)->data(Qt::UserRole) == Id)
                Elements->setCurrentRow(I);
        if (Id.isEmpty())
            Elements->setCurrentRow(-1);
        Refreshing = false;
        Inspector();
    };
    Canvas->LayoutChanged = [this](QString Id, QJsonObject Layout)
    {
        ElementId = Id;
        EditElement([&](QJsonObject &E) { E["layout"] = Layout; });
    };
    Canvas->DeleteRequested = [this]() { DeleteElement(); };
    Document.Changed = [this]()
    {
        QueueRefresh();
        if (Changed)
            Changed();
    };
}
void WidgetWorkspaceView::Error(const QString &Message)
{
    Note->setText(Message);
    Note->setProperty("error", true);
}
bool WidgetWorkspaceView::Open(const QString &Root)
{
    QString Message;
    if (!Document.Open(WidgetLayout::ProjectFile(Root), Message))
    {
        Error(Message);
        setEnabled(false);
        return false;
    }
    WidgetId = Document.GetData()["ui"].toObject()["dialogue"].toString();
    ElementId.clear();
    Refresh();
    return true;
}
void WidgetWorkspaceView::FlushEdits()
{
    auto *Focus = QApplication::focusWidget();
    if (Focus && isAncestorOf(Focus))
        Focus->clearFocus();
}
bool WidgetWorkspaceView::Save()
{
    FlushEdits();
    QString Message;
    if (!Document.Save(Message))
    {
        Error(Message);
        QMessageBox::warning(this, tr("Сохранение UI"), Message);
        return false;
    }
    if (Saved)
        Saved();
    return true;
}
bool WidgetWorkspaceView::PromptSave()
{
    FlushEdits();
    if (!Document.IsDirty())
        return true;
    auto Answer =
        QMessageBox::question(this, tr("Несохранённые виджеты"), tr("Сохранить изменения интерфейса проекта?"),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    return Answer != QMessageBox::Cancel && (Answer != QMessageBox::Save || Save());
}
void WidgetWorkspaceView::Undo()
{
    FlushEdits();
    if (!Locked)
        Document.Undo();
}
void WidgetWorkspaceView::Redo()
{
    FlushEdits();
    if (!Locked)
        Document.Redo();
}
void WidgetWorkspaceView::DeleteSelection()
{
    if (!Locked)
        DeleteElement();
}
void WidgetWorkspaceView::SetLocked(bool Value)
{
    if (Locked == Value)
        return;
    if (Value)
        FlushEdits();
    Locked = Value;
    Canvas->SetLocked(Value);
    Refresh();
}
void WidgetWorkspaceView::SelectWidget(const QString &Id)
{
    WidgetId = Id;
    ElementId.clear();
    Refresh();
}
QJsonObject WidgetWorkspaceView::CurrentWidget() const
{
    auto Ws = Document.GetData()["widgets"].toArray();
    for (auto W : Ws)
        if (W.toObject()["id"] == WidgetId)
            return W.toObject();
    return Ws.isEmpty() ? QJsonObject{} : Ws.first().toObject();
}
void WidgetWorkspaceView::QueueRefresh()
{
    if (Pending)
        return;
    Pending = true;
    QTimer::singleShot(0, this,
                       [this]()
                       {
                           Pending = false;
                           Refresh();
                       });
}
void WidgetWorkspaceView::Refresh()
{
    Refreshing = true;
    auto W = CurrentWidget();
    WidgetId = W["id"].toString();
    if (FindElement(W, ElementId).isEmpty())
        ElementId.clear();
    Picker->clear();
    for (auto V : Document.GetData()["widgets"].toArray())
    {
        auto Item = V.toObject();
        Picker->addItem(Item["name"].toString() + " · " + WidgetLayout::KindLabel(Item["kind"].toString()),
                        Item["id"].toString());
    }
    Picker->setCurrentIndex(Picker->findData(WidgetId));
    auto Kinds = WidgetLayout::Kinds();
    for (int I = 0; I < Kinds.size(); ++I)
    {
        auto *C = Assignments[I];
        C->clear();
        for (auto V : Document.GetData()["widgets"].toArray())
            if (V.toObject()["kind"] == Kinds[I])
                C->addItem(V.toObject()["name"].toString(), V.toObject()["id"].toString());
        C->setCurrentIndex(C->findData(Document.GetData()["ui"].toObject()[Kinds[I]].toString()));
        C->setEnabled(!Locked);
    }
    Elements->clear();
    for (auto V : W["elements"].toArray())
    {
        auto E = V.toObject();
        int Depth = 0;
        auto P = E["parentId"].toString();
        while (!P.isEmpty() && Depth < 512)
        {
            ++Depth;
            P = FindElement(W, P)["parentId"].toString();
        }
        auto Type = E["type"].toString();
        auto *Item = new QListWidgetItem(MakeFrontendIcon(Type == "panel"    ? "Square"
                                                          : Type == "image"  ? "Image"
                                                          : Type == "button" ? "MousePointer2"
                                                                             : "Type"),
                                         QString(Depth * 2, ' ') + E["name"].toString(), Elements);
        Item->setData(Qt::UserRole, E["id"].toString());
        if (!E["visible"].toBool(true))
            Item->setText(Item->text() + tr(" (скрыт)"));
        if (E["id"] == ElementId)
            Elements->setCurrentItem(Item);
    }
    Elements->setFixedHeight(std::max(120, Elements->count() * 40));
    EditTools->setEnabled(!Locked);
    Add->setEnabled(!Locked);
    Preview->setEnabled(!Locked && W["kind"] == "dialogue");
    Preview->setToolTip(W["kind"] == "dialogue"
                            ? tr("Запустить историю с текущим оформлением")
                            : tr("В нативной игре пока подключён только диалог. Меню и HUD доступны на холсте."));
    Canvas->SetWidget(W, ElementId);
    Inspector();
    Note->setProperty("error", false);
    Note->setText(Locked ? tr("Остановите игру для редактирования.")
                         : tr("%1 · Ctrl+S — сохранить · Ctrl+Z — отмена · бирюзовые метки — анкеры · розовая рамка — "
                              "выбранный элемент")
                               .arg(Document.IsDirty() ? tr("Есть несохранённые изменения") : tr("Виджеты UI")));
    Refreshing = false;
}
void WidgetWorkspaceView::Mutate(const std::function<void(QJsonObject &)> &Fn)
{
    if (Locked || Refreshing)
        return;
    auto Candidate = Document.GetData();
    Fn(Candidate);
    QString Message;
    if (!Document.Apply(Candidate, Message))
        Error(Message);
}
void WidgetWorkspaceView::EditWidget(const std::function<void(QJsonObject &)> &Fn)
{
    const auto Id = WidgetId;
    Mutate(
        [&](QJsonObject &D)
        {
            auto Ws = D["widgets"].toArray();
            for (int I = 0; I < Ws.size(); ++I)
                if (Ws[I].toObject()["id"] == Id)
                {
                    auto W = Ws[I].toObject();
                    Fn(W);
                    Ws[I] = W;
                }
            D["widgets"] = Ws;
        });
}
void WidgetWorkspaceView::EditElement(const std::function<void(QJsonObject &)> &Fn)
{
    const auto Id = ElementId;
    EditWidget(
        [&](QJsonObject &W)
        {
            auto Es = W["elements"].toArray();
            for (int I = 0; I < Es.size(); ++I)
                if (Es[I].toObject()["id"] == Id)
                {
                    auto E = Es[I].toObject();
                    Fn(E);
                    Es[I] = E;
                }
            W["elements"] = Es;
        });
}
void WidgetWorkspaceView::AddElement(const QString &Type)
{
    auto E = WidgetLayout::Element(Type);
    auto Parent = FindElement(CurrentWidget(), ElementId);
    if (Parent["type"] == "panel")
        E["parentId"] = ElementId;
    EditWidget(
        [&](QJsonObject &W)
        {
            auto Es = W["elements"].toArray();
            Es.append(E);
            W["elements"] = Es;
        });
    ElementId = E["id"].toString();
}
void WidgetWorkspaceView::DeleteElement()
{
    if (ElementId.isEmpty())
        return;
    EditWidget(
        [&](QJsonObject &W)
        {
            auto Es = W["elements"].toArray();
            QSet<QString> Removed{ElementId};
            bool More = true;
            while (More)
            {
                More = false;
                for (auto V : Es)
                {
                    auto E = V.toObject();
                    if (Removed.contains(E["parentId"].toString()) && !Removed.contains(E["id"].toString()))
                    {
                        Removed.insert(E["id"].toString());
                        More = true;
                    }
                }
            }
            QJsonArray Kept;
            for (auto V : Es)
                if (!Removed.contains(V.toObject()["id"].toString()))
                    Kept.append(V);
            W["elements"] = Kept;
        });
}
void WidgetWorkspaceView::Create(bool Copy)
{
    if (Locked)
        return;
    auto W = Copy ? CurrentWidget() : WidgetLayout::Create(CurrentWidget()["kind"].toString(), tr("Новый виджет"));
    if (Copy)
        W["name"] = W["name"].toString() + tr(" · копия");
    QString Id, Message;
    if (!Document.ImportWidget(W, Id, Message))
        Error(Message);
    else
        SelectWidget(Id);
}
void WidgetWorkspaceView::DeleteWidget()
{
    auto W = CurrentWidget();
    auto Kind = W["kind"].toString();
    auto Id = WidgetId;
    Mutate(
        [&](QJsonObject &D)
        {
            QJsonArray Ws;
            for (auto V : D["widgets"].toArray())
                if (V.toObject()["id"] != Id)
                    Ws.append(V);
            auto Ui = D["ui"].toObject();
            if (Ui[Kind] == Id)
                for (auto V : Ws)
                    if (V.toObject()["kind"] == Kind)
                    {
                        Ui[Kind] = V.toObject()["id"];
                        break;
                    }
            D["ui"] = Ui;
            D["widgets"] = Ws;
        });
}
void WidgetWorkspaceView::Reanchor(QPointF Min, QPointF Max, QPointF Pivot)
{
    auto W = CurrentWidget();
    auto E = FindElement(W, ElementId);
    auto Ref = W["referenceSize"].toArray();
    auto Parent = E["parentId"].toString();
    QSizeF Size =
        Parent.isEmpty() ? QSizeF(Ref[0].toDouble(), Ref[1].toDouble()) : WidgetLayout::Rects(W)[Parent].size();
    auto L = WidgetLayout::Reanchor(E["layout"].toObject(), Size, Min, Max, Pivot);
    EditElement([&](QJsonObject &Value) { Value["layout"] = L; });
}
void WidgetWorkspaceView::Import()
{
    auto Path = QFileDialog::getOpenFileName(this, tr("Импорт виджета"), {}, tr("Виджет Sacura (*.sakura-ui)"));
    if (Path.isEmpty())
        return;
    QJsonObject W;
    QString Message;
    if (!WidgetDocument::ReadPackage(Path, W, Message))
    {
        QMessageBox::warning(this, tr("Импорт виджета"), Message);
        return;
    }
    if (QMessageBox::question(
            this, tr("Импорт виджета"),
            tr("%1 · %2\nЭлементов: %3. Изображения встроены в пакет.\nДобавить независимую редактируемую копию?")
                .arg(W["name"].toString(), WidgetLayout::KindLabel(W["kind"].toString()))
                .arg(W["elements"].toArray().size())) != QMessageBox::Yes)
        return;
    QString Id;
    if (!Document.ImportWidget(W, Id, Message))
        Error(Message);
    else
        SelectWidget(Id);
}
void WidgetWorkspaceView::Export()
{
    FlushEdits();
    QDialog Dialog(this);
    Dialog.setWindowTitle(tr("Экспорт виджета"));
    auto *Form = new QFormLayout(&Dialog);
    auto *Author = new QLineEdit(&Dialog);
    auto *License = new QLineEdit(&Dialog);
    auto *Description = new QLineEdit(&Dialog);
    Form->addRow(tr("Автор"), Author);
    Form->addRow(tr("Лицензия"), License);
    Form->addRow(tr("Описание"), Description);
    auto *Buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &Dialog);
    Form->addRow(Buttons);
    connect(Buttons, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
    connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    if (Dialog.exec() != QDialog::Accepted)
        return;
    auto Path = QFileDialog::getSaveFileName(this, tr("Экспорт виджета"), {}, tr("Виджет Sacura (*.sakura-ui)"));
    if (Path.isEmpty())
        return;
    if (!Path.endsWith(".sakura-ui", Qt::CaseInsensitive))
        Path += ".sakura-ui";
    QString Message;
    if (!Document.ExportWidget(
            WidgetId, Path, Message,
            {{"author", Author->text()}, {"license", License->text()}, {"description", Description->text()}}))
        QMessageBox::warning(this, tr("Экспорт виджета"), Message);
}
void WidgetWorkspaceView::Inspector()
{
    const int Offset = Properties->verticalScrollBar()->value();
    if (auto *Old = Properties->takeWidget())
    {
        Old->blockSignals(true);
        for (auto *Child : Old->findChildren<QWidget *>())
            Child->blockSignals(true);
        Old->hide();
        Old->deleteLater();
    }
    auto *Form = new QWidget;
    Form->setObjectName("WidgetInspectorForm");
    auto *L = new QVBoxLayout(Form);
    L->setContentsMargins(12, 12, 12, 12);
    L->setSpacing(8);
    auto W = CurrentWidget();
    auto E = FindElement(W, ElementId);
    auto Line = [&](const QString &Caption, const QString &Value, std::function<void(QString)> Commit)
    {
        L->addWidget(new QLabel(Caption));
        auto *Input = new QLineEdit(Value, Form);
        Input->setObjectName("WidgetField_" + Caption);
        L->addWidget(Input);
        connect(Input, &QLineEdit::editingFinished, Form,
                [Input, Commit]()
                {
                    if (!Input->signalsBlocked())
                        Commit(Input->text());
                });
    };
    auto Number = [&](QBoxLayout *Target, const QString &Caption, double Value, double Min, double Max, double Step,
                      std::function<void(double)> Commit)
    {
        auto *Group = new QWidget(Form);
        auto *Fields = new QVBoxLayout(Group);
        Fields->setContentsMargins(0, 0, 0, 0);
        Fields->setSpacing(4);
        Fields->addWidget(new QLabel(Caption));
        auto *Input = new StudioNumberEdit(Group);
        Input->setObjectName("WidgetField_" + Caption);
        Input->setRange(Min, Max);
        Input->setDecimals(Step < 1 ? 2 : 0);
        Input->setSingleStep(Step);
        Input->setValue(Value);
        Input->setKeyboardTracking(false);
        Fields->addWidget(Input);
        Target->addWidget(Group);
        connect(Input, &QDoubleSpinBox::editingFinished, Form,
                [Input, Commit]()
                {
                    if (!Input->signalsBlocked())
                        Commit(Input->value());
                });
    };
    auto Select = [&](const QString &Caption, const QList<QPair<QString, QString>> &Options, const QString &Value,
                      std::function<void(QString)> Commit)
    {
        L->addWidget(new QLabel(Caption));
        auto *Input = new StudioSelect(Form);
        Input->setObjectName("WidgetField_" + Caption);
        for (auto Option : Options)
            Input->addItem(Option.second, Option.first);
        Input->setCurrentIndex(Input->findData(Value));
        L->addWidget(Input);
        connect(Input, qOverload<int>(&QComboBox::activated), Form,
                [Input, Commit]() { Commit(Input->currentData().toString()); });
        return Input;
    };
    Heading(L, tr("Виджет"));
    Line(tr("Название"), W["name"].toString(),
         [this](QString Value) { EditWidget([&](QJsonObject &Item) { Item["name"] = Value.trimmed(); }); });
    QList<QPair<QString, QString>> Kinds;
    for (auto K : WidgetLayout::Kinds())
        Kinds.append({K, WidgetLayout::KindLabel(K)});
    Select(tr("Тип"), Kinds, W["kind"].toString(),
           [this](QString Kind)
           {
               auto Id = WidgetId;
               Mutate(
                   [&](QJsonObject &D)
                   {
                       auto Ws = D["widgets"].toArray();
                       for (int I = 0; I < Ws.size(); ++I)
                           if (Ws[I].toObject()["id"] == Id)
                           {
                               auto Item = Ws[I].toObject();
                               Item["kind"] = Kind;
                               Ws[I] = Item;
                           }
                       auto Ui = D["ui"].toObject();
                       for (auto K : WidgetLayout::Kinds())
                       {
                           QString First;
                           bool Valid = false;
                           for (auto V : Ws)
                               if (V.toObject()["kind"] == K)
                               {
                                   if (First.isEmpty())
                                       First = V.toObject()["id"].toString();
                                   if (V.toObject()["id"] == Ui[K])
                                       Valid = true;
                               }
                           if (First.isEmpty())
                           {
                               auto Default = WidgetLayout::Create(K);
                               QString Name = Default["name"].toString() + tr(" (стандартный)");
                               int Suffix = 2;
                               QStringList Names;
                               for (auto V : Ws)
                                   Names.append(V.toObject()["name"].toString().toCaseFolded());
                               auto Base = Name;
                               while (Names.contains(Name.toCaseFolded()))
                                   Name = Base + QString(" %1").arg(Suffix++);
                               Default["name"] = Name;
                               First = Default["id"].toString();
                               Ws.append(Default);
                           }
                           if (!Valid)
                               Ui[K] = First;
                       }
                       D["widgets"] = Ws;
                       D["ui"] = Ui;
                   });
           });
    auto *SizeRow = new QHBoxLayout;
    L->addLayout(SizeRow);
    auto Ref = W["referenceSize"].toArray();
    for (int Axis = 0; Axis < 2; ++Axis)
        Number(SizeRow, Axis ? tr("Высота холста") : tr("Ширина холста"), Ref[Axis].toDouble(), 100, 7680, 1,
               [this, Axis](double V)
               {
                   EditWidget(
                       [&](QJsonObject &Item)
                       {
                           auto A = Item["referenceSize"].toArray();
                           A[Axis] = V;
                           Item["referenceSize"] = A;
                       });
               });
    if (!E.isEmpty())
    {
        Heading(L, tr("Элемент"));
        Line(tr("Имя элемента"), E["name"].toString(),
             [this](QString V) { EditElement([&](QJsonObject &Item) { Item["name"] = V; }); });
        auto *Visible = new StudioCheckBox(tr("Видимый"), Form);
        Visible->setChecked(E["visible"].toBool(true));
        L->addWidget(Visible);
        connect(Visible, &QCheckBox::toggled, Form,
                [this](bool V) { EditElement([&](QJsonObject &Item) { Item["visible"] = V; }); });
        QList<QPair<QString, QString>> Parents{{"", tr("Холст")}};
        for (auto Value : W["elements"].toArray())
        {
            auto Item = Value.toObject();
            if (Item["type"] != "panel" || Item["id"] == ElementId)
                continue;
            bool Descendant = false;
            auto P = Item;
            while (!P["parentId"].toString().isEmpty())
            {
                if (P["parentId"] == ElementId)
                {
                    Descendant = true;
                    break;
                }
                P = FindElement(W, P["parentId"].toString());
            }
            if (!Descendant)
                Parents.append({Item["id"].toString(), Item["name"].toString()});
        }
        Select(tr("Родитель"), Parents, E["parentId"].toString(),
               [this](QString Parent)
               {
                   auto Rs = WidgetLayout::Rects(CurrentWidget());
                   auto R = Rs[ElementId];
                   auto P = Parent.isEmpty() ? QRectF() : Rs[Parent];
                   EditElement(
                       [&](QJsonObject &Item)
                       {
                           Item["parentId"] = Parent;
                           Item["layout"] = QJsonObject{{"anchorMin", QJsonArray{0, 0}},
                                                        {"anchorMax", QJsonArray{0, 0}},
                                                        {"pivot", QJsonArray{0, 0}},
                                                        {"x", R.x() - P.x()},
                                                        {"y", R.y() - P.y()},
                                                        {"width", R.width()},
                                                        {"height", R.height()}};
                       });
               });
        Heading(L, tr("Положение и анкеры"));
        auto *Presets = new QGridLayout;
        L->addLayout(Presets);
        const QStringList Symbols = {"↖", "↑", "↗", "←", "•", "→", "↙", "↓", "↘"};
        for (int I = 0; I < 9; ++I)
        {
            auto *B = new QPushButton(Symbols[I], Form);
            B->setToolTip(tr("Анкер %1, %2").arg(I % 3 / 2.).arg(I / 3 / 2.));
            Presets->addWidget(B, I / 3, I % 3);
            connect(B, &QPushButton::clicked, Form,
                    [this, I]()
                    {
                        QPointF A(I % 3 / 2., I / 3 / 2.);
                        Reanchor(A, A, A);
                    });
        }
        auto *Stretch = new QHBoxLayout;
        L->addLayout(Stretch);
        for (int Axis = 0; Axis < 3; ++Axis)
            Button(Axis == 0   ? tr("По ширине")
                   : Axis == 1 ? tr("По высоте")
                               : tr("Заполнить"),
                   {}, Form, Stretch,
                   [this, Axis]()
                   {
                       auto T = FindElement(CurrentWidget(), ElementId)["layout"].toObject();
                       auto Min = Point(T, "anchorMin"), Max = Point(T, "anchorMax");
                       if (Axis != 1)
                       {
                           Min.setX(0);
                           Max.setX(1);
                       }
                       if (Axis != 0)
                       {
                           Min.setY(0);
                           Max.setY(1);
                       }
                       Reanchor(Min, Max, Point(T, "pivot"));
                   });
        auto Layout = E["layout"].toObject();
        auto *Position = new QHBoxLayout;
        L->addLayout(Position);
        auto *Extent = new QHBoxLayout;
        L->addLayout(Extent);
        for (auto Key : {"x", "y", "width", "height"})
            Number(QString(Key) == "x" || QString(Key) == "y" ? Position : Extent,
                   QString(Key) == "width"    ? tr("Ширина / Δ")
                   : QString(Key) == "height" ? tr("Высота / Δ")
                                              : QString(Key).toUpper(),
                   Layout[Key].toDouble(), -100000, 100000, 1,
                   [this, Key](double V)
                   {
                       EditElement(
                           [&](QJsonObject &Item)
                           {
                               auto A = Item["layout"].toObject();
                               A[Key] = V;
                               Item["layout"] = A;
                           });
                   });
        for (auto Key : {"anchorMin", "anchorMax", "pivot"})
        {
            auto *Row = new QHBoxLayout;
            L->addLayout(Row);
            for (int Axis = 0; Axis < 2; ++Axis)
                Number(Row,
                       (QString(Key) == "anchorMin"   ? tr("Анкер min")
                        : QString(Key) == "anchorMax" ? tr("Анкер max")
                                                      : tr("Опорная точка")) +
                           (Axis ? " Y" : " X"),
                       Layout[Key].toArray()[Axis].toDouble(), 0, 1, .05,
                       [this, Key, Axis](double V)
                       {
                           auto T = FindElement(CurrentWidget(), ElementId)["layout"].toObject();
                           auto A = T[Key].toArray();
                           A[Axis] = V;
                           T[Key] = A;
                           auto Min = Point(T, "anchorMin"), Max = Point(T, "anchorMax");
                           if (Min.x() > Max.x() || Min.y() > Max.y())
                           {
                               Error(tr("Минимальный анкер не должен превышать максимальный."));
                               return;
                           }
                           Reanchor(Min, Max, Point(T, "pivot"));
                       });
        }
        Heading(L, tr("Содержимое и действия"));
        Select(tr("Привязка к игре"),
               {{"none", tr("Без привязки")},
                {"speaker", tr("Имя персонажа")},
                {"dialogueText", tr("Текст реплики")},
                {"choices", tr("Ответы игрока")},
                {"status", tr("Подсказка")},
                {"title", tr("Название игры")}},
               E["role"].toString(), [this](QString V) { EditElement([&](QJsonObject &Item) { Item["role"] = V; }); });
        if (E["type"] != "panel")
        {
            L->addWidget(new QLabel(tr("Текст / подпись")));
            auto *Text = new TextEdit(Form);
            Text->setObjectName("WidgetElementText");
            Text->setPlainText(E["text"].toString());
            Text->setFixedHeight(80);
            L->addWidget(Text);
            Text->Commit = [this, Text](QString V)
            {
                if (!Text->signalsBlocked())
                    EditElement([&](QJsonObject &Item) { Item["text"] = V; });
            };
        }
        if (E["type"] == "button")
            Select(tr("Действие"),
                   {{"none", tr("Без действия")},
                    {"advance", tr("Продолжить диалог")},
                    {"start", tr("Начать игру (макет)")},
                    {"resume", tr("Снять паузу (макет)")}},
                   E["action"].toString(),
                   [this](QString V) { EditElement([&](QJsonObject &Item) { Item["action"] = V; }); });
        if (E["type"] == "image")
            Button(tr("Загрузить изображение…"), "Image", Form, L,
                   [this]()
                   {
                       auto Path = QFileDialog::getOpenFileName(this, tr("Изображение"), {},
                                                                tr("Изображения (*.png *.jpg *.jpeg)"));
                       if (Path.isEmpty())
                           return;
                       QFile File(Path);
                       if (!File.open(QIODevice::ReadOnly) || File.size() > 2 * 1024 * 1024)
                       {
                           Error(tr("Выберите PNG/JPEG до 2 МБ."));
                           return;
                       }
                       auto Bytes = File.readAll();
                       QBuffer Buffer(&Bytes);
                       Buffer.open(QIODevice::ReadOnly);
                       QImageReader Reader(&Buffer);
                       auto Format = Reader.format();
                       auto Size = Reader.size();
                       if ((Format != "png" && Format != "jpeg") || Size.width() > 8192 || Size.height() > 8192 ||
                           Size.isEmpty() || Reader.read().isNull())
                       {
                           Error(tr("Повреждённое или слишком большое изображение."));
                           return;
                       }
                       QString Src = "data:image/" + QString::fromLatin1(Format) + ";base64," +
                                     QString::fromLatin1(Bytes.toBase64());
                       EditElement([&](QJsonObject &Item) { Item["src"] = Src; });
                   });
        Heading(L, tr("Оформление"));
        auto Style = E["style"].toObject();
        for (auto Field : {QPair<QString, QString>{"background", tr("Фон")},
                           {"color", tr("Цвет текста")},
                           {"borderColor", tr("Цвет рамки")}})
            Line(Field.second, Style[Field.first].toString(),
                 [this, Key = Field.first](QString V)
                 {
                     if (V != "transparent" && !QColor(V).isValid())
                     {
                         Error(tr("Введите цвет, например #e2b8d1."));
                         return;
                     }
                     EditElement(
                         [&](QJsonObject &Item)
                         {
                             auto S = Item["style"].toObject();
                             S[Key] = V;
                             Item["style"] = S;
                         });
                 });
        const QList<QPair<QString, QString>> StyleFields = {
            {"fontSize", tr("Размер текста")}, {"borderRadius", tr("Скругление")}, {"borderWidth", tr("Рамка")},
            {"padding", tr("Отступы")},        {"gap", tr("Расстояние ответов")},  {"opacity", tr("Непрозрачность")}};
        for (auto Field : StyleFields)
            Number(L, Field.second, Style[Field.first].toDouble(Field.first == "opacity" ? 1 : 0), 0,
                   Field.first == "opacity"       ? 1
                   : Field.first == "borderWidth" ? 12
                   : Field.first == "gap"         ? 40
                   : Field.first == "padding"     ? 80
                   : Field.first == "fontSize"    ? 144
                                                  : 100,
                   Field.first == "opacity" ? .01 : 1,
                   [this, Key = Field.first](double V)
                   {
                       EditElement(
                           [&](QJsonObject &Item)
                           {
                               auto S = Item["style"].toObject();
                               S[Key] = V;
                               Item["style"] = S;
                           });
                   });
        Line(tr("Шрифт"), Style["fontFamily"].toString("Segoe UI"),
             [this](QString V)
             {
                 EditElement(
                     [&](QJsonObject &Item)
                     {
                         auto S = Item["style"].toObject();
                         S["fontFamily"] = V;
                         Item["style"] = S;
                     });
             });
        Select(tr("Начертание"), {{"400", "400"}, {"500", "500"}, {"600", "600"}, {"700", "700"}},
               QString::number(Style["fontWeight"].toInt(400)),
               [this](QString V)
               {
                   EditElement(
                       [&](QJsonObject &Item)
                       {
                           auto S = Item["style"].toObject();
                           S["fontWeight"] = V.toInt();
                           Item["style"] = S;
                       });
               });
        Select(tr("Выравнивание"), {{"left", tr("Слева")}, {"center", tr("По центру")}, {"right", tr("Справа")}},
               Style["textAlign"].toString("left"),
               [this](QString V)
               {
                   EditElement(
                       [&](QJsonObject &Item)
                       {
                           auto S = Item["style"].toObject();
                           S["textAlign"] = V;
                           Item["style"] = S;
                       });
               });
        auto *Order = new QHBoxLayout;
        L->addLayout(Order);
        auto Es = W["elements"].toArray();
        int Index = 0;
        for (int I = 0; I < Es.size(); ++I)
            if (Es[I].toObject()["id"] == ElementId)
                Index = I;
        for (int Direction : {-1, 1})
            Button(Direction < 0 ? tr("Назад") : tr("Вперёд"), Direction < 0 ? "ArrowUp" : "ArrowDown", Form, Order,
                   [this, Direction]()
                   {
                       EditWidget(
                           [&](QJsonObject &Item)
                           {
                               auto Items = Item["elements"].toArray();
                               for (int I = 0; I < Items.size(); ++I)
                                   if (Items[I].toObject()["id"] == ElementId && I + Direction >= 0 &&
                                       I + Direction < Items.size())
                                   {
                                       auto V = Items[I];
                                       Items[I] = Items[I + Direction];
                                       Items[I + Direction] = V;
                                       break;
                                   }
                               Item["elements"] = Items;
                           });
                   })
                ->setEnabled(Index + Direction >= 0 && Index + Direction < Es.size());
        Button(tr("Удалить элемент и детей"), "Trash2", Form, L, [this]() { DeleteElement(); });
    }
    else
    {
        auto *Empty = new QLabel(tr("Выберите элемент на холсте или в списке слева."));
        Empty->setWordWrap(true);
        Empty->setObjectName("WidgetEmptySelection");
        L->addWidget(Empty);
    }
    auto *Css = Button(tr("CSS виджета"), "ChevronRight", Form, L, []() {});
    Css->setCheckable(true);
    Css->setProperty("uiRole", "widgetDisclosure");
    auto *CssNote = new QLabel(
        tr("CSS доступен в веб-прототипе. В нативном редакторе используйте параметры оформления элементов."), Form);
    CssNote->setWordWrap(true);
    L->addWidget(CssNote);
    CssNote->hide();
    connect(Css, &QPushButton::toggled, CssNote,
            [Css, CssNote](bool Open)
            {
                CssNote->setVisible(Open);
                Css->setIcon(MakeFrontendIcon(Open ? "ChevronDown" : "ChevronRight"));
            });
    int Count = 0;
    for (auto V : Document.GetData()["widgets"].toArray())
        if (V.toObject()["kind"] == W["kind"])
            ++Count;
    Button(tr("Удалить виджет"), "Trash2", Form, L, [this]() { DeleteWidget(); })->setEnabled(Count > 1);
    L->addStretch();
    Form->setEnabled(!Locked);
    Properties->setWidget(Form);
    Properties->verticalScrollBar()->setValue(Offset);
}
} // namespace EditorUi
