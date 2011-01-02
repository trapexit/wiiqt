#include "textdialog.h"
#include "ui_textdialog.h"

TextDialog::TextDialog(QWidget *parent, const QString &txt ) :
    QDialog(parent),
    ui(new Ui::TextDialog)
{
    ui->setupUi(this);
    ui->textEdit->append( txt );
}

TextDialog::~TextDialog()
{
    delete ui;
}

void TextDialog::on_buttonBox_accepted()
{
    ret = ui->textEdit->document()->toPlainText();
}

QString TextDialog::GetText( QWidget *parent, const QString &txt )
{
    TextDialog d( parent, txt );
    if( !d.exec() )
        return QString();
    return d.ret;
}
