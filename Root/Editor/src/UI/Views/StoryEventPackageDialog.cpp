#include "UI/Views/StoryEventPackageDialog.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace EditorUi
{
bool StoryEventPackageDialog::Export(QWidget* Parent, StoryEventPackageForm& Form, QString& Path)
{
    QDialog Dialog(Parent);
    Dialog.setWindowTitle(QObject::tr("Экспорт события"));
    Dialog.setObjectName("StoryEventPackageDialog");
    auto* Layout = new QVBoxLayout(&Dialog);
    auto* Fields = new QFormLayout();
    Layout->addLayout(Fields);
    auto* Name = new QLineEdit(Form.Name, &Dialog);
    auto* Description = new QLineEdit(Form.Description, &Dialog);
    auto* Author = new QLineEdit(Form.Author, &Dialog);
    auto* License = new QLineEdit(Form.License, &Dialog);
    Fields->addRow(QObject::tr("Название"), Name);
    Fields->addRow(QObject::tr("Описание"), Description);
    Fields->addRow(QObject::tr("Автор"), Author);
    Fields->addRow(QObject::tr("Условия использования"), License);
    auto* Note = new QLabel(
        QObject::tr("Группы и действия останутся редактируемыми. Объекты сцены станут входами пакета."), &Dialog);
    Note->setWordWrap(true);
    Layout->addWidget(Note);
    auto* Buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &Dialog);
    Layout->addWidget(Buttons);
    auto Validate = [Name, Author, License, Buttons]() {
        Buttons->button(QDialogButtonBox::Save)
            ->setEnabled(!Name->text().trimmed().isEmpty() && !Author->text().trimmed().isEmpty() &&
                         !License->text().trimmed().isEmpty());
    };
    for (auto* Field : {Name, Author, License})
    {
        QObject::connect(Field, &QLineEdit::textChanged, &Dialog, Validate);
    }
    Validate();
    QObject::connect(Buttons, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
    QObject::connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    if (Dialog.exec() != QDialog::Accepted)
    {
        return false;
    }
    Form.Name = Name->text();
    Form.Description = Description->text();
    Form.Author = Author->text();
    Form.License = License->text();
    Path = QFileDialog::getSaveFileName(Parent, QObject::tr("Сохранить пакет события"), {},
                                        QObject::tr("Авторский пакет (*.sacura-package.json)"));
    if (!Path.isEmpty() && !Path.endsWith(".json", Qt::CaseInsensitive))
    {
        Path += ".sacura-package.json";
    }
    return !Path.isEmpty();
}
QString StoryEventPackageDialog::Open(QWidget* Parent)
{
    return QFileDialog::getOpenFileName(Parent, QObject::tr("Импорт события"), {},
                                        QObject::tr("Авторский пакет (*.json)"));
}
bool StoryEventPackageDialog::Import(QWidget* Parent, const StoryEventPackageForm& Form, const StoryGraphModel& Model,
                                     QMap<QString, QString>& Bindings)
{
    QDialog Dialog(Parent);
    Dialog.setWindowTitle(QObject::tr("Импорт события"));
    Dialog.setObjectName("StoryEventPackageDialog");
    auto* Layout = new QVBoxLayout(&Dialog);
    auto* Summary = new QLabel(QObject::tr("%1\nАвтор: %2\nУсловия: %3\n\n%4\n\nГрупп: %5 · действий: %6")
                                   .arg(Form.Name, Form.Author, Form.License, Form.Description)
                                   .arg(Form.Groups)
                                   .arg(Form.Actions),
                               &Dialog);
    Summary->setTextFormat(Qt::PlainText);
    Summary->setWordWrap(true);
    Layout->addWidget(Summary);
    auto* Fields = new QFormLayout();
    Fields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    Layout->addLayout(Fields);
    QMap<QString, QComboBox*> Pickers;
    for (const auto& Input : Form.Inputs)
    {
        auto* Picker = new QComboBox(&Dialog);
        Picker->addItem(QObject::tr("Выберите объект"), QString{});
        for (const auto& Object : Model.Objects)
        {
            Picker->addItem(Object.second, Object.first);
        }
        Picker->setMinimumContentsLength(8);
        Picker->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        Fields->addRow(Input.second, Picker);
        Pickers.insert(Input.first, Picker);
    }
    auto* Note = new QLabel(
        QObject::tr("Будет добавлена независимая копия. Совпадающее имя получит суффикс. Изменение можно отменить."),
        &Dialog);
    Note->setWordWrap(true);
    Layout->addWidget(Note);
    auto* Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
    Layout->addWidget(Buttons);
    Buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Добавить в реплику"));
    auto Validate = [Pickers, Buttons]() {
        bool Valid = true;
        for (auto* Picker : Pickers)
        {
            Valid &= !Picker->currentData().toString().isEmpty();
        }
        Buttons->button(QDialogButtonBox::Ok)->setEnabled(Valid);
    };
    for (auto* Picker : Pickers)
    {
        QObject::connect(Picker, &QComboBox::currentIndexChanged, &Dialog, Validate);
    }
    Validate();
    QObject::connect(Buttons, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
    QObject::connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    if (Dialog.exec() != QDialog::Accepted)
    {
        return false;
    }
    Bindings.clear();
    for (auto It = Pickers.begin(); It != Pickers.end(); ++It)
    {
        Bindings.insert(It.key(), It.value()->currentData().toString());
    }
    return true;
}
} // namespace EditorUi
